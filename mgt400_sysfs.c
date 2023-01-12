/* ------------------------------------------------------------------------- */
/* acq400_drv.c  D-TACQ ACQ400 FMC  DRIVER   
 *
 * mgt400_sysfs.c
 *
 *  Created on: 13 Jan 2015
 *      Author: pgm
 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2015 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include "acq400.h"
#include "mgt400.h"
#include "mgt400_sysfs.h"
#include "acq400_sysfs.h"


static ssize_t show_module_type(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%X\n", mdev->mod_id>>MOD_ID_TYPE_SHL);
}


static DEVICE_ATTR(module_type, S_IRUGO, show_module_type, 0);

static const char* _lookup_id(unsigned mod_id)
{
	switch(mod_id){
	case MOD_ID_MGT_DRAM:
		return "mgtdram";
	default:
		return "mgt482";
	}
}

static ssize_t show_module_name(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];

	return sprintf(buf, "%s\n", _lookup_id(GET_MOD_ID(mdev)));
}


static DEVICE_ATTR(module_name, S_IRUGO, show_module_name, 0);

static ssize_t store_RW32_debug(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct mgt400_dev *adev = mgt400_devices[dev->id];
	int RW32_debug;

	if (sscanf(buf, "%d", &RW32_debug) == 1){
		adev->RW32_debug = RW32_debug;
		return count;
	}else{
		return -1;
	}
}

static ssize_t show_RW32_debug(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *adev = mgt400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->RW32_debug);
}

static DEVICE_ATTR(RW32_debug,
		S_IRUGO|S_IWUSR, show_RW32_debug, store_RW32_debug);

#define DMA_STATUS(DIR, REG, SHL) 					\
static ssize_t show_dma_stat_##DIR(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	struct mgt400_dev *mdev = mgt400_devices[dev->id];		\
	u32 sta = mgt400rd32(mdev, REG) >> SHL;				\
	u32 count = (sta&DMA_DATA_FIFO_COUNT) >> DMA_DATA_FIFO_COUNT_SHL; \
	u32 flags = sta&DMA_DATA_FIFO_FLAGS;				\
	return sprintf(buf, "%d %d \n", count, flags);			\
}									\
									\
static DEVICE_ATTR(dma_stat_##DIR, S_IRUGO, show_dma_stat_##DIR, 0)	\

DMA_STATUS(desc_pull, DESC_FIFO_SR, DMA_DATA_PULL_SHL);
DMA_STATUS(desc_push, DESC_FIFO_SR, DMA_DATA_PUSH_SHL);
DMA_STATUS(data_pull, DMA_FIFO_SR,  DMA_DATA_PULL_SHL);
DMA_STATUS(data_push, DMA_FIFO_SR,  DMA_DATA_PUSH_SHL);

static ssize_t show_pull_buffer_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%lu\n", mdev->pull.buffer_count);
}

static DEVICE_ATTR(pull_buffer_count, S_IRUGO, show_pull_buffer_count, 0);

static ssize_t show_push_buffer_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%lu\n", mdev->push.buffer_count);
}

static DEVICE_ATTR(push_buffer_count, S_IRUGO, show_push_buffer_count, 0);

/* mask to 31 bits because EPICS has only signed data types.
 * it's OK to lose dynamic range here, because there is PLENTY of range
 * and then EPICS will expand to double anyway..
 */
static ssize_t show_pull_buffer_count_lw(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n",
		mgt400rd32(mdev, DMA_PULL_COUNT_LW)&0x7FFFFFFF);
}

static DEVICE_ATTR(pull_buffer_count_lw, S_IRUGO, show_pull_buffer_count_lw, 0);

static ssize_t show_push_buffer_count_lw(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n",
		mgt400rd32(mdev, DMA_PUSH_COUNT_LW)&0x7FFFFFFF);
}

static DEVICE_ATTR(push_buffer_count_lw, S_IRUGO, show_push_buffer_count_lw, 0);


static ssize_t show_enable(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n", mgt400rd32(mdev, ZDMA_CR)&ZDMA_CR_ENABLE);

}
static DEVICE_ATTR(enable, S_IRUGO, show_enable, 0);

static ssize_t store_aurora_enable(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf, size_t count)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	int enable;

	if (sscanf(buf, "%d", &enable) == 1){
		u32 cr = mgt400rd32(mdev, AURORA_CR);
		if (enable){
			cr |= AURORA_CR_ENA;
		}else{
			cr &= ~AURORA_CR_ENA;
		}
		mgt400wr32(mdev, AURORA_CR, cr);
		return strlen(buf);
	}else{
		return -1;
	}
}
static ssize_t show_aurora_enable(
		struct device * dev,
		struct device_attribute *attr,
		char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n", (mgt400rd32(mdev, AURORA_CR)&AURORA_CR_ENA)!=0);
}
static DEVICE_ATTR(aurora_enable, S_IRUGO|S_IWUSR, show_aurora_enable, store_aurora_enable);


static ssize_t show_aurora_lane_up(
		struct device * dev,
		struct device_attribute *attr,
		char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n", AURORA_SR_UP(mgt400rd32(mdev, AURORA_SR)));
}
static DEVICE_ATTR(aurora_lane_up, S_IRUGO, show_aurora_lane_up, 0);

static ssize_t show_aurora_errors(
		struct device * dev,
		struct device_attribute *attr,
		char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	unsigned errs = AURORA_SR_ERR(mgt400rd32(mdev, AURORA_SR));
	if (errs){
		u32 cr = mgt400rd32(mdev, AURORA_CR);
		mgt400wr32(mdev, AURORA_CR, cr | AURORA_CR_CLR);
		mgt400wr32(mdev, AURORA_CR, cr);
	}
	return sprintf(buf, "%u\n", errs);
}
static DEVICE_ATTR(aurora_errors, S_IRUGO, show_aurora_errors, 0);

static ssize_t show_heartbeat(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
#if 1
	/* EPICS DS can't handle a full u32 */
	return sprintf(buf, "%u\n", mgt400rd32(mdev, HEART) >> 1);
#else
	return sprintf(buf, "%u\n",
                       mdev->reg_cache.data[HEART/sizeof(int)] >> 1);
#endif
}
static DEVICE_ATTR(heartbeat, S_IRUGO, show_heartbeat, 0);


static ssize_t show_astats1(
		struct device * dev,
		struct device_attribute *attr,
		char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 astats = mgt400rd32(mdev, ASTATS1);
	return sprintf(buf, "%08x %3u %3u %3u %3u\n",
			astats, astats>>24, (astats>>16)&0x0ff,
			(astats>>8)&0x0ff, astats&0x0ff);
}

static DEVICE_ATTR(astats1, S_IRUGO, show_astats1, 0);


static ssize_t show_astats(
		struct device * dev,
		struct device_attribute *attr,
		char * buf, int reg)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 astats = mgt400rd32(mdev, reg);
	return sprintf(buf, "%08x %u %u\n",
				astats, (astats>>16)&0x0ffff, astats&0x0ffff);
}

#define SHOW_ASTATS(name, reg) 						\
static ssize_t show_##name(						\
	struct device * dev, struct device_attribute *attr, char * buf) \
{									\
	return show_astats(dev, attr, buf, reg);			\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_##name, 0)

SHOW_ASTATS(astats2, ASTATS2);
SHOW_ASTATS(alat_avg, ALAT_AVG);
SHOW_ASTATS(alat_min_max, ALAT_MIN_MAX);

static ssize_t show_name(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%s\n", mdev->devname);
}
static DEVICE_ATTR(name, S_IRUGO, show_name, 0);


static ssize_t show_site(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n", mdev->of_prams.site);
}
static DEVICE_ATTR(site, S_IRUGO, show_site, 0);

static ssize_t show_dev(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%u\n", MAJOR(mdev->cdev.dev));
}
static DEVICE_ATTR(dev, S_IRUGO, show_dev, 0);

static ssize_t show_clear_stats(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return sprintf(buf, "%u\n", 0);
}

static ssize_t store_clear_stats(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf, size_t count)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	int clear;

	if (sscanf(buf, "%d", &clear) && clear == 1){
		mgt400_clear_counters(mdev);
		return strlen(buf);
	}else{
		return -1;
	}
}
static DEVICE_ATTR(clear_stats, S_IRUGO|S_IWUSR, show_clear_stats, store_clear_stats);

static ssize_t show_spad(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
	return sprintf(buf, "%u\n", (zdma_cr&AGG_SPAD_EN) != 0);
}

static ssize_t store_spad(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	unsigned enable;
	if (sscanf(buf, "%u", &enable) > 0){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
		if (enable){
			zdma_cr |= AGG_SPAD_EN;
		}else{
			zdma_cr &= ~AGG_SPAD_EN;
		}
		mgt400wr32(mdev, ZDMA_CR, zdma_cr);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(spad, S_IRUGO|S_IWUSR, show_spad, store_spad);

static ssize_t show_auto_dma(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
	return sprintf(buf, "%u\n", (zdma_cr&ZDMA_CR_AUTO_PUSH_DMA) != 0);
}

static ssize_t store_auto_dma(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{

	unsigned enable;

	if (sscanf(buf, "%u", &enable) > 0){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
		if (enable){
			zdma_cr |= ZDMA_CR_AUTO_PUSH_DMA;
		}else{
			zdma_cr &= ~ZDMA_CR_AUTO_PUSH_DMA;
		}
		mgt400wr32(mdev, ZDMA_CR, zdma_cr);
	}else{
		count = -1;
	}

	return count;
}

static DEVICE_ATTR(auto_dma, S_IRUGO|S_IWUSR, show_auto_dma, store_auto_dma);

static ssize_t store_kill_comms(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{

	unsigned enable;

	if (sscanf(buf, "%u", &enable) > 0){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
		zdma_cr |= ZDMA_CR_KILL_COMMS;
		mgt400wr32(mdev, ZDMA_CR, zdma_cr);
		zdma_cr &= ~ZDMA_CR_KILL_COMMS;
		mgt400wr32(mdev, ZDMA_CR, zdma_cr);
	}else{
		count = -1;
	}

	return count;
}

static DEVICE_ATTR(kill_comms, S_IRUGO|S_IWUSR, 0, store_kill_comms);



#define AGG_SEL	"aggregator="

static ssize_t show_agg_reg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const unsigned offset,
	const unsigned mshift)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 regval = mgt400rd32(mdev, offset);
	char mod_group[80];
	char spad_buf[8];
	int site;

	for (site = 1, mod_group[0] = '\0'; site <= 6; ++site){
		if ((regval & AGG_MOD_EN(site, mshift)) != 0){
			if (strlen(mod_group) == 0){
				strcat(mod_group, "sites=");
			}else{
				strcat(mod_group, ",");
			}

			sprintf(mod_group+strlen(mod_group), "%d", site);
		}
	}
	if (strlen(mod_group) == 0){
		sprintf(mod_group, "sites=none");
	}

	show_spad(dev, attr, spad_buf);
	return sprintf(buf, "reg=0x%08x %s DATA_MOVER_EN=%s spad=%s",
			regval, mod_group, regval&DATA_MOVER_EN? "on": "off", spad_buf);
}

extern int good_sites[];
extern int good_sites_count;


int get_site(char s)
{
	struct acq400_sc_dev* sc_dev = container_of(acq400_devices[0], struct acq400_sc_dev, adev);
	int ii;
	int site = s-'0';

	for(ii = 0; ii < MAXDEVICES; ++ii){
		if (sc_dev->aggregator_set[ii] &&
		    sc_dev->aggregator_set[ii]->of_prams.site == site){
			return site;
		}
	}

	return -1;
}


static ssize_t store_agg_reg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const unsigned offset,
	const unsigned mshift)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	unsigned regval = mgt400rd32(mdev, offset);
	char* lbuf = kmalloc(strlen(buf)+1, GFP_KERNEL);
	char* crsr = lbuf;
	int pass = 0;
	char* tok;

	strcpy(lbuf, buf);

	dev_dbg(DEVP(mdev), "store_agg_reg \"%s\"", buf);

	while((tok = strsep(&crsr, " ")) != NULL){
		char* match;

		if ((match = strstr(tok, "sites=")) != 0){
			int site;
			char* cursor = match+strlen("sites=");

			regval &= ~(AGG_SITES_MASK << mshift);

			if (strncmp(cursor, "none", 4) != 0){
				for (; *cursor && *cursor != ' '; ++cursor){
					switch(*cursor){
					case ',':
					case ' ':	continue;
					case '\n':  	break;
					default:
						site = get_site(*cursor);
						if (site > 0){
							regval |= AGG_MOD_EN(site, mshift);
							break;
						}else{
							dev_err(dev, "bad site designator: %c", *cursor);
							count = -1;
							goto store99;
						}
					}
				}
			}

			pass = 1;
		}else if ((match = strstr(tok, "on")) != 0){
			dev_dbg(DEVP(mdev), "%d store_agg_reg %08x \"%s\"", __LINE__, regval, buf);
			regval |= DATA_MOVER_EN;
			pass = 1;
		}else if ((match = strstr(tok, "off")) != 0){
			dev_dbg(DEVP(mdev), "%d store_agg_reg %08x \"%s\"", __LINE__, regval, buf);
			regval &= ~DATA_MOVER_EN;
			pass = 1;
		}else if ((match = strstr(tok, "spad=")) != 0){
			if ((match+strlen("spad="))[0] == '1'){
				regval |= AGG_SPAD_EN;
			}else{
				regval &= ~AGG_SPAD_EN;
			}
			pass = 1;
		}
	}

	if (!pass && sscanf(buf, "%x", &regval) != 1){
		count = -1;
		goto store99;
	}
	dev_dbg(DEVP(mdev), "%d store_agg_reg %08x \"%s\"", __LINE__, regval, buf);
	mgt400wr32(mdev, offset, regval);

store99:
	kfree(lbuf);
	return count;
}


#define REG_KNOB(name, offset, mshift)					\
static ssize_t show_reg_##name(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_agg_reg(dev, attr, buf, offset, mshift);		\
}									\
static ssize_t store_reg_##name(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_agg_reg(dev, attr, buf, count,  offset, mshift);	\
}									\
static DEVICE_ATTR(name, 						\
	S_IRUGO|S_IWUSR, show_reg_##name, store_reg_##name)

REG_KNOB(aggregator, ZDMA_CR,	AGGREGATOR_MSHIFT);


static ssize_t show_ident(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "0x%08x\n", mgt400rd32(mdev, ZIDENT));
}

static ssize_t store_ident(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf, size_t count)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	unsigned ident;

	if (sscanf(buf, "0x%x", &ident) || sscanf(buf, "%d", &ident)){
		mgt400wr32(mdev, ZIDENT, ident);
		return strlen(buf);
	}else{
		return -1;
	}
}
static DEVICE_ATTR(ident, S_IRUGO|S_IWUSR, show_ident, store_ident);


static ssize_t show_pull_status(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%d\n", mdev->dma_enable_status[ID_PULL].status);
}
static DEVICE_ATTR(pull_status, S_IRUGO, show_pull_status, 0);

static ssize_t show_push_status(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	return sprintf(buf, "%d\n", mdev->dma_enable_status[ID_PUSH].status);
}
static DEVICE_ATTR(push_status, S_IRUGO, show_push_status, 0);

static ssize_t show_decim(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
	return sprintf(buf, "%u\n", ((zdma_cr>>AGG_DECIM_SHL)&AGG_DECIM_MASK)+1);
}

static ssize_t store_decim(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	unsigned decimate;

	if (sscanf(buf, "%u", &decimate) == 1 && decimate){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 zdma_cr = mgt400rd32(mdev, ZDMA_CR);
		zdma_cr &= ~(AGG_DECIM_MASK<<AGG_DECIM_SHL);

		if (--decimate > AGG_DECIM_MASK) decimate = AGG_DECIM_MASK;
		zdma_cr |= (decimate&AGG_DECIM_MASK) << AGG_DECIM_SHL;
		mgt400wr32(mdev, ZDMA_CR, zdma_cr);

		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(decimate, S_IRUGO|S_IWUSR, show_decim, store_decim);

static const struct attribute *sysfs_base_attrs[] = {
	&dev_attr_module_type.attr,
	&dev_attr_module_name.attr,
	&dev_attr_enable.attr,
	&dev_attr_name.attr,
	&dev_attr_site.attr,
	&dev_attr_dev.attr,
	NULL
};

static const struct attribute *sysfs_aurora_attrs[] = {
	&dev_attr_aurora_enable.attr,
	&dev_attr_aurora_lane_up.attr,
	&dev_attr_aurora_errors.attr,
	&dev_attr_astats1.attr,
	&dev_attr_astats2.attr,
	&dev_attr_alat_avg.attr,
	&dev_attr_alat_min_max.attr,
	&dev_attr_heartbeat.attr,
	&dev_attr_dma_stat_desc_pull.attr,
	&dev_attr_dma_stat_desc_push.attr,
	&dev_attr_dma_stat_data_pull.attr,
	&dev_attr_dma_stat_data_push.attr,
	&dev_attr_push_buffer_count.attr,
	&dev_attr_pull_buffer_count.attr,
	&dev_attr_push_buffer_count_lw.attr,
	&dev_attr_pull_buffer_count_lw.attr,
	&dev_attr_push_status.attr,
	&dev_attr_pull_status.attr,
	&dev_attr_clear_stats.attr,
	&dev_attr_aggregator.attr,
	&dev_attr_spad.attr,
	&dev_attr_auto_dma.attr,
	&dev_attr_kill_comms.attr,
	&dev_attr_ident.attr,
	&dev_attr_RW32_debug.attr,
	&dev_attr_decimate.attr,
	NULL
};

static ssize_t show_fpga_temp(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	/* EPICS DS can't handle a full u32 */
	return sprintf(buf, "%u\n", mgt400rd32(mdev, MGT_DRAM_STA) >> 20);
}
static DEVICE_ATTR(fpga_temp, S_IRUGO, show_fpga_temp, 0);

static ssize_t show_fpga_rev(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	unsigned rev = (mgt400rd32(mdev, MGT_DRAM_STA) >> 4)&0x0ffff;

	return sprintf(buf, "%u 0x%04x\n", rev, rev);
}
static DEVICE_ATTR(fpga_rev, S_IRUGO, show_fpga_rev, 0);


static const struct attribute *sysfs_mgtdram_attrs[] = {
	&dev_attr_fpga_temp.attr,
	&dev_attr_fpga_rev.attr,
	NULL
};

#define MAC_TOP3 0x002154

static ssize_t show_coloned_hex(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 mac_bot4 = mgt400rd32(mdev, HUDP_MAC);
	u8 hexb[6];

	hexb[0] = (MAC_TOP3>>16) & 0xff;
	hexb[1] = (MAC_TOP3>>8)  & 0xff;
	hexb[2] = (MAC_TOP3)     & 0xff;
	hexb[3] = (mac_bot4>>16) & 0xff;
	hexb[4] = (mac_bot4>>8)  & 0xff;
	hexb[5] = (mac_bot4)     & 0xff;

	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			hexb[0], hexb[1], hexb[2], hexb[3], hexb[4], hexb[5]);
}

static ssize_t store_coloned_hex(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 hexb[6];

	if (sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
			hexb+0, hexb+1, hexb+2, hexb+3, hexb+4, hexb+5) == 6 ){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 mac32 = 0;
		int ii;
		int lsl = 24;
		for (ii = 2; ii < 6; ++ii, lsl-= 8){
			mac32 |= hexb[ii] << lsl;
		}
		mgt400wr32(mdev, HUDP_MAC, mac32);

		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(mac, S_IRUGO|S_IWUSR, show_coloned_hex, store_coloned_hex);

static ssize_t show_dotted_quad(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int REG)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 ipaddr = mgt400rd32(mdev, REG);
	u8 decb[4];

	decb[0] = (ipaddr>>24) & 0xff;
	decb[1] = (ipaddr>>16) & 0xff;
	decb[2] = (ipaddr>>8)  & 0xff;
	decb[3] = (ipaddr)     & 0xff;

	return sprintf(buf, "%d.%d.%d.%d\n",
			decb[0], decb[1], decb[2], decb[3]);
}

static ssize_t store_dotted_quad(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int REG)
{
	u32 decb[4];

	if (sscanf(buf, "%d.%d.%d.%d", decb+0, decb+1, decb+2, decb+3) == 4 ){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 ipaddr = 0;
		int ii;
		int lsl = 24;
		for (ii = 0; ii < 4; ++ii, lsl-= 8){
			ipaddr |= decb[ii] << lsl;
		}
		mgt400wr32(mdev, REG, ipaddr);

		return count;
	}else{
		return -1;
	}
}

#define MAKE_DOTTED_QUAD(NAME, REG) 				\
static ssize_t show_dotted_quad##NAME(				\
	struct device * dev,					\
	struct device_attribute *attr,				\
	char * buf)						\
{								\
	return show_dotted_quad(dev, attr, buf, REG);		\
}								\
								\
static ssize_t store_dotted_quad##NAME(				\
	struct device * dev,					\
	struct device_attribute *attr,				\
	const char * buf,					\
	size_t count)						\
{								\
	return store_dotted_quad(dev, attr, buf, count, REG);	\
}								\
static DEVICE_ATTR(NAME, S_IRUGO|S_IWUSR, show_dotted_quad##NAME, store_dotted_quad##NAME)



MAKE_DOTTED_QUAD(ip, HUDP_IP_ADDR);
MAKE_DOTTED_QUAD(gw, HUDP_GW_ADDR);
MAKE_DOTTED_QUAD(netmask, HUDP_NETMASK);
MAKE_DOTTED_QUAD(dst_ip, HUDP_DEST_ADDR);
MAKE_DOTTED_QUAD(rx_src_ip, HUDP_RX_SRC_ADDR);




MAKE_DNUM(src_port, HUDP_SRC_PORT,  0xffff);
MAKE_DNUM(dst_port, HUDP_DEST_PORT, 0xffff);
MAKE_DNUM(rx_port,  HUDP_RX_PORT,   0xffff);
MAKE_DNUM(tx_pkt_ns, 	HUDP_TX_PKT_SZ, 0x7800);
MAKE_DNUM(tx_sample_sz, HUDP_TX_PKT_SZ, 0x03ff);


#define TX_SPP_MSK 0x7800
#define TX_SPP_SHL 11

static ssize_t show_tx_spp(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 tx_spp = (mgt400rd32(mdev, HUDP_TX_PKT_SZ)&TX_SPP_MSK) >> TX_SPP_SHL;
	return sprintf(buf, "%u\n", tx_spp+1);
}

static ssize_t store_tx_spp(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	unsigned tx_spp;

	if (sscanf(buf, "%u", &tx_spp) == 1){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 pkt_sz = mgt400rd32(mdev, HUDP_TX_PKT_SZ);
		pkt_sz &= ~TX_SPP_MSK;

		if (tx_spp > 0) tx_spp -= 1;   // hw guys count from zero, but defend against entry 0

		pkt_sz |= ((tx_spp<<TX_SPP_SHL)&TX_SPP_MSK);

		mgt400wr32(mdev, HUDP_TX_PKT_SZ, pkt_sz);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(tx_spp, S_IRUGO|S_IWUSR, show_tx_spp, store_tx_spp);


static ssize_t show_hudp_decim(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 hudp_con = mgt400rd32(mdev, HUDP_CON);
	return sprintf(buf, "%u\n", ((hudp_con>>AGG_DECIM_SHL)&AGG_DECIM_MASK)+1);
}

static ssize_t store_hudp_decim(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	unsigned decimate;

	if (sscanf(buf, "%u", &decimate) == 1 && decimate){
		struct mgt400_dev *mdev = mgt400_devices[dev->id];
		u32 hudp_con = mgt400rd32(mdev, ZDMA_CR);
		hudp_con &= ~(AGG_DECIM_MASK<<AGG_DECIM_SHL);

		if (--decimate > AGG_DECIM_MASK) decimate = AGG_DECIM_MASK;
		hudp_con |= (decimate&AGG_DECIM_MASK) << AGG_DECIM_SHL;
		mgt400wr32(mdev, HUDP_CON, hudp_con);

		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(hudp_decim, S_IRUGO|S_IWUSR, show_hudp_decim, store_hudp_decim);

MAKE_DNUM(tx_pkt_count, HUDP_TX_PKT_COUNT, 0xffffffff);
MAKE_DNUM(rx_pkt_count, HUDP_RX_PKT_COUNT, 0xffffffff);
MAKE_DNUM(rx_pkt_len,   HUDP_RX_PKT_LEN,   0x000003ff);

MAKE_BITS(ctrl, 	HUDP_CON, 0, 0xffffffff);
MAKE_BITS(tx_ctrl, 	HUDP_CON, MAKE_BITS_FROM_MASK, 0x0000000f);
MAKE_BITS(rx_en,        HUDP_CON, MAKE_BITS_FROM_MASK, (1<<(4+8)));
MAKE_BITS(rx_reset, 	HUDP_CON, MAKE_BITS_FROM_MASK, (1<<(3+8)));

MAKE_BITS(udp_data_src, HUDP_CON, MAKE_BITS_FROM_MASK, (1<<5));
MAKE_BITS(tx_en,        HUDP_CON, MAKE_BITS_FROM_MASK, (1<<4));
MAKE_BITS(tx_reset, 	HUDP_CON, MAKE_BITS_FROM_MASK, (1<<3));

MAKE_BITS(disco_en, 	HUDP_DISCO_COUNT, MAKE_BITS_FROM_MASK, HUDP_DISCO_EN);
MAKE_DNUM(disco_idx, 	HUDP_DISCO_COUNT, HUDP_DISCO_INDEX);
MAKE_DNUM(disco_count, 	HUDP_DISCO_COUNT, HUDP_DISCO_COUNT_COUNT);
MAKE_BITS(hudp_status,       HUDP_STATUS, MAKE_BITS_FROM_MASK, 0xffffffff);

MAKE_DNUM(tx_calc_pkt_sz, HUDP_CALC_PKT_SZ, 0xffffffff);
MAKE_DNUM(slice_len,      UDP_SLICE, 0x0000ff00);
MAKE_DNUM(slice_off,      UDP_SLICE, 0x000000ff);


#define LOWBYTE(reg, shl)  (((reg)>>(shl))&0x00ff)

static ssize_t show_arp_mac_resp(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];
	u32 upper = mgt400rd32(mdev, ARP_RESP_MAC_UPPER);
	u32 lower = mgt400rd32(mdev, ARP_RESP_MAC_LOWER);

	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			LOWBYTE(upper,  8),  LOWBYTE(upper,  0),
			LOWBYTE(lower, 24),  LOWBYTE(lower, 16),
			LOWBYTE(lower,  8),  LOWBYTE(lower,  0) );
}

static DEVICE_ATTR(arp_mac_resp, S_IRUGO, show_arp_mac_resp, 0);

static const struct attribute *sysfs_hudp_attrs[] = {
	&dev_attr_mac.attr,
	&dev_attr_ip.attr,
	&dev_attr_gw.attr,
	&dev_attr_netmask.attr,
	&dev_attr_dst_ip.attr,
	&dev_attr_src_port.attr,
	&dev_attr_dst_port.attr,
	&dev_attr_rx_port.attr,
	&dev_attr_rx_src_ip.attr,
	&dev_attr_tx_pkt_ns.attr,
	&dev_attr_tx_spp.attr,
	&dev_attr_tx_sample_sz.attr,
	&dev_attr_tx_calc_pkt_sz.attr,
	&dev_attr_ctrl.attr,
	&dev_attr_tx_reset.attr,
	&dev_attr_rx_reset.attr,
	&dev_attr_hudp_decim.attr,
	&dev_attr_tx_en.attr,
	&dev_attr_rx_en.attr,
	&dev_attr_tx_ctrl.attr,
	&dev_attr_udp_data_src.attr,

	&dev_attr_tx_pkt_count.attr,
	&dev_attr_rx_pkt_count.attr,
	&dev_attr_rx_pkt_len.attr,

	&dev_attr_disco_en.attr,
	&dev_attr_disco_idx.attr,
	&dev_attr_disco_count.attr,

	&dev_attr_clear_stats.attr,
	&dev_attr_hudp_status.attr,

	&dev_attr_arp_mac_resp.attr,
	&dev_attr_slice_len.attr,
	&dev_attr_slice_off.attr,
	NULL
};
void mgt400_createSysfs(struct device *dev)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];

	dev_info(dev, "mgt400_createSysfs()");
	if (sysfs_create_files(&dev->kobj, sysfs_base_attrs)){
		dev_err(dev, "failed to create sysfs");
		return;
	}
	if (IS_MGT_HUDP(mdev)){
		dev_info(dev, "MGT_HUDP");
		if (sysfs_create_files(&dev->kobj, sysfs_hudp_attrs)){
			dev_err(dev, "failed to create sysfs_hudp_attrs");
		}
	}else{
		if (sysfs_create_files(&dev->kobj, sysfs_aurora_attrs)){
			dev_err(dev, "failed to create sysfs");
		}else if (IS_MGT_DRAM(mdev)){
			if (sysfs_create_files(&dev->kobj, sysfs_mgtdram_attrs)){
				dev_err(dev, "failed to create sysfs_mgtdram_attrs");
			}
		}
	}
}
