/* ------------------------------------------------------------------------- */
/* acq480_sysfs.c ACQ420_FMC						     */
/*
 * acq480_sysfs.c
 *
 *  Created on: 27 Jul 2015
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

/* acq480 training
 * 0: RESET	set CR=0x09, reset
 * 1: START	set CR=0x01, enable
 * 						acq480_knobs : Set ADC DESKEW
 * 2: DESKEW	set DESKEW, wait done
 * 3:           DESKEW DONE			acq480_knobs : Set ADC SYNC
 * 4: SYNC	set SYNC, wait done
 * 5:		SYNC DONE			acq480_knobs : Clear SYNC
 * 4: ACTIVATE 	set CR=0x15 : go
 */

enum ACQ480_TRAINING {
	ACQ480_RESET,
	ACQ480_START,
	ACQ480_DESKEW,
	ACQ480_DESKEW_DONE,
	ACQ480_SYNC,
	ACQ480_SYNC_DONE,
	ACQ480_ACTIVATE,
	ACQ480_FAIL
};

static ssize_t show_train_states(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	char* pb = buf;
#define ST_APPEND(ST) pb += sprintf(pb, "%s=%d\n", #ST, ST)
	ST_APPEND(ACQ480_RESET);
	ST_APPEND(ACQ480_START);
	ST_APPEND(ACQ480_DESKEW);
	ST_APPEND(ACQ480_DESKEW_DONE);
	ST_APPEND(ACQ480_SYNC);
	ST_APPEND(ACQ480_SYNC_DONE);
	ST_APPEND(ACQ480_ACTIVATE);
	ST_APPEND(ACQ480_FAIL);
	return pb - buf;
#undef ST_APPEND
}
static DEVICE_ATTR(train_states, S_IRUGO, show_train_states, 0);

static ssize_t show_train(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);

	return sprintf(buf, "%u\n", adc_dev->acq480_train);
}

static ssize_t acq480_reset(struct acq400_dev *adev, int goodrc)
{
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);

	u32 ctrl = acq400rd32(adev, ADC_CTRL);
	ctrl &= ~(ADC_CTRL_ADC_EN|ADC_CTRL_FIFO_EN);
	ctrl &= ~(ADC480_CTRL_DESKEW_TRAIN|ADC480_CTRL_SYNC_TRAIN);
	acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_ADC_RST);

	adc_dev->acq480_train = ACQ480_RESET;
	return goodrc;
}

static ssize_t acq480_start(struct acq400_dev *adev, int goodrc)
{
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);

	if (adc_dev->acq480_train != ACQ480_RESET){
		dev_err(DEVP(adev), "acq480_start not in ACQ480_RESET");
		return -1;
	}else{
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		acq400wr32(adev, ADC_CTRL, ctrl &~ ADC_CTRL_ADC_RST);
		adc_dev->acq480_train = ACQ480_START;
		return goodrc;
	}
}

static int training_done(unsigned stat, unsigned shl)
// DONE when ALL bits HI
{
	return ((stat>>shl) & ADC480_FIFO_STA_DONE_MASK) == ADC480_FIFO_STA_DONE_MASK;
}
static ssize_t acq480_deskew(struct acq400_dev *adev, int goodrc)
{
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);

	if (adc_dev->acq480_train != ACQ480_START){
		dev_err(DEVP(adev), "acq480_deskew not in ACQ480_START");
		return -1;
	}else{
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		u32 stat;
		acq400wr32(adev, ADC_CTRL, ctrl | ADC480_CTRL_DESKEW_TRAIN);
		adc_dev->acq480_train = ACQ480_DESKEW;

		stat = acq400rd32(adev, ADC_FIFO_STA);
		msleep(20);
		stat = acq400rd32(adev, ADC_FIFO_STA);
		if (!training_done(stat, ADC480_FIFO_STA_DESKEW_DONE_SHL)){
			dev_err(DEVP(adev), "deskew failed to complete %08x", stat);
			return -1;
		}else{
			adc_dev->acq480_train = ACQ480_DESKEW_DONE;
			return goodrc;
		}
	}
}

static ssize_t acq480_sync(struct acq400_dev *adev, int goodrc)
{
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);

	if (adc_dev->acq480_train != ACQ480_DESKEW_DONE){
		dev_err(DEVP(adev), "acq480_sync not in ACQ480_DESKEW_DONE");
		return -1;
	}else{
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		u32 stat;
		acq400wr32(adev, ADC_CTRL, ctrl | ADC480_CTRL_SYNC_TRAIN);
		adc_dev->acq480_train = ACQ480_SYNC;

		stat = acq400rd32(adev, ADC_FIFO_STA);
		msleep(20);
		stat = acq400rd32(adev, ADC_FIFO_STA);
		if (!training_done(stat, ADC480_FIFO_STA_SYNC_DONE_SHL)){
			dev_err(DEVP(adev), "deskew failed to complete %08x", stat);
			return -1;
		}else{
			acq400wr32(adev, ADC_CTRL, ctrl &
				~(ADC480_CTRL_DESKEW_TRAIN|ADC480_CTRL_SYNC_TRAIN));
			adc_dev->acq480_train = ACQ480_SYNC_DONE;
			return goodrc;
		}
	}
}
static ssize_t acq480_activate(struct acq400_dev *adev, int goodrc)
{
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);

	if (adc_dev->acq480_train != ACQ480_SYNC_DONE){
		dev_err(DEVP(adev), "acq480_activate not in ACQ480_SYNC_DONE");
		return -1;
	}else{
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_FIFO_EN | ADC_CTRL_ADC_EN);
		dev_info(DEVP(adev), "acq480_activate GOOD");
		adc_dev->acq480_train = ACQ480_ACTIVATE;
		return goodrc;
	}
}


static ssize_t store_train(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);
	int train;

	if (sscanf(buf, "%u", &train) == 1){
		dev_dbg(DEVP(adev), "store_train() %u", train);
		switch(train){
		case ACQ480_RESET:
			return acq480_reset(adev, count);
		case ACQ480_START:
			return acq480_start(adev, count);
		case ACQ480_DESKEW:
			return acq480_deskew(adev, count);
		case ACQ480_SYNC:
			return acq480_sync(adev, count);
		case ACQ480_ACTIVATE:
			return acq480_activate(adev, count);
		case ACQ480_DESKEW_DONE:
		case ACQ480_SYNC_DONE:
			dev_err(DEVP(adev), "do not set DONE state %u", train);
			return -1;
		case ACQ480_FAIL:
			adc_dev->acq480_train = ACQ480_FAIL;
			break;
		default:
			dev_err(DEVP(adev), "unrecognised state %u", train);
		}

		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(train, S_IRUGO|S_IWUSR, show_train, store_train);


static ssize_t store_dclock_reset(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 reset;

	if (sscanf(buf, "%u", &reset) == 1 && reset == 1){
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		if (ctrl & ADC_CTRL_ENABLE_CAPTURE){
			dev_err(dev, "resetting DCLK, but ADC_ENABLED: 0x%08x", ctrl);
			ctrl &= ~ADC_CTRL_ENABLE_CAPTURE;
			acq400wr32(adev, ADC_CTRL, ctrl);
		}
		if (ctrl & ADC_CTRL_480_DCLK_SYNC){
			dev_err(dev, "resetting DCLK, but it was already set: 0x%08x", ctrl);
		}
		ctrl &= ~ADC_CTRL_480_DCLK_SYNC;
		dev_dbg(dev, "setting ADC_CTRL %08x", ctrl|ADC_CTRL_480_DCLK_SYNC);
		acq400wr32(adev, ADC_CTRL, ctrl|ADC_CTRL_480_DCLK_SYNC);
		msleep(1);
		dev_dbg(dev, "clear ADC_CTRL %08x", ctrl);
		acq400wr32(adev, ADC_CTRL, ctrl);
		return count;
	}else{
		return -1;
	}
}


static DEVICE_ATTR(dclock_reset, S_IWUSR, 0, store_dclock_reset);

static ssize_t show_acq480_train_ctrl(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 tc = acq400rd32(adev, ACQ480_TRAIN_CTRL);

	return sprintf(buf, "0x%08x\n", tc);
}



static ssize_t store_acq480_train_ctrl(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 tc;

	if (sscanf(buf, "%x", &tc) == 1){
		acq400wr32(adev, ACQ480_TRAIN_CTRL, tc);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(acq480_train_ctrl,
		S_IRUGO|S_IWUSR, show_acq480_train_ctrl, store_acq480_train_ctrl);

static ssize_t show_acq480_two_lane_mode(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);

	return sprintf(buf, "%d\n", (adc_ctrl&ADC_CTRL_480_TWO_LANE_MODE) != 0);
}


static ssize_t store_acq480_two_lane_mode(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 tc;

	if (sscanf(buf, "%x", &tc) == 1){
		u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);
		if (tc){
			adc_ctrl |= ADC_CTRL_480_TWO_LANE_MODE;
		} else {
			adc_ctrl &= ~ADC_CTRL_480_TWO_LANE_MODE;
		}
		acq400wr32(adev, ADC_CTRL, adc_ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(acq480_two_lane_mode,
		S_IRUGO|S_IWUSR, show_acq480_two_lane_mode, store_acq480_two_lane_mode);


static ssize_t show_acq482_cmap(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);

	return sprintf(buf, "%d\n", (adc_ctrl&ADC_CTRL_482_CMAP) != 0);
}


static ssize_t store_acq482_cmap(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 tc;

	if (sscanf(buf, "%x", &tc) == 1){
		u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);
		if (tc){
			adc_ctrl |= ADC_CTRL_482_CMAP;
		} else {
			adc_ctrl &= ~ADC_CTRL_482_CMAP;
		}
		acq400wr32(adev, ADC_CTRL, adc_ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(acq482_cmap,
		S_IRUGO|S_IWUSR, show_acq482_cmap, store_acq482_cmap);



static ssize_t show_acq480_train_xx_val(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	int data_reg, int frame_shl)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 tc = acq400rd32(adev, data_reg);
	u32 ctrl = acq400rd32(adev, ADC_CTRL);

	return sprintf(buf, "0x%08x %x\n", tc,
			(ctrl>>frame_shl)&ADC480_CTRL_FRAME_MASK);
}


static ssize_t show_acq480_train_hi_val(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return show_acq480_train_xx_val(dev,attr, buf,
			ACQ480_TRAIN_HI_VAL, ADC480_CTRL_FRAME_HI_SHL);
}


static DEVICE_ATTR(acq480_train_hi_val, S_IRUGO, show_acq480_train_hi_val, 0);

static ssize_t show_acq480_train_lo_val(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return show_acq480_train_xx_val(dev,attr, buf,
			ACQ480_TRAIN_LO_VAL, ADC480_CTRL_FRAME_LO_SHL);
}


static DEVICE_ATTR(acq480_train_lo_val, S_IRUGO, show_acq480_train_lo_val, 0);



static ssize_t show_acq480_loti(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%d\n", acq480_train_fail(adev));
}


static DEVICE_ATTR(acq480_loti, S_IRUGO, show_acq480_loti, 0);

static ssize_t show_acq480_fpga_decim(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 version = GET_MOD_ID_VERSION(acq400_devices[dev->id]);
	int decim;

	version &= ~(MOD_ID_IS_SLAVE>>MOD_ID_VERSION_SHL);

	switch(version){
	case MOD_ID_TYPE_ACQ480DIV4:
		decim = 4; break;
	case MOD_ID_TYPE_ACQ480DIV10:
		decim = 10; break;
	default:
		decim = 1;
	}

	return sprintf(buf, "%d\n", decim);
}


static DEVICE_ATTR(acq480_fpga_decim, S_IRUGO, show_acq480_fpga_decim, 0);

static ssize_t store_ffir_reset(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned reset;

	if (sscanf(buf, "%u", &reset) == 1 && reset){
		u32 csr = acq400rd32(adev, ACQ480_FIRCO_CSR);
		csr &= ~ACQ480_FIRCO_CSR_RESET;
		acq400wr32(adev, ACQ480_FIRCO_CSR, csr|ACQ480_FIRCO_CSR_RESET);
		mdelay(2);
		acq400wr32(adev, ACQ480_FIRCO_CSR, csr);
		return count;
	}else{
		return -1;
	}
}
static DEVICE_ATTR(ffir_reset, S_IWUSR, 0, store_ffir_reset);

static ssize_t show_ffir_counter(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 csr = acq400rd32(adev, ACQ480_FIRCO_CSR);
	u32 ctr = (csr&ACQ480_FIRCO_CSR_CTR)>>ACQ480_FIRCO_CSR_CTR_SHL;
	return sprintf(buf, "%u\n", ctr);
}

static DEVICE_ATTR(ffir_counter, S_IRUGO, show_ffir_counter, 0);

static ssize_t store_ffir_coeff(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int coeff;

	if (sscanf(buf, "%d", &coeff) == 1){
		acq400wr32(adev, ACQ480_FIRCO_LOAD, coeff);
		return count;
	}else{
		return -1;
	}
}
static DEVICE_ATTR(ffir_coeff, S_IWUSR, 0, store_ffir_coeff);

MAKE_BITS(mr_sel1, ACQ480_ADC_MULTIRATE, MAKE_BITS_FROM_MASK, ACQ480_ADC_MR_MRSEL1);
MAKE_BITS(mr_sel0, ACQ480_ADC_MULTIRATE, MAKE_BITS_FROM_MASK, ACQ480_ADC_MR_MRSEL0);
MAKE_BITS(mr_10dec,ACQ480_ADC_MULTIRATE, MAKE_BITS_FROM_MASK, ACQ480_ADC_MR_MR10_DEC);
MAKE_BITS(mr_en,   ACQ480_ADC_MULTIRATE, MAKE_BITS_FROM_MASK, ACQ480_ADC_MR_EN);

const struct attribute *acq480_ffir_attrs[] = {
	&dev_attr_ffir_reset.attr,
	&dev_attr_ffir_counter.attr,
	&dev_attr_ffir_coeff.attr,

	&dev_attr_train.attr,
	&dev_attr_train_states.attr,
	&dev_attr_dclock_reset.attr,
	&dev_attr_acq480_train_ctrl.attr,
	&dev_attr_acq480_train_hi_val.attr,
	&dev_attr_acq480_train_lo_val.attr,
	&dev_attr_acq480_loti.attr,
	&dev_attr_acq480_two_lane_mode.attr,
	&dev_attr_acq482_cmap.attr,
	&dev_attr_acq480_fpga_decim.attr,

	&dev_attr_mr_sel1.attr,
	&dev_attr_mr_sel0.attr,
	&dev_attr_mr_10dec.attr,
	&dev_attr_mr_en.attr,
	NULL
};

const struct attribute ** acq480_attrs = (acq480_ffir_attrs+3);
