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
#include "acq400_sysfs.h"
#include "bwg.h"

static ssize_t show_module_type(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct bwg_dev *bwg_dev = bwg_devices[dev->id];
	return sprintf(buf, "%X\n", bwg_dev->mod_id>>MOD_ID_TYPE_SHL);
}


static DEVICE_ATTR(module_type, S_IRUGO, show_module_type, 0);


static ssize_t store_RW32_debug(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct bwg_dev *bwg_dev = bwg_devices[dev->id];
	int RW32_debug;

	if (sscanf(buf, "%d", &RW32_debug) == 1){
		bwg_dev->RW32_debug = RW32_debug;
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
	struct bwg_dev *bwg_dev = bwg_devices[dev->id];
	return sprintf(buf, "%d\n", bwg_dev->RW32_debug);
}

static DEVICE_ATTR(RW32_debug,
		S_IRUGO|S_IWUSR, show_RW32_debug, store_RW32_debug);


/* generic show_bits, store_bits */

ssize_t acq400_show_bits(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;

	return sprintf(buf, "%x\n", field);
}

ssize_t acq400_show_bitN(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;

	return sprintf(buf, "%x\n", !field);
}

ssize_t acq400_store_bitN(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK,
		unsigned show_warning)
{
	u32 field;
	if (sscanf(buf, "%x", &field) == 1){
		u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= ((!field)&MASK) << SHL;
		if (show_warning){
			dev_warn(dev, "deprecated %04x = %08x", REG, regval);
		}
		bwg_wr32(bwg_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}

ssize_t acq400_store_bits(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK,
		unsigned show_warning)
{
	u32 field;
	if (sscanf(buf, "%x", &field) == 1){
		u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		if (show_warning){
			dev_warn(dev, "deprecated %04x = %08x", REG, regval);
		}
		bwg_wr32(bwg_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}

ssize_t acq400_show_dnum(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;
	u32 sbit = (MASK+1) >> 1;
	if ((sbit&field) != 0){
		while(sbit <<= 1){
			field |= sbit;
		}
	}

	return sprintf(buf, "%d\n", (int)field);
}
ssize_t acq400_store_dnum(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK)
{
	int field;
	if (sscanf(buf, "%d", &field) == 1){
		u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		bwg_wr32(bwg_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}




MAKE_BITS(wa_cr,  WA_CR, MAKE_BITS_FROM_MASK, 0xffffffff);
MAKE_DNUM(wa_len, WA_LEN, 0xffffffff);
MAKE_DNUM(wa_nco, WA_NCO, 0xffffffff);

MAKE_BITS(wb_cr,  WB_CR, MAKE_BITS_FROM_MASK, 0xffffffff);
MAKE_DNUM(wb_len, WB_LEN, 0xffffffff);
MAKE_DNUM(wb_nco, WB_NCO, 0xffffffff);


static ssize_t show_sta(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG)
{
	u32 regval = bwg_rd32(bwg_devices[dev->id], REG);
	return sprintf(buf, "%u,%u\n", regval>>16, regval&0x0ffff);
}


#define MAKE_STA(NAME, REG)						\
static ssize_t show_sta##NAME(						\
	struct device *d,						\
	struct device_attribute *a,					\
	char *b)							\
{									\
	return show_sta(d, a, b, REG);					\
}									\
									\
static DEVICE_ATTR(NAME, S_IRUGO, show_sta##NAME, 0)

MAKE_STA(wa_sta, WA_STA);
MAKE_STA(wb_sta, WB_STA);

static const struct attribute *sysfs_base_attrs[] = {
	&dev_attr_wa_cr.attr,
	&dev_attr_wa_len.attr,
	&dev_attr_wa_nco.attr,
	&dev_attr_wa_sta.attr,
	&dev_attr_wb_cr.attr,
	&dev_attr_wb_len.attr,
	&dev_attr_wb_nco.attr,
	&dev_attr_wb_sta.attr,
	&dev_attr_module_type.attr,
	&dev_attr_RW32_debug.attr,
	NULL
};


void bwg_createSysfs(struct device *dev)
{
	dev_info(dev, "bwg_createSysfs()");
	if (sysfs_create_files(&dev->kobj, sysfs_base_attrs)){
		dev_err(dev, "failed to create sysfs");
	}
}
