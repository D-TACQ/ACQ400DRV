/*
 * mgt400_sysfs_utils.c
 *
 *  Created on: 18 May 2022
 *      Author: pgm
 */

#include <linux/ctype.h>

#include "acq400.h"
#include "mgt400.h"

#include "mgt400_sysfs.h"
#include "acq400_sysfs.h"

ssize_t mgt400_show_bits(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = mgt400rd32(mgt400_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;

	return sprintf(buf, "%x\n", field);
}

ssize_t mgt400_store_bits(
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
		u32 regval = mgt400rd32(mgt400_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		if (show_warning){
			dev_warn(dev, "deprecated %04x = %08x", REG, regval);
		}
		mgt400wr32(mgt400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}

ssize_t mgt400_show_dnum(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = mgt400rd32(mgt400_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;
	u32 sbit = (MASK+1) >> 1;
	if ((sbit&field) != 0){
		while(sbit <<= 1){
			field |= sbit;
		}
	}

	return sprintf(buf, "%d\n", (int)field);
}
ssize_t mgt400_store_dnum(
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
		u32 regval = mgt400rd32(mgt400_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		mgt400wr32(mgt400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}


ssize_t mgt400_show_u(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = mgt400rd32(mgt400_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;

	return sprintf(buf, "%u\n", field);
}
ssize_t mgt400_store_u(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK)
{
	unsigned field;
	if (sscanf(buf, "%u", &field) == 1){
		u32 regval = mgt400rd32(mgt400_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		mgt400wr32(mgt400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}
