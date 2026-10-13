/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2011 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    http://www.d-tacq.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file dsp_atd9802_core.c Analog Threshold Detect DSP module .. multi-site
 * 
 *  Created on: August 18, 2015
 *      Author: pgm
 *
 */

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/types.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mm.h>
//#include <linux/mm_types.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <linux/slab.h>

#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/dma-direction.h>
#include "hbm.h"
#include "acq400.h"

#include "debugfs2.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#include <linux/slab.h>
#include <linux/kdev_t.h>

#include <linux/moduleparam.h>

#include <linux/interrupt.h>
#include <linux/wait.h>

#include "acq400_dsp.h"
#include "dsp_atd_9802.h"

int atd_suppress_mod_event_nsec = 0;
module_param(atd_suppress_mod_event_nsec, int, 0644);
MODULE_PARM_DESC(atd_suppress_mod_event_nsec,
		 "hold off mod_event at least one buffer");

int soft_trigger_nsec = NSEC_PER_MSEC * 10;
module_param(soft_trigger_nsec, int, 0644);
MODULE_PARM_DESC(soft_trigger_nsec, "high hold time for soft trigger pulse");

int active_sites = 6;
module_param(active_sites, int, 0644);
MODULE_PARM_DESC(active_sites, "number of sites in set");

int verbose = 0;
module_param(verbose, int, 0644);

#define REVID "B1002"

ssize_t regfs_event_read(struct file *file, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	int int_count = PD(file)->int_count;
	int rc = wait_event_interruptible(rdev->w_waitq,
					  rdev->ints != int_count);
	if (rc < 0) {
		return -EINTR;
	} else {
		char lbuf[128];
		int nbytes;
		struct EventInfo eventInfo;
		int timeout = 0;
		int delta_ints = rdev->ints - PD(file)->int_count;
		PD(file)->int_count = rdev->ints;
		acq400_init_event_info(&eventInfo);

		nbytes = snprintf(
			lbuf, sizeof(lbuf), "%d %d %d %s 0x%08x %u %u %u %u\n",
			PD(file)->int_count,
			eventInfo.hbm0 ? eventInfo.hbm0->ix : -1,
			eventInfo.hbm1 ? eventInfo.hbm1->ix : -1,
			timeout ? "TO" : "OK", adev->status[0], /* @@TODO */
			adev->sample_count, adev->latch_count,
			adev->sample_count - adev->latch_count, delta_ints);

		if (nbytes > count) {
			nbytes = count;
		}

		rc = copy_to_user(buf, lbuf, nbytes);

		if (rc != 0) {
			return -1;
		} else {
			rdev->client_ready = 1;
			return nbytes;
		}
	}
}

#define ATD_CR 0x4
#define ATD_MOD_EVENT_EN (1 << 5)
/* new 9802 DSP ATD */
#define DSP_ATD_TRG_STATUS 0x70
#define DSP_ATD_TRG_LATCH(n) (0x100 + (n - 1) * 0x10)

void atd_enable_mod_event(struct REGFS_DEV *rdev, int enable)
{
	unsigned cr = ioread32(rdev->va + ATD_CR);
	if (enable) {
		cr |= ATD_MOD_EVENT_EN;
	} else {
		cr &= ~ATD_MOD_EVENT_EN;
	}
	iowrite32(cr, rdev->va + ATD_CR);
}

static enum hrtimer_restart
modevent_masker_timer_handler(struct hrtimer *handle)
{
	struct ATD_9802_DEV *adev =
		container_of(handle, struct ATD_9802_DEV, atd.timer);

	atd_enable_mod_event(&adev->rdev, 1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart soft_trigger_timer_handler(struct hrtimer *handle)
{
	acq400_soft_trigger(0);
	return HRTIMER_NORESTART;
}

static int count_set_bits(unsigned xx)
{
	int count = 0;
	for (; xx; xx >>= 1) {
		if ((xx & 1) != 0) {
			++count;
		}
	}
	return count;
}
static int is_group_trigger(struct ATD_9802_DEV *adev)
{
	unsigned set_bits = 0;
	unsigned is_active = 0;
	unsigned ii;
	int rc;

	for (ii = 0; ii < active_sites; ++ii) {
		unsigned group_trigger_mask = adev->group_trigger_mask[ii];

		if (group_trigger_mask != 0) {
			unsigned active = adev->group_status_latch[ii] &
					  group_trigger_mask;

			if (adev->group_first_n_triggers ==
			    GROUP_FIRST_N_TRIGGERS_ALL) {
				if (active == group_trigger_mask) {
					is_active = 1;

					if (verbose)
						dev_dbg(ATD_DEVP(adev),
							"%s sites:%d/%d"
							" active %08x gtm %08x is_active",
							__FUNCTION__, ii,
							active_sites, active,
							group_trigger_mask);

				} else {
					is_active = 0;
					break;
				}
			} else {
				set_bits += count_set_bits(active);
			}
		}
	}

	rc = is_active ||
	     (set_bits && set_bits >= adev->group_first_n_triggers);
	//[ 4572.610780] acq400_dspfs 80000000.dsp1: is_group_trigger sites:2/2  rc: 0 || (0 && 0 >= 1) FALSE
	dev_dbg(ATD_DEVP(adev),
		"%s sites:%d/%d"
		" rc = %d || (%d && %d >= %d)"
		" %s",
		__FUNCTION__, ii, active_sites, is_active, set_bits, set_bits,
		adev->group_first_n_triggers, rc ? "TRUE" : "FALSE");

	return rc;
}

static irqreturn_t dsp_atd_9802_isr(int irq, void *dev_id)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_id;
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	const int ready = rdev->client_ready;
	u32 irq_stat;
	u32 fun_stat = 0;
	u32 all_stat[ATD_TRG_MAXREG] = {};
	int ix;

	if (ready) {
		adev->sample_count = acq400_agg_sample_count();
	}

	irq_stat = ioread32(rdev->va + ATD_TRG_SITE_ID);

	dev_dbg(ATD_DEVP(adev), "%s ATD_TRG_SITE_ID: %02x", __FUNCTION__,
		irq_stat);
	for (ix = 0; irq_stat && ix < ATD_TRG_MAXREG; ++ix, irq_stat >>= 1) {
		if (irq_stat & 1) {
			unsigned site = ix + 1;
			u32 site_state =
				ioread32(rdev->va + ATD_TRG_LATCH(site));
			iowrite32(site_state,
				  rdev->va + ATD_TRG_LATCH(site)); /* RORA */

			dev_dbg(&rdev->pdev->dev, "%s ATD_TRG_LATCH[%d] 0x%08x",
				__FUNCTION__, ix, site_state);

			all_stat[ix] = site_state;
			adev->status_latch[ix] |= site_state;
			if (adev->gsmode == GS_NOW) {
				adev->group_status_latch[ix] = site_state;
			} else {
				adev->group_status_latch[ix] |= site_state;
			}
		}
	}

	rdev->ints++;
	fun_stat = all_stat[0];

	if (ready) {
		rdev->client_ready = 0;
		memcpy(adev->status, all_stat, ATD_TRG_MAXREG * sizeof(u32));
		adev->latch_count = acq400_adc_latch_count();
		wake_up_interruptible(&rdev->w_waitq);
	}
	if (atd_suppress_mod_event_nsec) {
		atd_enable_mod_event(rdev, 0);
		hrtimer_start(&adev->atd.timer,
			      ktime_set(0, atd_suppress_mod_event_nsec),
			      HRTIMER_MODE_REL);
	}

	if (is_group_trigger(adev)) {
		int ii;
		acq400_soft_trigger(1);

		for (ii = 0; ii < active_sites; ++ii) {
			adev->group_status_latch[0] = 0;
		}
		hrtimer_start(&adev->soft_trigger.timer,
			      ktime_set(0, soft_trigger_nsec),
			      HRTIMER_MODE_REL);
		dev_dbg(ATD_DEVP(adev),
			"%s GROUP_STATUS CONDITION MET: soft trigger",
			__FUNCTION__);
	}

	if (ready) {
		dev_dbg(&rdev->pdev->dev,
			"%s acq400_agg_sample_count %5d sc %08x %s lc %08x diff %d  irq:%08x fun:%08x\n",
			__FUNCTION__, rdev->ints, adev->sample_count,
			adev->sample_count > adev->latch_count ? ">" : "<",
			adev->latch_count,
			adev->sample_count > adev->latch_count ?
				adev->sample_count - adev->latch_count :
				adev->latch_count - adev->sample_count,
			irq_stat, fun_stat);
	}

	return IRQ_HANDLED;
}

static int init_event(struct REGFS_DEV *rdev)
{
	struct resource *ri =
		platform_get_resource(rdev->pdev, IORESOURCE_IRQ, 0);
	int rc = 0;
	if (ri) {
		rc = devm_request_irq(&rdev->pdev->dev, ri->start,
				      dsp_atd_9802_isr, IRQF_NO_THREAD,
				      ri->name, rdev);
		if (rc) {
			dev_err(&rdev->pdev->dev, "unable to get IRQ%d\n",
				ri->start);
		}
		init_waitqueue_head(&rdev->w_waitq);
	}

	regfs_event_fops.read = regfs_event_read;

	return rc;
}

/* split u32 into u16,u16 : convenient for EPICS mbbi */
static int sprintf_split_words(char *buf, unsigned lw)
{
	return sprintf(buf, "%04x,%04x\n", lw >> 16, lw & 0x0000ffff);
}

static ssize_t _show_status_latch(struct device *dev,
				  struct device_attribute *attr, char *buf,
				  const int ix)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	int rc = sprintf_split_words(buf, adev->status_latch[ix]);

	adev->status_latch[ix] = 0;
	return rc;
}

#define STATUS_LATCH(SITE)                                                    \
	static int show_status_latch_##SITE(                                  \
		struct device *dev, struct device_attribute *attr, char *buf) \
	{                                                                     \
		return _show_status_latch(dev, attr, buf, SITE - 1);          \
	}                                                                     \
	static DEVICE_ATTR(status_latch_##SITE, S_IRUGO,                      \
			   show_status_latch_##SITE, 0);

STATUS_LATCH(1);
STATUS_LATCH(2);
STATUS_LATCH(3);
STATUS_LATCH(4);
STATUS_LATCH(5);
STATUS_LATCH(6);

static ssize_t _show_status(struct device *dev, struct device_attribute *attr,
			    char *buf, const int ix)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	return sprintf_split_words(buf, adev->status[ix]);
}

#define STATUS(SITE)                                                          \
	static int show_status_##SITE(                                        \
		struct device *dev, struct device_attribute *attr, char *buf) \
	{                                                                     \
		return _show_status(dev, attr, buf, SITE - 1);                \
	}                                                                     \
	static DEVICE_ATTR(status_##SITE, S_IRUGO, show_status_##SITE, 0);

STATUS(1);
STATUS(2);
STATUS(3);
STATUS(4);
STATUS(5);
STATUS(6);

static ssize_t _show_group_status_latch(struct device *dev,
					struct device_attribute *attr,
					char *buf, const int ix)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	int rc = sprintf_split_words(buf, adev->group_status_latch[ix]);
	return rc;
}

#define GROUP_STATUS_LATCH(SITE)                                              \
	static int show_group_status_latch_##SITE(                            \
		struct device *dev, struct device_attribute *attr, char *buf) \
	{                                                                     \
		return _show_group_status_latch(dev, attr, buf, SITE - 1);    \
	}                                                                     \
	static DEVICE_ATTR(group_status_latch_##SITE, S_IRUGO,                \
			   show_group_status_latch_##SITE, 0);

GROUP_STATUS_LATCH(1);
GROUP_STATUS_LATCH(2);
GROUP_STATUS_LATCH(3);
GROUP_STATUS_LATCH(4);
GROUP_STATUS_LATCH(5);
GROUP_STATUS_LATCH(6);

static ssize_t _store_group_trigger_mask(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count,
					 const int ix)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	unsigned eh, el;

	if (sscanf(buf, "%x,%x", &eh, &el) == 2) {
		adev->group_trigger_mask[ix] = eh << 16 | el;
		return count;
	} else {
		return -1;
	}
}
static ssize_t _show_group_trigger_mask(struct device *dev,
					struct device_attribute *attr,
					char *buf, const int ix)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	int rc = sprintf_split_words(buf, adev->group_trigger_mask[ix]);
	return rc;
}

#define GROUP_TRIGGER_MASK(SITE)                                              \
	static ssize_t store_group_trigger_mask_##SITE(                       \
		struct device *dev, struct device_attribute *attr,            \
		const char *buf, size_t count)                                \
	{                                                                     \
		return _store_group_trigger_mask(dev, attr, buf, count,       \
						 SITE - 1);                   \
	}                                                                     \
	static ssize_t show_group_trigger_mask_##SITE(                        \
		struct device *dev, struct device_attribute *attr, char *buf) \
	{                                                                     \
		return _show_group_trigger_mask(dev, attr, buf, SITE - 1);    \
	}                                                                     \
	static DEVICE_ATTR(group_trigger_mask_##SITE, S_IRUGO | S_IWUSR,      \
			   show_group_trigger_mask_##SITE,                    \
			   store_group_trigger_mask_##SITE);

GROUP_TRIGGER_MASK(1);
GROUP_TRIGGER_MASK(2);
GROUP_TRIGGER_MASK(3);
GROUP_TRIGGER_MASK(4);
GROUP_TRIGGER_MASK(5);
GROUP_TRIGGER_MASK(6);

static ssize_t store_group_first_n_triggers(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);

	if (sscanf(buf, "%u", &adev->group_first_n_triggers) == 1) {
		return count;
	} else {
		return -1;
	}
}
static ssize_t show_group_first_n_triggers(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);

	int rc = sprintf(buf, "%d\n", adev->group_first_n_triggers);
	return rc;
}

static DEVICE_ATTR(group_first_n_triggers, S_IRUGO | S_IWUSR,
		   show_group_first_n_triggers, store_group_first_n_triggers);

static ssize_t store_group_status_mode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);
	unsigned en;

	if (sscanf(buf, "%d", &en) == 1) {
		adev->gsmode = en == 1;
		return count;
	} else {
		return -1;
	}
}
static ssize_t show_group_status_mode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct REGFS_DEV *rdev = (struct REGFS_DEV *)dev_get_drvdata(dev);
	struct ATD_9802_DEV *adev =
		container_of(rdev, struct ATD_9802_DEV, rdev);

	int rc = sprintf(buf, "%d %s\n", adev->gsmode,
			 adev->gsmode == GS_NOW ? "GS_NOW" : "GS_HISTORIC");
	return rc;
}

static DEVICE_ATTR(group_status_mode, S_IRUGO | S_IWUSR, show_group_status_mode,
		   store_group_status_mode);

static const struct attribute *sysfs_base_attrs[] = {
	&dev_attr_group_status_mode.attr,
	&dev_attr_group_first_n_triggers.attr,

	&dev_attr_status_1.attr,
	&dev_attr_status_2.attr,
	&dev_attr_status_3.attr,
	&dev_attr_status_4.attr,
	&dev_attr_status_5.attr,
	&dev_attr_status_6.attr,

	&dev_attr_status_latch_1.attr,
	&dev_attr_status_latch_2.attr,
	&dev_attr_status_latch_3.attr,
	&dev_attr_status_latch_4.attr,
	&dev_attr_status_latch_5.attr,
	&dev_attr_status_latch_6.attr,

	&dev_attr_group_status_latch_1.attr,
	&dev_attr_group_status_latch_2.attr,
	&dev_attr_group_status_latch_3.attr,
	&dev_attr_group_status_latch_4.attr,
	&dev_attr_group_status_latch_5.attr,
	&dev_attr_group_status_latch_6.attr,

	&dev_attr_group_trigger_mask_1.attr,
	&dev_attr_group_trigger_mask_2.attr,
	&dev_attr_group_trigger_mask_3.attr,
	&dev_attr_group_trigger_mask_4.attr,
	&dev_attr_group_trigger_mask_5.attr,
	&dev_attr_group_trigger_mask_6.attr,

	NULL
};

int dsp_atd_9802_probe(struct platform_device *pdev)
{
	struct ATD_9802_DEV *adev =
		kzalloc(sizeof(struct ATD_9802_DEV), GFP_KERNEL);
	struct REGFS_DEV *rdev = &adev->rdev;

	int rc = acq400dsp_devicetree_init(pdev, pdev->dev.of_node);
	if (rc != 0) {
		dev_err(&pdev->dev, "acq400dsp_devicetree_init() failed");
		goto fail;
	}

	dev_info(&pdev->dev, "%s id:%d num_resources:%d %s", __FUNCTION__,
		 pdev->id, pdev->num_resources, rc == 0 ? "OK" : "dts error");
	rdev->pdev = pdev;

	rc = regfs_probe_rdev(pdev, rdev);
	if (rc != 0) {
		dev_err(&pdev->dev, "regfs_probe_rdev() failed");
		goto fail;
	}

	init_event(rdev);
	if (sysfs_create_files(&pdev->dev.kobj, sysfs_base_attrs)) {
		dev_err(&pdev->dev, "failed to create sysfs");
	}
	acq400_timer_init(&adev->atd.timer, modevent_masker_timer_handler);
	acq400_timer_init(&adev->soft_trigger.timer,
			  soft_trigger_timer_handler);
fail:
	return rc;
}

int dsp_atd_9802_remove(struct platform_device *pdev)
{
	struct REGFS_DEV *rdev =
		(struct REGFS_DEV *)dev_get_drvdata(&pdev->dev);
	regfs_remove(pdev);
	// cdev_delete() ?
	kfree(rdev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id acq400dsp_of_match[] /* __devinitdata */ = {
	{ .compatible = "D-TACQ,acq400dsp" },
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, acq400dsp_of_match);
#else
#define acq400dsp_of_match NULL
#endif /* CONFIG_OF */

#define MODULE_NAME "acq400_dspfs"

static struct platform_driver acq400dsp_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = acq400dsp_of_match,
        },
        .probe = dsp_atd_9802_probe,
        .remove = dsp_atd_9802_remove,
};

static int __init acq400_dsp_init(void)
{
	int status;

	printk("D-TACQ ACQ400 ATD_9802 DSP Driver %s\n", REVID);

	status = platform_driver_register(&acq400dsp_driver);
	return status;
}

static void __exit acq400_dsp_exit(void)
{
}

module_init(acq400_dsp_init);
module_exit(acq400_dsp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ dsp_atd_9802 driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
