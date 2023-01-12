/* ------------------------------------------------------------------------- */
/* dio_sysfs.c D-TACQ ACQ400 series driver, sysfs (knobs)	             */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2010 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
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


#include <linux/ctype.h>

#include "acq400.h"
#include "acq400_sysfs.h"

#include "sysfs_attrs.h"


static ssize_t show_DO32_reg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned reg)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "0x%08x\n", acq400rd32(adev, reg));
}

/*
 * DO32=value  :: DO32 = value
 * DO32=+value :: DO32 |= value   	set bits
 * DO32=-value :: DO32 &= ~value        clear bits
 * DO32=+dvalue:: DO32 |= 1<<value      set bit
 * DO32=-dvalue:: DO32 &= ~(1<<value)   clear bit
 */
static ssize_t store_DO32_reg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	unsigned reg)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned DO32 = 0;
	enum BITMODE { CLR=-1, CPY=0, SET=1 } mode = CPY;		/* -1: CLR, 0: OVERWRITE, 1: SET */
	int is_bit = 0;

	if (buf[0] == '+'){
		mode = SET;
		buf++;
	}else if (buf[0] == '-'){
		mode = CLR;
		buf++;
	}
	if (mode != CPY && buf[0] == 'd'){
		is_bit = 1;
		buf++;
	}
	if (sscanf(buf, "0x%x", &DO32) == 1 || sscanf(buf, "%u", &DO32) == 1){
		if (mode != CPY){
			unsigned old = acq400rd32(adev, reg);
			if (is_bit){
				DO32 = 1<<DO32;
			}
			if (mode == SET){
				DO32 = old | DO32;
			}else if (mode == CLR){
				DO32 = old & ~DO32;
			}else{
				return -1;
			}
		}
		acq400wr32(adev, reg, DO32);
		return count;
	}else{
		return -1;
	}
}


#define MAKE_STORE_DO32(name, wreg, rreg) 					\
static ssize_t show_##name(							\
	struct device * dev,							\
	struct device_attribute *attr,						\
	char * buf)								\
{										\
	return show_DO32_reg(dev, attr, buf, rreg);				\
}										\
static ssize_t store_##name(							\
	struct device * dev,							\
	struct device_attribute *attr,						\
	const char * buf,							\
	size_t count)								\
{										\
	return store_DO32_reg(dev, attr, buf, count, wreg);			\
}										\
static DEVICE_ATTR(name, S_IRUGO|S_IWUSR, show_##name, store_##name)

MAKE_STORE_DO32(DO32, 			DIO432_FIFO, DIO432_DI_SNOOP);
MAKE_STORE_DO32(DO32_immediate_mask, 	DIO482_PG_IMM_MASK, DIO482_PG_IMM_MASK);

extern const struct device_attribute dev_attr_byte_is_output;

SCOUNT_KNOB(FPTRG, DIO482_PG_FPTRG_COUNT);

static ssize_t show_num_pg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, DIO432_CTRL);

	return sprintf(buf, "%d\n", ctrl&DIO432_CTRL_PG_CLK_IS_DO? 5: 4);
}

static ssize_t store_num_pg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, DIO432_CTRL);
	unsigned num_pg;

	if (sscanf(buf, "%u", &num_pg) == 1){
		if (num_pg == 5){
			ctrl |= DIO432_CTRL_PG_CLK_IS_DO;
		}else{
			ctrl &= ~DIO432_CTRL_PG_CLK_IS_DO;
		}
		acq400wr32(adev, DIO432_CTRL, ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(num_pg, S_IRUGO|S_IWUSR, show_num_pg, store_num_pg);

MAKE_BITS(chain_PG,   DIO432_CTRL, MAKE_BITS_FROM_MASK, DIO432_CTRL_CHAIN_PG);
MAKE_BITS(trgout_PG4, DIO432_CTRL, MAKE_BITS_FROM_MASK, DIO432_CTRL_CHAIN_PG);
MAKE_DNUM(wdt, 	      DIO482_PG_WDT, DIO482_PG_WDT_MASK);

MAKE_BITS(bypass_trg_debounce, DIO432_CTRL, MAKE_BITS_FROM_MASK, DIO432_CTRL_BYPASS_TRG);


const struct attribute *dio482_pg_attrs[] = {
		&dev_attr_DO32.attr,
		&dev_attr_DO32_immediate_mask.attr,
		&dev_attr_scount_FPTRG.attr,
		&dev_attr_trgout_PG4.attr,
		&dev_attr_chain_PG.attr,
		&dev_attr_num_pg.attr,
		&dev_attr_wdt.attr,
		&dev_attr_bypass_trg_debounce.attr,
		NULL
};

const struct attribute *dio482_pg32_attrs[] = {
		&dev_attr_DO32.attr,
		&dev_attr_DO32_immediate_mask.attr,
		&dev_attr_wdt.attr,
		&dev_attr_bypass_trg_debounce.attr,
		NULL
};


MAKE_BITS(pwm_clkdiv, PWM_SOURCE_CLK_CTRL, MAKE_BITS_FROM_MASK, 0xffff<<PWM_SOURCE_CLK_CTRL_DIV_SHL);
MAKE_SIGNAL(pwm_src, PWM_SOURCE_CLK_CTRL, PWM_SOURCE_CLK_CTRL_SHL, PWM_SOURCE_CLK_CTRL_EN, ENA, DIS, 1);


const struct attribute *pwm2_attrs[] = {
	&dev_attr_pwm_src.attr,
	&dev_attr_pwm_clkdiv.attr,
	NULL
};



static ssize_t show_bits_cos_en(
	struct device *d,
	struct device_attribute *a,
	char *b)
{
	return acq400_show_bits(d, a, b, DIO482_COS_EN, 0, 0xffffffff);
}
static ssize_t store_bits_cos_en(
	struct device * d,
	struct device_attribute *a,
	const char * b,
	size_t c)
{
	ssize_t rc = acq400_store_bits(d, a, b, c, DIO482_COS_EN, 0, 0xffffffff, 0);

	if (rc > 0) {
		struct acq400_dev *adev = acq400_devices[d->id];
		u32 ctrl = acq400rd32(adev, DIO482_COS_EN);
		u32 icr =  x400_get_interrupt(adev);
		if (ctrl){
			icr |= DIO_INT_CSR_COS_EN;
		}else{
			icr &= ~DIO_INT_CSR_COS_EN;
		}
		x400_set_interrupt(adev, icr);
	}
	return rc;
}
static DEVICE_ATTR(cos_en, S_IRUGO|S_IWUSR, show_bits_cos_en, store_bits_cos_en);


static ssize_t show_status_latch(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XTD_dev *xtd_dev = container_of(adev, struct XTD_dev, adev);
	unsigned src = xtd_dev->atd.event_source;

	xtd_dev->atd.event_source = 0;
	return sprintf(buf, "%08x\n", src);
}

static DEVICE_ATTR(status_latch, S_IRUGO, show_status_latch, 0);


static ssize_t show_di_snoop(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%08x\n", acq400rd32(adev, DIO432_DI_SNOOP));
}

static DEVICE_ATTR(di_snoop, S_IRUGO, show_di_snoop, 0);


const struct attribute *dio482_attrs[] = {
	&dev_attr_di_snoop.attr,
	&dev_attr_status_latch.attr,
	&dev_attr_cos_en.attr,
	NULL
};




/* TDC ... is a dio! */

MAKE_BITS(tdc_en, 	TDC_CR, 	  MAKE_BITS_FROM_MASK, TDC_CR_ENABLE);
MAKE_BITS(tdc_train, 	TDC_CR, 	  MAKE_BITS_FROM_MASK, TDC_CR_TRAIN);
MAKE_BITS(tdc_pad_en,   TDC_CR,           MAKE_BITS_FROM_MASK, TDC_CR_PAD_EN);
MAKE_BITS(tdc_load_cal, TDC_LOADED_CALIB, MAKE_BITS_FROM_MASK, 0xffffffff);

MAKE_BITS(tdc_disable_ch1, TDC_CH_MASK, MAKE_BITS_FROM_MASK, TDC_CH_MASK_CH1);
MAKE_BITS(tdc_disable_ch2, TDC_CH_MASK, MAKE_BITS_FROM_MASK, TDC_CH_MASK_CH2);
MAKE_BITS(tdc_disable_ch3, TDC_CH_MASK, MAKE_BITS_FROM_MASK, TDC_CH_MASK_CH3);
MAKE_BITS(tdc_disable_ch4, TDC_CH_MASK, MAKE_BITS_FROM_MASK, TDC_CH_MASK_CH4);

SCOUNT_KNOB(evt_ch1, 	TDC_CH1_EVT_COUNT);
SCOUNT_KNOB(evt_ch2, 	TDC_CH2_EVT_COUNT);
SCOUNT_KNOB(evt_ch3, 	TDC_CH3_EVT_COUNT);
SCOUNT_KNOB(evt_ch4, 	TDC_CH4_EVT_COUNT);

const struct attribute *acq494_attrs[] = {
	&dev_attr_tdc_en.attr,
	&dev_attr_tdc_pad_en.attr,
	&dev_attr_tdc_train.attr,
	&dev_attr_tdc_load_cal.attr,

	&dev_attr_tdc_disable_ch1.attr,
	&dev_attr_tdc_disable_ch2.attr,
	&dev_attr_tdc_disable_ch3.attr,
	&dev_attr_tdc_disable_ch4.attr,

	&dev_attr_scount_evt_ch1.attr,
	&dev_attr_scount_evt_ch2.attr,
	&dev_attr_scount_evt_ch3.attr,
	&dev_attr_scount_evt_ch4.attr,
	0
};





#define KNOB_FUN_REG(fun, chan, REG, FIELDS, ZB)					\
ssize_t show_##fun##chan(								\
	struct device * dev,								\
	struct device_attribute *attr,							\
	char * buf)									\
{											\
	return show_fields(dev, attr, buf, #fun#chan, REG, FIELDS, ZB);			\
}											\
ssize_t store_##fun##chan(								\
	struct device * dev,								\
	struct device_attribute *attr,							\
	const char * buf,								\
	size_t count)									\
{											\
	return store_fields(dev, attr, buf, count, REG, FIELDS, ZB);			\
}											\
static DEVICE_ATTR(fun##chan, S_IRUGO|S_IWUSR, show_##fun##chan, store_##fun##chan)

const unsigned PPW_TRG_FIELDS[] = { PPW_TRG_BUS, PPW_TRG_BIT, PPW_TRG_RISING, 0 };
const unsigned PPW_PWM_FIELDS[] = { PPW_PWM_IS, PPW_PWM_ICOUNT, PPW_PWM_OCOUNT, PPW_PWM_PRD, 0 };
const unsigned PPW_REP_FIELDS[] = { PPW_REP_FIELD, 0 };

#define PPW_FUNS(CH) \
	KNOB_FUN_REG(ppw_trg, CH, PPW_TRG(CH), PPW_TRG_FIELDS, !ZERO_BASED); \
	KNOB_FUN_REG(ppw_pwm, CH, PPW_PWM(CH), PPW_PWM_FIELDS, !ZERO_BASED); \
	KNOB_FUN_REG(ppw_rep, CH, PPW_REP(CH), PPW_REP_FIELDS, ZERO_BASED)

PPW_FUNS(1);
PPW_FUNS(2);
PPW_FUNS(3);
PPW_FUNS(4);
PPW_FUNS(5);
PPW_FUNS(6);

const struct attribute *dio482ppw_attrs[] = {
	&dev_attr_DO32.attr,
	&dev_attr_DO32_immediate_mask.attr,
	&dev_attr_ppw_trg1.attr, &dev_attr_ppw_pwm1.attr, &dev_attr_ppw_rep1.attr,
	&dev_attr_ppw_trg2.attr, &dev_attr_ppw_pwm2.attr, &dev_attr_ppw_rep2.attr,
	&dev_attr_ppw_trg3.attr, &dev_attr_ppw_pwm3.attr, &dev_attr_ppw_rep3.attr,
	&dev_attr_ppw_trg4.attr, &dev_attr_ppw_pwm4.attr, &dev_attr_ppw_rep4.attr,
	&dev_attr_ppw_trg5.attr, &dev_attr_ppw_pwm5.attr, &dev_attr_ppw_rep5.attr,
	&dev_attr_ppw_trg6.attr, &dev_attr_ppw_pwm6.attr, &dev_attr_ppw_rep6.attr,
	0
};

