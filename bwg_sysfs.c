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

	return sprintf(buf, "reg=0x%08x %s DATA_MOVER_EN=%s\n",
			regval, mod_group, regval&DATA_MOVER_EN? "on": "off");
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
	char* match;
	int pass = 0;
	unsigned regval = mgt400rd32(mdev, offset);

	dev_dbg(DEVP(mdev), "store_agg_reg \"%s\"", buf);

	if ((match = strstr(buf, "sites=")) != 0){
		char* cursor = match+strlen("sites=");
		int site;

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
						return -1;
					}
				}
			}
		}
		pass = 1;
	}
	if ((match = strstr(buf, "on")) != 0){
		regval |= DATA_MOVER_EN;
		pass = 1;
	}else if ((match = strstr(buf, "off")) != 0){
		regval &= ~DATA_MOVER_EN;
		pass = 1;
	}

	if (!pass && sscanf(buf, "%x", &regval) != 1){
		return -1;
	}

	mgt400wr32(mdev, offset, regval);
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


static const struct attribute *sysfs_base_attrs[] = {
	&dev_attr_module_type.attr,
	&dev_attr_module_name.attr,
	&dev_attr_enable.attr,
	&dev_attr_aurora_enable.attr,
	&dev_attr_aurora_lane_up.attr,
	&dev_attr_aurora_errors.attr,
	&dev_attr_astats1.attr,
	&dev_attr_astats2.attr,
	&dev_attr_alat_avg.attr,
	&dev_attr_alat_min_max.attr,
	&dev_attr_heartbeat.attr,
	&dev_attr_name.attr,
	&dev_attr_site.attr,
	&dev_attr_dev.attr,
	&dev_attr_dma_stat_desc_pull.attr,
	&dev_attr_dma_stat_desc_push.attr,
	&dev_attr_dma_stat_data_pull.attr,
	&dev_attr_dma_stat_data_push.attr,
	&dev_attr_push_buffer_count.attr,
	&dev_attr_pull_buffer_count.attr,
	&dev_attr_push_buffer_count_lw.attr,
	&dev_attr_pull_buffer_count_lw.attr,
	&dev_attr_clear_stats.attr,
	&dev_attr_aggregator.attr,
	&dev_attr_spad.attr,
	&dev_attr_auto_dma.attr,
	&dev_attr_ident.attr,
	&dev_attr_RW32_debug.attr,
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
void mgt400_createSysfs(struct device *dev)
{
	struct mgt400_dev *mdev = mgt400_devices[dev->id];

	dev_info(dev, "mgt400_createSysfs()");
	if (sysfs_create_files(&dev->kobj, sysfs_base_attrs)){
		dev_err(dev, "failed to create sysfs");
	}else if (IS_MGT_DRAM(mdev)){
		if (sysfs_create_files(&dev->kobj, sysfs_mgtdram_attrs)){
			dev_err(dev, "failed to create sysfs2");
		}
	}
}
