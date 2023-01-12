/* ------------------------------------------------------------------------- */
/* acq423_sysfs.c ACQ420_FMC						     */
/* ------------------------------------------------------------------------- */
/* acq423_sysfs.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 3 May 2018  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2018 Peter Milne, D-TACQ Solutions Ltd         *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO 
 * TODO
 * ------------------------------------------------------------------------- */

#include "acq400.h"
#include "hbm.h"

#include "acq400_sysfs.h"

static int odd_channels_status(struct acq400_dev *adev)
{
	u32 br = acq400rd32(adev, ACQ423_BANK);
	return (br&ACQ423_BANK_ODD_CHAN) != 0;
}

static ssize_t show_span(
	int ch, struct device * dev, struct device_attribute *attr,
	char * buf)
{
	int ch0 = ch - 1;
	int offsetw = ch0/ACQ423_SPAN_FPR;
	int offsetb = offsetw*sizeof(int);
	int shl = (ch0%ACQ423_SPAN_FPR) * ACQ423_SPAN_FW;
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 reg = acq400rd32(adev, ACQ423_SPAN_A+offsetb);
	return sprintf(buf, "%d\n", (reg >> shl) &ACQ423_SPAN_MASK);
}

static ssize_t store_span(
	int ch, struct device * dev, struct device_attribute *attr,
	const char * buf, size_t count)
{
	int ch0 = ch - 1;
	int offsetw = ch0/ACQ423_SPAN_FPR;
	int offsetb = offsetw*sizeof(int);
	int shl = (ch0%ACQ423_SPAN_FPR) * ACQ423_SPAN_FW;

	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 span;
	u32 reg = acq400rd32(adev, ACQ423_SPAN_A+offsetb);

	if (sscanf(buf, "0x%x", &span) == 0 &&
	    sscanf(buf, "%d", &span) == 0){
		return -1;
	}else if (span == 0){
		return -ERANGE;
	}else if (span > 7){
		return -ERANGE;
	}

	reg &= ~(ACQ423_SPAN_MASK<<shl);
	reg |= (span&ACQ423_SPAN_MASK) << shl;
	if (odd_channels_status(adev)){
		/* ALL EVENS MUST BE ZERO */
		reg &= ~ACQ423_SPAN_MASK_ODDS;
	}
	acq400wr32(adev, ACQ423_SPAN_A+offsetb, reg);
	return count;
}
#define MAKE_ACQ423_SPAN(NAME, CH)				\
static ssize_t show_span_ch##NAME(				\
	struct device * dev,					\
	struct device_attribute *attr,				\
	char * buf)						\
{								\
	return show_span(CH, dev, attr, buf);			\
}								\
								\
static ssize_t store_span_ch##NAME(				\
	struct device * dev,					\
	struct device_attribute *attr,				\
	const char * buf,					\
	size_t count)						\
{								\
	return store_span(CH, dev, attr, buf, count);		\
}								\
static DEVICE_ATTR(gain##NAME, S_IRUGO|S_IWUSR,		\
		show_span_ch##NAME, store_span_ch##NAME)

MAKE_ACQ423_SPAN( 1, 1);
MAKE_ACQ423_SPAN( 2, 2);
MAKE_ACQ423_SPAN( 3, 3);
MAKE_ACQ423_SPAN( 4, 4);
MAKE_ACQ423_SPAN( 5, 5);
MAKE_ACQ423_SPAN( 6, 6);
MAKE_ACQ423_SPAN( 7, 7);
MAKE_ACQ423_SPAN( 8, 8);

MAKE_ACQ423_SPAN( 9, 9);
MAKE_ACQ423_SPAN(10,10);
MAKE_ACQ423_SPAN(11,11);
MAKE_ACQ423_SPAN(12,12);
MAKE_ACQ423_SPAN(13,13);
MAKE_ACQ423_SPAN(14,14);
MAKE_ACQ423_SPAN(15,15);
MAKE_ACQ423_SPAN(16,16);

MAKE_ACQ423_SPAN(17,17);
MAKE_ACQ423_SPAN(18,18);
MAKE_ACQ423_SPAN(19,19);
MAKE_ACQ423_SPAN(20,20);
MAKE_ACQ423_SPAN(21,21);
MAKE_ACQ423_SPAN(22,22);
MAKE_ACQ423_SPAN(23,23);
MAKE_ACQ423_SPAN(24,24);

MAKE_ACQ423_SPAN(25,25);
MAKE_ACQ423_SPAN(26,26);
MAKE_ACQ423_SPAN(27,27);
MAKE_ACQ423_SPAN(28,28);
MAKE_ACQ423_SPAN(29,29);
MAKE_ACQ423_SPAN(30,30);
MAKE_ACQ423_SPAN(31,31);
MAKE_ACQ423_SPAN(32,32);


static ssize_t show_gains(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int ch0;
	unsigned regs[4];
	int reg;

	for (reg = 0; reg < 4; ++reg){
		regs[reg] = acq400rd32(adev, ACQ423_SPAN_A+reg*sizeof(int));
	}
	for (ch0 = 0; ch0 < 32; ++ch0){
		int shl = (ch0%ACQ423_SPAN_FPR) * ACQ423_SPAN_FW;
		reg = ch0/ACQ423_SPAN_FPR;
		sprintf(buf+ch0, "%x", (regs[reg] >> shl)&ACQ423_SPAN_MASK);
	}
	strcat(buf, "\n");
	return strlen(buf);
}
static DEVICE_ATTR(gains, S_IRUGO, show_gains, 0);

static ssize_t show_odd_channels(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%d\n", odd_channels_status(adev));
}
static ssize_t store_odd_channels(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
/* user mask: 1=enabled. Compute nchan_enabled BEFORE inverting MASK */
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int odd_chan_en;

	if (sscanf(buf, "%d", &odd_chan_en) == 1){
		u32 br = acq400rd32(adev, ACQ423_BANK);

		if (odd_chan_en){
			br |= ACQ423_BANK_ODD_CHAN;
			adev->nchan_enabled = 16;
		}else{
			br &= ~ACQ423_BANK_ODD_CHAN;
			adev->nchan_enabled = 32;
		}

		acq400wr32(adev, ACQ423_BANK, br);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(odd_channels,
		S_IRUGO|S_IWUSR, show_odd_channels, store_odd_channels);

MAKE_BITS(clk_from_sync, ADC_CTRL, MAKE_BITS_FROM_MASK, ADC_CTRL_423_CLK_FROM_SYNC);
MAKE_BITS(d37_mode,  ACQ423_BANK, MAKE_BITS_FROM_MASK, ACQ423_BANK_D37_MODE);

const struct attribute *acq423_attrs[] = {
	&dev_attr_odd_channels.attr,
	&dev_attr_d37_mode.attr,
	&dev_attr_clk_from_sync.attr,
	&dev_attr_gain1.attr,
	&dev_attr_gain2.attr,
	&dev_attr_gain3.attr,
	&dev_attr_gain4.attr,
	&dev_attr_gain5.attr,
	&dev_attr_gain6.attr,
	&dev_attr_gain7.attr,
	&dev_attr_gain8.attr,
	&dev_attr_gain9.attr,
	&dev_attr_gain10.attr,
	&dev_attr_gain11.attr,
	&dev_attr_gain12.attr,
	&dev_attr_gain13.attr,
	&dev_attr_gain14.attr,
	&dev_attr_gain15.attr,
	&dev_attr_gain16.attr,
	&dev_attr_gain17.attr,
	&dev_attr_gain18.attr,
	&dev_attr_gain19.attr,
	&dev_attr_gain20.attr,
	&dev_attr_gain21.attr,
	&dev_attr_gain22.attr,
	&dev_attr_gain23.attr,
	&dev_attr_gain24.attr,
	&dev_attr_gain25.attr,
	&dev_attr_gain26.attr,
	&dev_attr_gain27.attr,
	&dev_attr_gain28.attr,
	&dev_attr_gain29.attr,
	&dev_attr_gain30.attr,
	&dev_attr_gain31.attr,
	&dev_attr_gain32.attr,
	&dev_attr_gains.attr,
	NULL
};
