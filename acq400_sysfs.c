/* ------------------------------------------------------------------------- */
/* acq400_sysfs.c D-TACQ ACQ400 series driver, sysfs (knobs)	             */
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

#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */

#include "lk-shim.h"


#include <linux/device.h>
#include <linux/user.h>

#include "acq400.h"
#include "hbm.h"

#include "acq400_sysfs.h"

#include "sysfs_attrs.h"

#define MODIFY_REG_ACCESS_READ_BEFORE	0x1
#define MODIFY_REG_ACCESS_READ_AFTER	0x2
int modify_reg_access;
module_param(modify_reg_access, int, 0644);
MODULE_PARM_DESC(modify_spad_access, "1: force read before, 2: force read after");

int reset_fifo_verbose;
module_param(reset_fifo_verbose, int, 0644);


int acq480_rtm_translen_offset = 2;
module_param(acq480_rtm_translen_offset, int, 0644);


int TIM_CTRL_LOCK;
module_param(TIM_CTRL_LOCK, int, 0644);
MODULE_PARM_DESC(dds_strobe_msec, "disable usr access when set");

MAKE_TRIPLET(fmc_clk, FMC_DSR, FMC_DSR_CLK_BUS, FMC_DSR_CLK_BUS_DX, FMC_DSR_CLK_DIR);
MAKE_TRIPLET(fmc_trg, FMC_DSR, FMC_DSR_TRG_BUS, FMC_DSR_TRG_BUS_DX, FMC_DSR_TRG_DIR);

static const struct attribute *fmc_fp_attrs[] = {
	&dev_attr_fmc_clk.attr,
	&dev_attr_fmc_trg.attr,
	NULL
};



MAKE_BITS(gate_sync,    ADC_CTRL,      MAKE_BITS_FROM_MASK,	ADC_CTRL_435_GATE_SYNC);
MAKE_BITS(sync_in_clk,  HDMI_SYNC_DAT, HDMI_SYNC_IN_CLKb, 	0x1);
MAKE_BITS(sync_in_sync, HDMI_SYNC_DAT, HDMI_SYNC_IN_SYNCb, 	0x1);
MAKE_BITS(sync_in_trg,  HDMI_SYNC_DAT, HDMI_SYNC_IN_TRGb, 	0x1);
MAKE_BITS(sync_in_gpio, HDMI_SYNC_DAT, HDMI_SYNC_IN_GPIOb, 	0x1);


MAKE_BITS(di4_3, 	HDMI_SYNC_DAT, HDMI_SYNC_IN_CLKb, 	0x1);
MAKE_BITS(di4_2, 	HDMI_SYNC_DAT, HDMI_SYNC_IN_SYNCb, 	0x1);
MAKE_BITS(di4_1, 	HDMI_SYNC_DAT, HDMI_SYNC_IN_TRGb, 	0x1);
MAKE_BITS(di4_0, 	HDMI_SYNC_DAT, HDMI_SYNC_IN_GPIOb, 	0x1);

MAKE_BIT_RON(sync_out_cable_det, HDMI_SYNC_DAT, HDMI_SYNC_OUT_CABLE_DETNb, 0x1);

MAKE_BITS(sync_out_clk,  HDMI_SYNC_DAT, HDMI_SYNC_OUT_CLKb, 	0x1);
MAKE_BITS(sync_out_sync, HDMI_SYNC_DAT, HDMI_SYNC_OUT_SYNCb, 	0x1);
MAKE_BITS(sync_out_trg,  HDMI_SYNC_DAT, HDMI_SYNC_OUT_TRGb, 	0x1);
MAKE_BITS(sync_out_gpio, HDMI_SYNC_DAT, HDMI_SYNC_OUT_GPIOb, 	0x1);


MAKE_BITS(do4_3, 	HDMI_SYNC_DAT, HDMI_SYNC_OUT_CLKb, 	0x1);
MAKE_BITS(do4_2, 	HDMI_SYNC_DAT, HDMI_SYNC_OUT_SYNCb, 	0x1);
MAKE_BITS(do4_1, 	HDMI_SYNC_DAT, HDMI_SYNC_OUT_TRGb, 	0x1);
MAKE_BITS(do4_0, 	HDMI_SYNC_DAT, HDMI_SYNC_OUT_GPIOb, 	0x1);


MAKE_BITS(sync_out_src_sync, HDMI_SYNC_OUT_SRC, HSO_SYNC_SHL+HSO_SS_SEL_SHL, HSO_XX_SEL_MASK);
MAKE_BITS(sync_out_src_trg,  HDMI_SYNC_OUT_SRC, HSO_TRG_SHL+HSO_SS_SEL_SHL,  HSO_XX_SEL_MASK);
MAKE_BITS(sync_out_src_gpio, HDMI_SYNC_OUT_SRC, HSO_GPIO_SHL+HSO_SS_SEL_SHL, HSO_XX_SEL_MASK);
MAKE_BITS(sync_out_src_clk,  HDMI_SYNC_OUT_SRC, HSO_CLK_SHL+HSO_SS_SEL_SHL,  HSO_XX_SEL_MASK);

MAKE_BITS(sync_out_src_sync_dx, HDMI_SYNC_OUT_SRC, HSO_SYNC_SHL, HSO_DX_MASK);
MAKE_BITS(sync_out_src_trg_dx,  HDMI_SYNC_OUT_SRC, HSO_TRG_SHL,  HSO_DX_MASK);
MAKE_BITS(sync_out_src_gpio_dx, HDMI_SYNC_OUT_SRC, HSO_GPIO_SHL, HSO_DX_MASK);
MAKE_BITS(sync_out_src_clk_dx,  HDMI_SYNC_OUT_SRC, HSO_CLK_SHL,  HSO_DX_MASK);

MAKE_BITS(evt_src_d7, EVT_BUS_SRC, EBS_7_SHL, EBS_MASK);
MAKE_BITS(evt_src_d6, EVT_BUS_SRC, EBS_6_SHL, EBS_MASK);
MAKE_BITS(evt_src_d5, EVT_BUS_SRC, EBS_5_SHL, EBS_MASK);
MAKE_BITS(evt_src_d4, EVT_BUS_SRC, EBS_4_SHL, EBS_MASK);
MAKE_BITS(evt_src_d3, EVT_BUS_SRC, EBS_3_SHL, EBS_MASK);
MAKE_BITS(evt_src_d2, EVT_BUS_SRC, EBS_2_SHL, EBS_MASK);
MAKE_BITS(evt_src_d1, EVT_BUS_SRC, EBS_1_SHL, EBS_MASK);
MAKE_BITS(evt_src_d0, EVT_BUS_SRC, EBS_0_SHL, EBS_MASK);

MAKE_BITS(sig_src_route_sync_d1, SIG_SRC_ROUTE, SSR_SYNC_1_SHL, SSR_MASK);
MAKE_BITS(sig_src_route_sync_d0, SIG_SRC_ROUTE, SSR_SYNC_0_SHL, SSR_MASK);
MAKE_BITS(sig_src_route_trg_d1,  SIG_SRC_ROUTE, SSR_TRG_1_SHL,  SSR_MASK);
MAKE_BITS(sig_src_route_trg_d0,  SIG_SRC_ROUTE, SSR_TRG_0_SHL,  SSR_MASK);
MAKE_BITS(sig_src_route_clk_d1,  SIG_SRC_ROUTE, SSR_CLK_1_SHL,  SSR_MASK);
MAKE_BITS(sig_src_route_clk_d0,  SIG_SRC_ROUTE, SSR_CLK_0_SHL,  SSR_MASK);

MAKE_BITS(fpctl_gpio, 		 FPCTL,         FPCTL_GPIO_SHL, FPCTL_MASK);
MAKE_BITS(fpctl_sync,		 FPCTL,         FPCTL_SYNC_SHL, FPCTL_MASK);
MAKE_BITS(fpctl_trg,             FPCTL,         FPCTL_TRG_SHL,  FPCTL_MASK);
MAKE_BITS(zclk_sel, MOD_CON, MCR_ZCLK_SELECT_SHL, MCR_ZCLK_MASK);

MAKE_BITS(adc_nobt, ADC_CTRL, ADC_CTRL_42x_RES_SHL, ADC_CTRL_42x_RES_MASK);

static const struct attribute *hdmi_sync_attrs[] = {
	&dev_attr_sync_in_clk.attr,
	&dev_attr_sync_in_sync.attr,
	&dev_attr_sync_in_trg.attr,
	&dev_attr_sync_in_gpio.attr,

	&dev_attr_di4_3.attr,
	&dev_attr_di4_2.attr,
	&dev_attr_di4_1.attr,
	&dev_attr_di4_0.attr,

	&dev_attr_sync_out_cable_det.attr,

	&dev_attr_sync_out_clk.attr,
	&dev_attr_sync_out_sync.attr,
	&dev_attr_sync_out_trg.attr,
	&dev_attr_sync_out_gpio.attr,

	&dev_attr_do4_3.attr,
	&dev_attr_do4_2.attr,
	&dev_attr_do4_1.attr,
	&dev_attr_do4_0.attr,

	&dev_attr_sync_out_src_sync.attr,
	&dev_attr_sync_out_src_trg.attr,
	&dev_attr_sync_out_src_gpio.attr,
	&dev_attr_sync_out_src_clk.attr,
	&dev_attr_sync_out_src_sync_dx.attr,
	&dev_attr_sync_out_src_trg_dx.attr,
	&dev_attr_sync_out_src_gpio_dx.attr,
	&dev_attr_sync_out_src_clk_dx.attr,

	&dev_attr_evt_src_d7.attr,
	&dev_attr_evt_src_d6.attr,
	&dev_attr_evt_src_d5.attr,
	&dev_attr_evt_src_d4.attr,
	&dev_attr_evt_src_d3.attr,
	&dev_attr_evt_src_d2.attr,
	&dev_attr_evt_src_d1.attr,
	&dev_attr_evt_src_d0.attr,

	&dev_attr_sig_src_route_sync_d1.attr,
	&dev_attr_sig_src_route_sync_d0.attr,
	&dev_attr_sig_src_route_trg_d1.attr,
	&dev_attr_sig_src_route_trg_d0.attr,
	&dev_attr_sig_src_route_clk_d1.attr,
	&dev_attr_sig_src_route_clk_d0.attr,

	&dev_attr_fpctl_gpio.attr,
	&dev_attr_fpctl_sync.attr,
	&dev_attr_fpctl_trg.attr,

	&dev_attr_zclk_sel.attr,
	NULL
};
static ssize_t show_clkdiv(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 clkdiv = acq400rd32(acq400_devices[dev->id], ADC_CLKDIV);
	return sprintf(buf, "%u\n", clkdiv);
}

static ssize_t store_clkdiv(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 clkdiv;

	if (sscanf(buf, "%u", &clkdiv) == 1 &&
	    clkdiv >= 1 && clkdiv <= adev->clkdiv_mask){
		acq400wr32(adev, ADC_CLKDIV, clkdiv);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(clkdiv, S_IRUGO|S_IWUSR, show_clkdiv, store_clkdiv);

static ssize_t show_adc_conv_time(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 adc_conv_time = acq400rd32(acq400_devices[dev->id], ADC_CONV_TIME);
	return sprintf(buf, "%u\n", adc_conv_time);
}

static ssize_t store_adc_conv_time(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 adc_conv_time;
	if (sscanf(buf, "%u", &adc_conv_time) == 1 &&
	    adc_conv_time >= 1 &&
	    adc_conv_time <= ADC_CLK_DIV_MASK){
		acq400wr32(acq400_devices[dev->id], ADC_CONV_TIME, adc_conv_time);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(adc_conv_time, S_IRUGO|S_IWUSR, show_adc_conv_time, store_adc_conv_time);

static ssize_t show_clk_min_max(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 cmm = acq400rd32(acq400_devices[dev->id], ACQ424_CLK_MIN_MAX);
	return sprintf(buf, "%u %u\n", cmm>>16, cmm&0x0ffff);
}

static DEVICE_ATTR(clk_min_max, S_IRUGO, show_clk_min_max, 0);


unsigned get_gain(u32 gains, int chan)
{
	return (gains >> chan*2) & 0x3;
}

unsigned set_gain(u32 gains, int chan, u32 value)
{
	u32 mask = 0x3 << chan*2;
	gains &= ~mask;
	gains |= (value&0x3) << chan*2;
	return gains;
}
static ssize_t show_gains(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 gains = acq400rd32(acq400_devices[dev->id], ADC_GAIN);
	return sprintf(buf, "%u%u%u%u\n",
			get_gain(gains, 0), get_gain(gains, 1),
			get_gain(gains, 2), get_gain(gains, 3));
}

static ssize_t store_gains(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	char gx[4];

	if (sscanf(buf, "%c%c%c%c", gx+0, gx+1, gx+2, gx+3) == 4){
		u32 gains = acq400rd32(acq400_devices[dev->id], ADC_GAIN);
		int ic;
		for (ic = 0; ic < 4; ++ic){
			if (gx[ic] >= '0' && gx[ic] <= '3'){
				gains = set_gain(gains, ic, gx[ic]-'0');
			}else{
				switch(gx[ic]) {
				case '-':
				case 'x':
				case 'X':
					break;
				default:
					dev_warn(dev, "bad value at %c at %d", gx[ic], ic);
				}
			}
		}
		dev_dbg(dev, "set gains: %02x", gains);
		acq400wr32(acq400_devices[dev->id], ADC_GAIN, gains);
		return count;
	}else{
		dev_warn(dev, "rejecting input args != 4");
		return -1;
	}
}

static DEVICE_ATTR(gains, S_IRUGO|S_IWUSR, show_gains, store_gains);


static ssize_t show_gain(
	int chan,
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 gains = acq400rd32(acq400_devices[dev->id], ADC_GAIN);

	return sprintf(buf, "%u\n", get_gain(gains, chan));
}

static ssize_t store_gain(
		int chan,
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count)
{
	char gx;

	if (sscanf(buf, "%c", &gx) == 1){
		u32 gains = acq400rd32(acq400_devices[dev->id], ADC_GAIN);
		if (gx >= '0' && gx <= '3'){
			gains = set_gain(gains, chan, gx-'0');
		}else{
			switch(gx) {
			case '-':
			case 'x':
			case 'X':
				break;
			default:
				dev_warn(dev, "bad value at %c at %d", gx, chan);
			}
		}

		dev_dbg(dev, "set gain: %02x", gains);
		acq400wr32(acq400_devices[dev->id], ADC_GAIN, gains);
		return count;
	}else{
		dev_warn(dev, "rejecting input args != 4");
		return -1;
	}
}

#define MAKE_GAIN(CH)							\
static ssize_t show_gain##CH(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_gain(CH - 1, dev, attr, buf);			\
}									\
									\
static ssize_t store_gain##CH(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_gain(CH - 1, dev, attr, buf, count);		\
}									\
static DEVICE_ATTR(gain##CH, S_IRUGO|S_IWUSR, show_gain##CH, store_gain##CH)

MAKE_GAIN(1);
MAKE_GAIN(2);
MAKE_GAIN(3);
MAKE_GAIN(4);

static ssize_t show_signal(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	int shl, int mbit,
	const char*signame, const char* mbit_hi, const char* mbit_lo)
{
	u32 adc_ctrl = acq400rd32(acq400_devices[dev->id], REG);
	if (adc_ctrl&mbit){
		u32 sel = (adc_ctrl >> shl) & CTRL_SIG_MASK;
		unsigned dx = sel&CTRL_SIG_SEL;
		int rising = ((adc_ctrl >> shl) & CTRL_SIG_RISING) != 0;

		return sprintf(buf, "%s=%d,%d,%d %s d%u %s\n",
				signame, (adc_ctrl&mbit)>>getSHL(mbit), dx, rising,
				mbit_hi, dx, rising? "RISING": "FALLING");
	}else{
		return sprintf(buf, "%s=0,0,0 %s\n", signame, mbit_lo);
	}
}

int store_signal3(struct acq400_dev* adev,
		unsigned REG,
		int shl, int mbit,
		unsigned imode, unsigned dx, unsigned rising)
{
	u32 adc_ctrl = acq400rd32(adev, REG);

	adc_ctrl &= ~mbit;
	if (imode != 0){
		if (dx > 7){
			dev_warn(DEVP(adev), "rejecting \"%u\" dx > 7", dx);
			return -1;
		}
		adc_ctrl &= ~(CTRL_SIG_MASK << shl);
		adc_ctrl |=  (dx|(rising? CTRL_SIG_RISING:0))<<shl;
		adc_ctrl |= imode << getSHL(mbit);
	}
	acq400wr32(adev, REG, adc_ctrl);
	return 0;

}

int lock_action(struct device * dev, const char* buf)
{
	wait_queue_head_t _waitq;

	dev_warn(dev, "pid %d attempted to write to TIM_CTRL with LOCK ON: \"%s\"", current->pid, buf);

	init_waitqueue_head(&_waitq);
	if (TIM_CTRL_LOCK > 1){
		wait_event_interruptible_timeout(_waitq, 0, TIM_CTRL_LOCK);
		dev_warn(dev, "pid %d return", current->pid);
	}
	return -EBUSY;
}
static ssize_t store_signal(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		int shl, int mbit, const char* mbit_hi, const char* mbit_lo,
		int not_while_busy)
{
	char sense;
	char mode[16];
	unsigned imode, dx, rising;
	struct acq400_dev* adev = acq400_devices[dev->id];
	int nscan;

	if (not_while_busy && adev->busy){
		return -EBUSY;
	}

	if (REG == TIM_CTRL && shl == TIM_CTRL_TRIG_SHL && TIM_CTRL_LOCK){
		return lock_action(dev, buf);

	}
	/* first form: imode,dx,rising : easiest with auto eg StreamDevice */
	nscan = sscanf(buf, "%u,%u,%u", &imode, &dx, &rising);
	if (nscan == 3){
		if (store_signal3(adev, REG, shl, mbit, imode, dx, rising)){
			return -1;
		}else{
			return count;
		}
	}

	/* second form: mode dDX sense : better for human scripting */
	nscan = sscanf(buf, "%10s d%u %c", mode, &dx, &sense);

	switch(nscan){
	case 1:
		if (strcmp(mode, mbit_lo) == 0){
			return store_signal3(adev, REG, shl, mbit, 0, 0, 0);
		}
		dev_warn(dev, "single arg must be:\"%s\"", mbit_lo);
		return -1;
	case 3:
		if (strcmp(mode, mbit_hi) == 0){
			unsigned falling = strchr("Ff-Nn", sense) != NULL;
			rising 	= strchr("Rr+Pp", sense) != NULL;

			if (!rising && !falling){
				dev_warn(dev,
				 "rejecting \"%s\" sense must be R or F", buf);
				return -1;
			}else if (store_signal3(adev, REG, shl, mbit, 1, dx, rising)){
				return -1;
			}else{
				return count;
			}
		}
		/* else fall thru */
	default:
		dev_warn(dev, "%s|%s dX R|F", mbit_lo, mbit_hi);
		return -1;
	}
}

#define MAKE_SIGNAL(SIGNAME, REG, shl, mbit, HI, LO, NWB)		\
static ssize_t show_##SIGNAME(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_signal(dev, attr, buf, REG, shl, mbit, #SIGNAME, HI, LO);\
}									\
									\
static ssize_t store_##SIGNAME(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_signal(dev, attr, buf, count, REG, shl, mbit, HI, LO, NWB);\
}									\
static DEVICE_ATTR(SIGNAME, S_IRUGO|S_IWUSR, 				\
		show_##SIGNAME, store_##SIGNAME)

#define ENA	"enable"
#define DIS	"disable"
#define EXT	"external"
#define SOFT	"soft"
#define INT	"internal"
MAKE_SIGNAL(event1, TIM_CTRL, TIM_CTRL_EVENT1_SHL, TIM_CTRL_MODE_EV1_EN, ENA, DIS,	0);
MAKE_SIGNAL(event0, TIM_CTRL, TIM_CTRL_EVENT0_SHL, TIM_CTRL_MODE_EV0_EN, ENA, DIS,	0);
MAKE_SIGNAL(trg,    TIM_CTRL, TIM_CTRL_TRIG_SHL,   TIM_CTRL_MODE_HW_TRG_EN, EXT, DIS,	1);
MAKE_SIGNAL(clk,    TIM_CTRL, TIM_CTRL_CLK_SHL,	   TIM_CTRL_MODE_HW_CLK, EXT, INT,	1);
MAKE_SIGNAL(sync,   TIM_CTRL, TIM_CTRL_SYNC_SHL,   TIM_CTRL_MODE_SYNC,   EXT, INT,	1);


MAKE_SIGNAL(gpg_trg, GPG_CONTROL, GPG_CTRL_TRG_SHL, GPG_CTRL_EXT_TRG, EXT, SOFT,	1);
MAKE_SIGNAL(gpg_clk, GPG_CONTROL, GPG_CTRL_CLK_SHL, GPG_CTRL_EXTCLK,  EXT, INT,		1);
MAKE_SIGNAL(gpg_sync,GPG_CONTROL, GPG_CTRL_SYNC_SHL, GPG_CTRL_EXT_SYNC, EXT, SOFT,	1);

/* MUST set RGM as well */
MAKE_SIGNAL(rgm, ADC_CTRL, ADC_CTRL_RGM_GATE_SHL,
		ADC_CTRL_RGM_MODE_MASK<<ADC_CTRL_RGM_MODE_SHL, ENA, DIS, 1);



static ssize_t show_gpg_top_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 gpg_ctrl = acq400rd32(adev, GPG_CONTROL);
	u32 gpg_top = (gpg_ctrl&GPG_CTRL_TOPADDR) >> GPG_CTRL_TOPADDR_SHL;
	return sprintf(buf, "%u\n", gpg_top+2);
}

static ssize_t store_gpg_top_count(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 gpg_top;

	if (sscanf(buf, "%u", &gpg_top) == 1){
		set_gpg_top(adev, gpg_top);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(gpg_top_count, S_IRUGO|S_IWUSR, show_gpg_top_count, store_gpg_top_count);


static ssize_t show_gpg_debug(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
/* state, addr, ctr, ostate, until */
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	u32 deb = acq400rd32(adev, GPG_DEBUG);
	u32 state = getField(deb, GPG_DBG_STATE);
	u32 addr = getField(deb, GPG_DBG_ADDR);
	u32 ctr = getField(deb, GPG_DBG_CTR);
	u32 gpd = ((u32*)sc_dev->gpg_buffer)[addr];
	u32 ostate = gpd&0x000000ff;

	return sprintf(buf, "%u,%u,%u,%u,%x,%u\n",
			deb, addr, ctr, state, ostate, gpd>>8);
}

static DEVICE_ATTR(gpg_debug, S_IRUGO, show_gpg_debug, 0);


static ssize_t show_axi_freq(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 clk = acq400rd32(adev, SYS_CLK) >> 16;

	if (clk != 0) clk = clk+1;

	return sprintf(buf, "%u\n", clk*100*100/1000000);
}

static DEVICE_ATTR(axi_freq, S_IRUGO, show_axi_freq, 0);

static ssize_t show_wr_tai_cur(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 tl = acq400rd32(adev, WR_TAI_CUR_L);
	u32 th = acq400rd32(adev, WR_TAI_CUR_H);

	unsigned long long tai = th;
	tai = tai << 32 | tl;

	return sprintf(buf, "%llu\n", tai);
}

static DEVICE_ATTR(wr_tai_cur, S_IRUGO, show_wr_tai_cur, 0);

static ssize_t store_wr_tai_trg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 tai_trg;
	u32 tai_sec, clocks;

	if (sscanf(buf, "%u,%u", &tai_sec, &clocks) == 2){
		tai_trg = tai_sec << 28 | clocks;
		if (tai_trg != 0){
			tai_trg |= WR_TAI_TRG_EN;
		}
		acq400wr32(adev, WR_TAI_TRG, tai_trg);
		return count;
	} else if (sscanf(buf, "%u", &tai_trg) == 1){
		if (tai_trg != 0){
			tai_trg |= WR_TAI_TRG_EN;
		}
		acq400wr32(adev, WR_TAI_TRG, tai_trg);
		return count;
	}else{
		return -1;
	}
}

static ssize_t show_wr_tai_trg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	unsigned tai_trg = acq400rd32(adev, WR_TAI_TRG);
	return sprintf(buf, "0x%08x %u %u\n", tai_trg, (tai_trg>>28)&0x7, tai_trg&0x0fffffff);
}

static DEVICE_ATTR(wr_tai_trg, S_IRUGO|S_IWUSR, show_wr_tai_trg, store_wr_tai_trg);


static ssize_t show_wr_stamp(
	struct device * dev,
	struct device_attribute *attr,
	char * buf, u32 reg)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	unsigned tai_stamp = acq400rd32(adev, WR_TAI_STAMP);
	return sprintf(buf, "0x%08x %u %u\n", tai_stamp, (tai_stamp>>28)&0x7, tai_stamp&0x0fffffff);
}
static ssize_t show_wr_tai_stamp(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return show_wr_stamp(dev, attr, buf, WR_TAI_STAMP);
}

static DEVICE_ATTR(wr_tai_stamp, S_IRUGO, show_wr_tai_stamp, 0);

static ssize_t show_wr_cur_vernier(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return show_wr_stamp(dev, attr, buf, WR_CUR_VERNR);
}

static DEVICE_ATTR(wr_cur_vernier, S_IRUGO, show_wr_cur_vernier, 0);

MAKE_BITS(wr_clk_pv, WR_CLK_GEN, MAKE_BITS_FROM_MASK, WR_CLK_GEN_PV);
MAKE_BITS(wr_clk_pv3, WR_CLK_GEN, MAKE_BITS_FROM_MASK, WR_CLK_GEN_PV3);


MAKE_SIGNAL(wr_trg_src, WR_CTRL, WR_CTRL_TRG_SRC_SHL, WR_CTRL_TS_INTEN, ENA, DIS, 1);

#define MAKE_WR_EVENT_COUNT(FUN) 								\
static ssize_t store_wr_##FUN##count(								\
	struct device * dev,									\
	struct device_attribute *attr,								\
	const char * buf,									\
	size_t count)										\
{												\
	struct acq400_dev* adev = acq400_devices[dev->id];					\
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);		\
	unsigned clear;										\
	if (sscanf(buf, "%u", &clear) == 1 && clear==1){					\
		sc_dev->FUN.wc_count = 0;							\
		return count;									\
	}else{											\
		return -1;									\
	}											\
}												\
static ssize_t show_wr_##FUN##count(								\
	struct device * dev,									\
	struct device_attribute *attr,								\
	char * buf)										\
{												\
	struct acq400_dev* adev = acq400_devices[dev->id];					\
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);		\
	return sprintf(buf, "%u\n", sc_dev->FUN.wc_count);					\
}												\
static DEVICE_ATTR(wr_##FUN##_count, S_IRUGO|S_IWUSR, show_wr_##FUN##count, store_wr_##FUN##count)

MAKE_WR_EVENT_COUNT(pps_client);
MAKE_WR_EVENT_COUNT(ts_client);
MAKE_WR_EVENT_COUNT(wrtt_client);

static const struct attribute *acq2106_wr_attrs[] = {
	&dev_attr_wr_clk_pv.attr,
	&dev_attr_wr_clk_pv3.attr,
	&dev_attr_wr_trg_src.attr,
	&dev_attr_wr_tai_cur.attr,
	&dev_attr_wr_tai_trg.attr,
	&dev_attr_wr_tai_stamp.attr,
	&dev_attr_wr_cur_vernier.attr,
	&dev_attr_wr_pps_client_count.attr,
	&dev_attr_wr_ts_client_count.attr,
	&dev_attr_wr_wrtt_client_count.attr,
	NULL
};



static ssize_t show_gpg_mode(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];

	u32 gpg_ctrl = acq400rd32(adev, GPG_CONTROL);
	u32 gpg_mode = (gpg_ctrl&GPG_CTRL_MODE) >> GPG_CTRL_MODE_SHL;
	return sprintf(buf, "%u\n", gpg_mode);
}

static ssize_t store_gpg_mode(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 gpg_mode;

	if (sscanf(buf, "%u", &gpg_mode) == 1){
		u32 gpg_ctrl = acq400rd32(adev, GPG_CONTROL);
		gpg_mode <<= GPG_CTRL_MODE_SHL;
		gpg_mode &= GPG_CTRL_MODE;
		gpg_ctrl &= ~GPG_CTRL_MODE;
		gpg_ctrl |= gpg_mode;

		acq400wr32(adev, GPG_CONTROL, gpg_ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(gpg_mode, S_IRUGO|S_IWUSR, show_gpg_mode, store_gpg_mode);

static ssize_t show_gpg_enable(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 gpg_ctrl = acq400rd32(adev, GPG_CONTROL);
	return sprintf(buf, "%u\n", (gpg_ctrl&GPG_CTRL_ENABLE) != 0);
}

static ssize_t store_gpg_enable(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 gpg_enable;

	if (sscanf(buf, "%u", &gpg_enable) == 1){
		u32 gpg_ctrl = acq400rd32(adev, GPG_CONTROL);
		if (gpg_enable){
			gpg_ctrl |= GPG_CTRL_ENABLE;
		}else{
			gpg_ctrl &= ~GPG_CTRL_ENABLE;
		}
		acq400wr32(adev, GPG_CONTROL, gpg_ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(gpg_enable, S_IRUGO|S_IWUSR, show_gpg_enable, store_gpg_enable);

static ssize_t show_simulate(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 simulate = acq400_devices[dev->id]->ramp_en != 0;
	return sprintf(buf, "%u\n", simulate);
}

static ssize_t store_simulate(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 simulate;
	if (sscanf(buf, "%u", &simulate) == 1){
		acq400_devices[dev->id]->ramp_en = simulate != 0;
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(simulate, S_IRUGO|S_IWUSR, show_simulate, store_simulate);


static ssize_t show_event0_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->rt.event_count);
}

static DEVICE_ATTR(event0_count, S_IRUGO, show_event0_count, 0);

static ssize_t show_spad(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	return sprintf(buf, "%u,%u,%u\n",
			sc_dev->spad.spad_en,
			sc_dev->spad.spad_en==SP_EN? sc_dev->spad.len: 0,
			sc_dev->spad.spad_en!=SP_OFF? sc_dev->spad.diX: 0);
}

static ssize_t store_spad(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct Spad spad = { 1, 8, 0 };
	if (sscanf(buf, "%u,%u,%u", &spad.spad_en, &spad.len, &spad.diX) > 0){
		if (spad.diX > SD_DI32) spad.diX = SD_SEW;
		if (spad.len > AGG_SPAD_LEN) spad.len = AGG_SPAD_LEN;
		switch(spad.spad_en){
		default:
			spad.spad_en = SP_OFF;	/* fall thru */
		case SP_OFF:
			spad.len = 0; spad.diX = SD_SEW; break;
			break;
		case SP_EN:
			if (spad.len < 1) spad.len = 8;
			break;
		case SP_FRAME:
			spad.len = 0;
			break;
		}
		sc_dev->spad = spad;
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(spad, S_IRUGO|S_IWUSR, show_spad, store_spad);

static ssize_t show_spadN(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int ix)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 spad = get_spadN(adev, ix);
	return sprintf(buf, "0x%08x\n", spad);
}

static ssize_t store_spadN(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int ix)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	int rc = count;
	u32 spad;
	enum { ASSIGN, SETBITS, CLRBITS } mode;


	if (sscanf(buf, "+%x", &spad) == 1){
		mode = SETBITS;
	}else if (sscanf(buf, "-%x", &spad) == 1){
		mode = CLRBITS;
	}else if (sscanf(buf, "%x", &spad) == 1){
		mode = ASSIGN;
	}else{
		return -1;
	}

	if (mutex_lock_interruptible(&adev->mutex)) {
		return -EINTR;
	}
	if (mode != ASSIGN){
		u32 rspad = get_spadN(adev, ix);
		switch(mode){
		case SETBITS:
			rspad |= spad; spad = rspad; break;
		case CLRBITS:
			rspad &= ~spad; spad = rspad; break;
		default:
			dev_err(DEVP(adev), "BAD mode %s %d", __FILE__, __LINE__);
			goto store_spadN_99;
		}
	}
	set_spadN(adev, ix, spad);

store_spadN_99:
	mutex_unlock(&adev->mutex);
	return rc;
}


#define MAKE_SPAD(IX)							\
static ssize_t show_spad##IX(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_spadN(dev, attr, buf, IX);				\
}									\
									\
static ssize_t store_spad##IX(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_spadN(dev, attr, buf, count, IX);			\
}									\
static DEVICE_ATTR(spad##IX, S_IRUGO|S_IWUSR, 				\
		show_spad##IX, store_spad##IX)

MAKE_SPAD(0);
MAKE_SPAD(1);
MAKE_SPAD(2);
MAKE_SPAD(3);
MAKE_SPAD(4);
MAKE_SPAD(5);
MAKE_SPAD(6);
MAKE_SPAD(7);


MAKE_SIGNAL(spad1_us,    USEC_CCR, USEC_CCR_SRC_SHL,   USEC_CCR_EN, ENA, DIS,	1);

static ssize_t show_reg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int reg_off,
	const char* fmt,
	int ff)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 reg = acq400rd32(adev, reg_off);
	return sprintf(buf, fmt, reg+ff);
}

static ssize_t store_reg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int reg_off,
	int ff)
{
	u32 reg;
	if (sscanf(buf, "0x%x", &reg) == 1 || sscanf(buf, "%u", &reg) == 1){
		struct acq400_dev* adev = acq400_devices[dev->id];
		if (reg > ff){
			reg -= ff;
		}else{
			reg = 0;
		}
		if ((modify_reg_access&MODIFY_REG_ACCESS_READ_BEFORE) != 0){
			acq400rd32(adev, reg_off);
		}
		acq400wr32(adev, reg_off, reg);
		if ((modify_reg_access&MODIFY_REG_ACCESS_READ_AFTER) != 0){
			u32 regr = acq400rd32(adev, reg_off);
			if (regr != reg){
				dev_warn(DEVP(adev),
					"reg:%04x mismatch w:%08x r:%08x",
					reg_off, reg, regr);
			}
		}
		return count;
	}else{
		return -1;
	}
}


#define MAKE_REG_RO(kname, REG, FMT)					\
static ssize_t show_reg_##REG(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_reg(dev, attr, buf, REG, FMT, 0);			\
}									\
									\
static DEVICE_ATTR(kname, S_IRUGO, show_reg_##REG, 0)

#define MAKE_REG_RW(kname, REG, FMT)					\
static ssize_t show_reg_##REG(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_reg(dev, attr, buf, REG, FMT, 0);			\
}									\
									\
static ssize_t store_reg_##REG(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_reg(dev, attr, buf, count, REG, 0);	\
}									\
static DEVICE_ATTR(kname, S_IRUGO|S_IWUSR, show_reg_##REG, store_reg_##REG)

MAKE_REG_RW(sw_emb_word1, ACQ435_SW_EMB_WORD1, "0x%08x\n");
MAKE_REG_RW(sw_emb_word2, ACQ435_SW_EMB_WORD2, "0x%08x\n");
MAKE_REG_RO(evt_sc_latch, EVT_SC_LATCH,	"%u\n");

static ssize_t show_reg_rtm_translen(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	int ff = IS_ACQ480(adev)? acq480_rtm_translen_offset: 0;

	return show_reg(dev, attr, buf, ADC_TRANSLEN, "%u\n", ff);
}

static ssize_t store_reg_rtm_translen(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	int ff = IS_ACQ480(adev)? acq480_rtm_translen_offset: 0;

	return store_reg(dev, attr, buf, count, ADC_TRANSLEN, ff);
}
static DEVICE_ATTR(rtm_translen, S_IRUGO|S_IWUSR,
		show_reg_rtm_translen, store_reg_rtm_translen);



static ssize_t show_nbuffers(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->nbuffers);
}

static DEVICE_ATTR(nbuffers, S_IRUGO, show_nbuffers, 0);

int lcm(int x, int y)
{
	int n1 = x;
	int n2 = y;

	while(n1!=n2){
		if(n1>n2){
			n1=n1-n2;
		}else{
			n2=n2-n1;
		}
	}
	return x*y/n1;
}

extern int bufferlen;




/* buffer len MUST be modulo PAGE_SIZE because we're going to mmap it.
 * For AXI, buffer len MUST be modulo AXI_DMA_BLOCK, since
 * AXI_DMA_BLOCK is a factor of PAGE_SIZE, this doesn't affect the bufferlen
 * calculation, but it is needed to set the transfer #blocks in the FPGA.
 */

static ssize_t _store_optimise_bufferlen(
	struct device * dev,
	u32 sample_size,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int mindma = lcm(sample_size, PAGE_SIZE);
	int spb = bufferlen/mindma;
	int newbl = spb*mindma;

	dev_dbg(DEVP(adev), "store_optimise_bufferlen ss:%d mindma:0x%x spb:%d len:0x%x",
			sample_size, mindma, spb, newbl);

	acq400_set_bufferlen(adev, newbl);

	if (IS_ACQ480(acq400_devices[1])){
		acq400_set_AXI_DMA_len(adev, newbl);
	}
	return count;
}
static ssize_t store_optimise_bufferlen(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 sample_size;
	u32 burst_len = 1;
	if (sscanf(buf, "%u %u", &sample_size, &burst_len) >= 1){
		if (sample_size == 0){
			sample_size = 64;	/* pick a sensible default */
		}
		if (burst_len > 1){
			if (burst_len * sample_size < bufferlen){
				sample_size = burst_len * sample_size;
			}
			/* else .. not going to fit, don't even try */
		}
		return _store_optimise_bufferlen(dev, sample_size, count);
	}

	return -1;
}

static DEVICE_ATTR(optimise_bufferlen, S_IWUSR, 0, store_optimise_bufferlen);
static ssize_t store_bufferlen(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 _bufferlen;
	if (sscanf(buf, "%u", &_bufferlen) == 1){
		struct acq400_dev *adev = acq400_devices[dev->id];

		if (acq400_set_bufferlen(adev, _bufferlen) == _bufferlen){
			return count;
		}
	}

	return -1;
}
static ssize_t show_bufferlen(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->bufferlen);
}

static DEVICE_ATTR(bufferlen, S_IRUGO|S_IWUSR, show_bufferlen, store_bufferlen);


static ssize_t store_dist_bufferlen(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 _bufferlen;
	if (sscanf(buf, "%u", &_bufferlen) == 1){
		struct acq400_dev *adev = acq400_devices[dev->id];

		if (acq400_set_dist_bufferlen(adev, _bufferlen) == _bufferlen){
			return count;
		}
	}

	return -1;
}
static ssize_t show_dist_bufferlen(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", acq400_get_dist_bufferlen(adev));
}

static DEVICE_ATTR(dist_bufferlen, S_IRUGO|S_IWUSR, show_dist_bufferlen, store_dist_bufferlen);

static ssize_t store_AXI_DMA_len(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 dma_len;
	if (sscanf(buf, "%u", &dma_len) == 1){
		struct acq400_dev *adev = acq400_devices[dev->id];

		if (acq400_set_AXI_DMA_len(adev, dma_len) == dma_len){
			return count;
		}
	}

	return -1;
}
static ssize_t show_AXI_DMA_len(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", acq400_get_AXI_DMA_len(adev));
}

static DEVICE_ATTR(AXI_DMA_len, S_IRUGO|S_IWUSR, show_AXI_DMA_len, store_AXI_DMA_len);

static ssize_t show_hitide(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->hitide);
}

static DEVICE_ATTR(hitide, S_IRUGO, show_hitide, 0);

static ssize_t show_lotide(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->lotide);
}

static ssize_t store_lotide(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 lotide;
	if (sscanf(buf, "%u", &lotide) == 1){
		unsigned reg = IS_DIO432X(adev)? DIO432_DO_LOTIDE: DAC_LOTIDE;
		adev->lotide = lotide;
		acq400wr32(adev, reg, adev->lotide);
		return count;
	}else{
		return -1;
	}
}
static DEVICE_ATTR(lotide, S_IRUGO|S_IWUSR, show_lotide, store_lotide);


static ssize_t show_adc_18b(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->adc_18b);
}

static ssize_t store_adc_18b(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 adc_18b;
	if (!IS_ACQ43X(adev) && sscanf(buf, "%u", &adc_18b) == 1){
		adev->adc_18b = adc_18b != 0;
		adev->word_size = adc_18b? 4: 2;
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(adc_18b, S_IRUGO|S_IWUSR, show_adc_18b, store_adc_18b);

static ssize_t show_data32(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->data32);
}

static ssize_t store_data32(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 data32;
	if (!IS_ACQ43X(adev) && sscanf(buf, "%u", &data32) == 1){
		adev->data32 = data32 != 0;
		adev->word_size = data32? 4: 2;
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(data32, S_IRUGO|S_IWUSR, show_data32, store_data32);

static ssize_t show_bitslice_frame(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, ADC_CTRL);
	return sprintf(buf, "%u\n", (ctrl&ADC_CTRL_435_EMBED_STR) != 0);
}

static ssize_t store_bitslice_frame(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 bitslice_frame;
	if (sscanf(buf, "%u", &bitslice_frame) == 1){
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		if (bitslice_frame){
			ctrl |= ADC_CTRL_435_EMBED_STR;
		}else{
			ctrl &= ~ADC_CTRL_435_EMBED_STR;
		}
		acq400wr32(adev, ADC_CTRL, ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(bitslice_frame, S_IRUGO|S_IWUSR, show_bitslice_frame, store_bitslice_frame);


static ssize_t show_stats(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct STATS *stats = &acq400_devices[dev->id]->stats;
	return sprintf(buf, "ints=%u fifo_ints=%u dma_transactions=%u fifo_errs=%d\n",
			stats->interrupts,
			stats->fifo_interrupts, stats->dma_transactions,
			stats->fifo_errors);
}

static ssize_t store_stats(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct STATS *stats = &acq400_devices[dev->id]->stats;
	u32 clr;

	if (sscanf(buf, "%u", &clr) == 1){
		if (clr){
			memset(stats, 0, sizeof(struct STATS));
		}
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(stats, S_IRUGO|S_IWUSR, show_stats, store_stats);

static ssize_t show_shot(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->stats.shot);
}

static ssize_t store_shot(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 set;

	if (sscanf(buf, "%u", &set) == 1){
		adev->stats.shot = set;
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(shot, S_IRUGO|S_IWUSR, show_shot, store_shot);


static ssize_t store_completed_shot(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 set;

	if (sscanf(buf, "%u", &set) == 1){
		adev->stats.completed_shot = set;
		return count;
	}else{
		return -1;
	}
}
static ssize_t show_completed_shot(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->stats.completed_shot);
}



static DEVICE_ATTR(completed_shot, S_IRUGO|S_IWUSR, show_completed_shot, store_completed_shot);

static ssize_t show_run(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->stats.run);
}


static DEVICE_ATTR(run, S_IRUGO, show_run, 0);


static ssize_t show_clk_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 counter = adev->reg_cache.data[ADC_CLK_CTR/sizeof(int)];
	return sprintf(buf, "%u\n", counter&ADC_SAMPLE_CTR_MASK);
}

static DEVICE_ATTR(clk_count, S_IRUGO, show_clk_count, 0);


static ssize_t show_sample_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 count = adev->reg_cache.data[ADC_SAMPLE_CTR/sizeof(int)];
	return sprintf(buf, "%u\n", count&ADC_SAMPLE_CTR_MASK);
}

static DEVICE_ATTR(sample_count, S_IRUGO, show_sample_count, 0);



static ssize_t show_clk_counter_src(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 counter = acq400rd32(acq400_devices[dev->id], ADC_CLK_CTR);
	return sprintf(buf, "%u\n", counter>>ADC_CLK_CTR_SRC_SHL);
}

static ssize_t store_clk_counter_src(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	u32 clk_counter_src;
	if (sscanf(buf, "%u", &clk_counter_src) == 1){
		clk_counter_src &= ADC_CLK_CTR_SRC_MASK;
		acq400wr32(acq400_devices[dev->id], ADC_CLK_CTR,
				clk_counter_src << ADC_CLK_CTR_SRC_SHL);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(clk_counter_src,
		S_IRUGO|S_IWUSR, show_clk_counter_src, store_clk_counter_src);


static ssize_t show_hi_res_mode(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 mode = acq400rd32(adev, ACQ435_MODE);
	return sprintf(buf, "%u\n", (mode&ACQ435_MODE_HIRES_512)? 1: 0);
}

static ssize_t store_hi_res_mode(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 hi_res_mode;
	if (sscanf(buf, "%u", &hi_res_mode) == 1){
		u32 mode = acq400rd32(adev, ACQ435_MODE);
		if (hi_res_mode == 0){
			mode &= ~ACQ435_MODE_HIRES_512;
		}else{
			mode |= ACQ435_MODE_HIRES_512;
		}
		hi_res_mode &= ADC_CLK_CTR_SRC_MASK;
		dev_dbg(DEVP(adev), "store_hi_res_mode %d 0x%08x",
				hi_res_mode!=0, mode);
		acq400wr32(adev, ACQ435_MODE, mode);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(hi_res_mode,
		S_IRUGO|S_IWUSR, show_hi_res_mode, store_hi_res_mode);

/** NB inverted to 1: enabled */

#define BADBANKMASK 0xffffffff

unsigned bank_mask_txt2int(const char* txt)
{
	const char* cursor;
	unsigned mask = 0;

	for (cursor = txt; *cursor && cursor-txt < 4; ++cursor){
		switch(*cursor){
		case 'A':	mask |= 1<<0; break;
		case 'B':	mask |= 1<<1; break;
		case 'C':	mask |= 1<<2; break;
		case 'D':	mask |= 1<<3; break;
		case '\n':
		case '\r':
				return mask;
		default:	return BADBANKMASK;
		}
	}
	return mask;
}

char* bank_mask_int2txt(unsigned mask, char txt[])
{
	char* pt = txt;
	int bit = 0;
	for (bit = 0; bit < 4; ++bit, mask >>= 1){
		if ((mask&1) != 0){
			switch(bit){
			case 0:		*pt++ = 'A'; break;
			case 1:		*pt++ = 'B'; break;
			case 2:		*pt++ = 'C'; break;
			case 3:		*pt++ = 'D'; break;
			}
		}
	}
	*pt++ ='\0';
	return txt;
}
static ssize_t show_bank_mask(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 mask = ~acq400rd32(adev, ACQ435_MODE) & ACQ435_MODE_BXDIS;
	char txt[8];
	return sprintf(buf, "%s\n",  bank_mask_int2txt(mask, txt));
}

unsigned nbits(unsigned mask)
{
	unsigned nb;
	for(nb = 0; mask != 0; mask >>= 1){
		if (mask&1){
			++nb;
		}
	}

	return nb;
}

static ssize_t store_bank_mask(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
/* user mask: 1=enabled. Compute nchan_enabled BEFORE inverting MASK */
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 bank_mask = bank_mask_txt2int(buf);
	if (bank_mask != BADBANKMASK){
		u32 mode = acq400rd32(adev, ACQ435_MODE);

		mode &= ~ACQ435_MODE_BXDIS;
		adev->nchan_enabled = CHANNELS_PER_BANK(adev) * nbits(bank_mask);
		bank_mask = ~bank_mask&ACQ435_MODE_BXDIS;
		mode |= bank_mask;

		acq400wr32(adev, ACQ435_MODE, mode);
		if (IS_AO424(adev)){
			measure_ao_fifo(adev);
		}
		return count;
	}else{
		return -1;
	}
}

DEVICE_ATTR(bank_mask,
		S_IRUGO|S_IWUSR, show_bank_mask, store_bank_mask);

static ssize_t store_active_chan(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int active_chan;

	dev_dbg(DEVP(adev), "store_active_chan 01 \"%s\"", buf);
	if (sscanf(buf, "%d", &active_chan) == 1){
		dev_dbg(DEVP(adev), "store_active_chan 50 set nchan_enabled %d", active_chan);
		adev->nchan_enabled = active_chan;
		return count;
	}else{
		return -1;
	}
}

static ssize_t show_active_chan(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->nchan_enabled);
}

static DEVICE_ATTR(active_chan,
		S_IRUGO|S_IWUSR, show_active_chan, store_active_chan);

static ssize_t show_module_type(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%X\n", adev->mod_id>>MOD_ID_TYPE_SHL);
}


static DEVICE_ATTR(module_type, S_IRUGO, show_module_type, 0);

static ssize_t show_fpga_rev(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->mod_id&MOD_ID_REV_MASK);
}


static DEVICE_ATTR(fpga_rev, S_IRUGO, show_fpga_rev, 0);

static ssize_t show_module_role(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	if (adev->of_prams.site == 0){
		int clkout = adev->mod_id& MOD_ID_IS_CLKOUT;
		return sprintf(buf, "%s\n", clkout? "CLKOUT": "CLKIN");
	}else{
		int slave = adev->mod_id&MOD_ID_IS_SLAVE;
		return sprintf(buf, "%s\n", slave ? "SLAVE": "MASTER");
	}
}


static DEVICE_ATTR(module_role, S_IRUGO, show_module_role, 0);

static const char* _lookup_id(struct acq400_dev *adev)
{
	static struct IDLUT_ENTRY {
		int key;
		const char* name;
	} idlut[] = {
		{ MOD_ID_ACQ1001SC, 	"acq1001sc" 	},
		{ MOD_ID_ACQ2006SC,	"acq2006sc"	},
		{ MOD_ID_ACQ2106SC, 	"acq2106sc"	},
		{ MOD_ID_KMCU,		"kmcu"		},
		{ MOD_ID_KMCU30,	"kmcu30"	},
		{ MOD_ID_ACQ420FMC,	"acq420fmc"	},
		{ MOD_ID_ACQ420FMC_2000,"acq420fmc"	},
		{ MOD_ID_ACQ423ELF,     "acq423elf"     },
		{ MOD_ID_ACQ424ELF,	"acq424elf"	},
		{ MOD_ID_ACQ425ELF,     "acq425elf"	},
		{ MOD_ID_ACQ425ELF_2000,"acq425elf"	},
		{ MOD_ID_ACQ427ELF,	"acq427elf"	},
		{ MOD_ID_ACQ427ELF_2000,"acq427elf"     },
		{ MOD_ID_ACQ430FMC,     "acq430fmc"	},
		{ MOD_ID_ACQ435ELF,	"acq435elf"	},
		{ MOD_ID_ACQ436ELF,	"acq436elf"	},
		{ MOD_ID_ACQ437ELF,	"acq437elf"	},
		{ MOD_ID_ACQ480FMC,   	"acq480fmc"	},
		{ MOD_ID_AO420FMC,	"ao420fmc"	},
		{ MOD_ID_AO420FMC_CS2,	"ao420fmc"	},
		{ MOD_ID_AO424ELF,	"ao424elf"	},
		{ MOD_ID_DAC_CELF, 	"ao428elf"	},
		{ MOD_ID_FMC104,        "fmc104"	},
		{ MOD_ID_DIO432FMC, 	"dio432"	},
		{ MOD_ID_DIO432PMOD,	"dio432"	},
		{ MOD_ID_DIO482FMC,  	"dio432"	},	/* logically same */
		{ MOD_ID_CPSC2,         "sc"	},
		{ MOD_ID_CPSC2_COMMS,   "cpsc2com"	},
		{ MOD_ID_CPSC2_DAC,     "cpsc2dac"	},
	};
#define NID	(sizeof(idlut)/sizeof(struct IDLUT_ENTRY))
	int ii;
	int id = GET_MOD_ID(adev);

	switch(id){
	case MOD_ID_DIO_BISCUIT:
		switch(GET_MOD_IDV(adev)){
		case MOD_IDV_V2F:
			return "v2f";
		case MOD_IDV_DIO:
			return "dio";
		case MOD_IDV_QEN:
			return "qen";
		case MOD_IDV_ACQ1014:
			return "acq1014";
		default:
			break;
		}
	default:
		for (ii = 0; ii < NID; ++ii){
			if (idlut[ii].key == id){
				return idlut[ii].name;
			}
		}
	}

	return "unknown";
}
static ssize_t show_module_name(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%s\n", _lookup_id(adev));
}


static DEVICE_ATTR(module_name, S_IRUGO, show_module_name, 0);

static ssize_t show_site(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->of_prams.site);
}


static DEVICE_ATTR(site, S_IRUGO, show_site, 0);

static ssize_t show_sysclkhz(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->sysclkhz);
}

static DEVICE_ATTR(sysclkhz, S_IRUGO, show_sysclkhz, 0);


static ssize_t show_nacc(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 acc_dec = acq400rd32(adev, ADC_ACC_DEC);
	if (acc_dec == ADC_NACC_PASSTHRU){
		return sprintf(buf, "0,0,0\n");
	}else{
		unsigned shift = (acc_dec&ADC_ACC_DEC_SHIFT_MASK)>>
					getSHL(ADC_ACC_DEC_SHIFT_MASK);
		unsigned start = (acc_dec&ADC_ACC_DEC_START_MASK)>>
					getSHL(ADC_ACC_DEC_START_MASK);
		return sprintf(buf, "%u,%u,%u\n",
			(acc_dec&ADC_ACC_DEC_LEN)+1, shift, start);
	}
}

static ssize_t store_nacc(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned nacc;
	unsigned shift = 999;
	unsigned start = 0;

	if (sscanf(buf, "%u,%u,%u", &nacc, &shift, &start) >= 1){
		u32 acdc = 0;

		if (nacc > 0){
			nacc = min(nacc, ADC_MAX_NACC);
			start = min(start,nacc-1);

			if (shift == 999){
				for (shift = 0; 1<<shift < nacc; ++shift)
					;
				if ((1<<shift) > nacc){
					shift -= 1;
				}
				start = nacc - (1<<shift);
			}
			shift = min(shift, ADC_ACC_DEC_SHIFT_MAX);

			acdc = nacc-1;
			acdc |= shift<<getSHL(ADC_ACC_DEC_SHIFT_MASK);
			acdc |= start<<getSHL(ADC_ACC_DEC_START_MASK);
		}
		acq400wr32(adev, ADC_ACC_DEC, acdc);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(nacc, S_IRUGO|S_IWUSR, show_nacc, store_nacc);
static DEVICE_ATTR(ACC, S_IRUGO|S_IWUSR, show_nacc, store_nacc);

static ssize_t store_dec(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned ndec;
	unsigned start = 0;

	if (sscanf(buf, "%u", &ndec) == 1){
		u32 acdc= 0;

		if (ndec > 0){
			ndec = min(ndec, ADC_MAX_NACC);
			start = ndec-1;
			acdc = ndec-1;
			acdc |= start<<getSHL(ADC_ACC_DEC_START_MASK);
		}
		acq400wr32(adev, ADC_ACC_DEC, acdc);
		return count;
	}else{
		return -1;
	}
}
static DEVICE_ATTR(DEC, S_IRUGO|S_IWUSR, show_nacc, store_dec);

static ssize_t show_is_triggered(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int triggered = acq400rd32(adev, ADC_FIFO_STA)&ADC_FIFO_STA_ACTIVE;

	return sprintf(buf, "%u\n", triggered != 0);
}


static DEVICE_ATTR(is_triggered, S_IRUGO, show_is_triggered, 0);

static ssize_t show_continuous_reader(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u\n", adev->continuous_reader);
}

static DEVICE_ATTR(continuous_reader, S_IRUGO, show_continuous_reader, 0);

static ssize_t show_has_axi_dma(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%u\n", IS_AXI64(adev)? IS_AXI64_DUALCHAN_CAPABLE(adev)? 2: 1: 0);
}

static ssize_t store_has_axi_dma(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	init_axi_dma(adev);
	return count;
}


static DEVICE_ATTR(has_axi_dma, S_IRUGO|S_IWUSR,
		show_has_axi_dma, store_has_axi_dma);



static ssize_t show_has_axi_dma_stack(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%u\n", IS_ACQ2106_STAGGER(adev)+IS_ACQ2106_STACK(adev));
}

static DEVICE_ATTR(has_axi_dma_stack, S_IRUGO, show_has_axi_dma_stack, 0);

static ssize_t store_RW32_debug(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
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
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->RW32_debug);
}

static DEVICE_ATTR(RW32_debug,
		S_IRUGO|S_IWUSR, show_RW32_debug, store_RW32_debug);


static ssize_t store_sod(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int sod_mode;

	if (sscanf(buf, "%d", &sod_mode) == 1){
		adev->sod_mode = sod_mode;
		return count;
	}else{
		return -1;
	}
}

static ssize_t show_sod(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", adev->sod_mode);
}

static DEVICE_ATTR(sod, S_IRUGO|S_IWUSR, show_sod, store_sod);

MAKE_BIT_N(sync_trg_to_clk, ADC_CTRL, MAKE_BITS_FROM_MASK, ADC_CTRL_SYNC_TRG_N, 0);

static ssize_t show_is_adc(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%d\n", IS_ADC(adev));
}

static DEVICE_ATTR(is_adc, S_IRUGO, show_is_adc, 0);

static const struct attribute *sysfs_base_attrs[] = {
	&dev_attr_module_type.attr,
	&dev_attr_module_role.attr,
	&dev_attr_module_name.attr,
	&dev_attr_nbuffers.attr,
	&dev_attr_bufferlen.attr,
	&dev_attr_optimise_bufferlen.attr,
	&dev_attr_site.attr,
	&dev_attr_data32.attr,
	&dev_attr_continuous_reader.attr,
	&dev_attr_RW32_debug.attr,
	NULL
};

static const struct attribute *sysfs_device_attrs[] = {
	&dev_attr_clkdiv.attr,
	&dev_attr_simulate.attr,
	&dev_attr_stats.attr,
	&dev_attr_event1.attr,
	&dev_attr_event0.attr,
	&dev_attr_sync.attr,
	&dev_attr_trg.attr,
	&dev_attr_sod.attr,
	&dev_attr_clk.attr,
	&dev_attr_clk_count.attr,
	&dev_attr_clk_counter_src.attr,
	&dev_attr_sample_count.attr,
	&dev_attr_shot.attr,
	&dev_attr_completed_shot.attr,
	&dev_attr_run.attr,
	&dev_attr_hitide.attr,
	&dev_attr_lotide.attr,
	&dev_attr_sysclkhz.attr,
	&dev_attr_active_chan.attr,
	&dev_attr_nacc.attr,
	&dev_attr_ACC.attr,
	&dev_attr_DEC.attr,
	&dev_attr_is_triggered.attr,
	&dev_attr_event0_count.attr,
	&dev_attr_sync_trg_to_clk.attr,
	&dev_attr_is_adc.attr,
	&dev_attr_evt_sc_latch.attr,
	NULL,
};


MAKE_BITS(emulate_acq196, ADC_CTRL, MAKE_BITS_FROM_MASK, ADC_CTRL_424_EMUL_196);

static const struct attribute *acq424_attrs[] = {
	&dev_attr_clk_min_max.attr,
	&dev_attr_adc_conv_time.attr,
	&dev_attr_emulate_acq196.attr,
	NULL
};
static const struct attribute *acq425_attrs[] = {
	&dev_attr_bank_mask.attr,
	&dev_attr_adc_nobt.attr,
	&dev_attr_adc_18b.attr,
	&dev_attr_gains.attr,
	&dev_attr_gain1.attr,
	&dev_attr_gain2.attr,
	&dev_attr_gain3.attr,
	&dev_attr_gain4.attr,
	&dev_attr_adc_conv_time.attr,
	NULL
};

#define ACQ420_ATTRS	(acq425_attrs+1)


static const struct attribute *acq435_attrs[] = {
	&dev_attr_hi_res_mode.attr,
	&dev_attr_bank_mask.attr,
	&dev_attr_bitslice_frame.attr,
	&dev_attr_sw_emb_word1.attr,
	&dev_attr_sw_emb_word2.attr,
	&dev_attr_gate_sync.attr,
	NULL
};


SCOUNT_KNOB(CLK_EXT, 	ACQ2006_CLK_COUNT(0));
SCOUNT_KNOB(CLK_MB, 	ACQ2006_CLK_COUNT(1));
SCOUNT_KNOB(CLK_S1,     ACQ2006_CLK_COUNT(SITE2DX(1)));
SCOUNT_KNOB(CLK_S2,     ACQ2006_CLK_COUNT(SITE2DX(2)));
SCOUNT_KNOB(CLK_S3,     ACQ2006_CLK_COUNT(SITE2DX(3)));
SCOUNT_KNOB(CLK_S4,     ACQ2006_CLK_COUNT(SITE2DX(4)));
SCOUNT_KNOB(CLK_S5,     ACQ2006_CLK_COUNT(SITE2DX(5)));
SCOUNT_KNOB(CLK_S6,     ACQ2006_CLK_COUNT(SITE2DX(6)));

SCOUNT_KNOB(TRG_EXT, 	ACQ2006_TRG_COUNT(0));
SCOUNT_KNOB(TRG_MB, 	ACQ2006_TRG_COUNT(1));
SCOUNT_KNOB(TRG_S1,     ACQ2006_TRG_COUNT(SITE2DX(1)));
SCOUNT_KNOB(TRG_S2,     ACQ2006_TRG_COUNT(SITE2DX(2)));
SCOUNT_KNOB(TRG_S3,     ACQ2006_TRG_COUNT(SITE2DX(3)));
SCOUNT_KNOB(TRG_S4,     ACQ2006_TRG_COUNT(SITE2DX(4)));
SCOUNT_KNOB(TRG_S5,     ACQ2006_TRG_COUNT(SITE2DX(5)));
SCOUNT_KNOB(TRG_S6,     ACQ2006_TRG_COUNT(SITE2DX(6)));

SCOUNT_KNOB(EVT_EXT, 	ACQ2006_EVT_COUNT(0));
SCOUNT_KNOB(EVT_MB, 	ACQ2006_EVT_COUNT(1));
SCOUNT_KNOB(EVT_S1,     ACQ2006_EVT_COUNT(SITE2DX(1)));
SCOUNT_KNOB(EVT_S2,     ACQ2006_EVT_COUNT(SITE2DX(2)));
SCOUNT_KNOB(EVT_S3,     ACQ2006_EVT_COUNT(SITE2DX(3)));
SCOUNT_KNOB(EVT_S4,     ACQ2006_EVT_COUNT(SITE2DX(4)));
SCOUNT_KNOB(EVT_S5,     ACQ2006_EVT_COUNT(SITE2DX(5)));
SCOUNT_KNOB(EVT_S6,     ACQ2006_EVT_COUNT(SITE2DX(6)));

SCOUNT_KNOB(SYN_EXT, 	ACQ2006_SYN_COUNT(0));
SCOUNT_KNOB(SYN_MB, 	ACQ2006_SYN_COUNT(1));
SCOUNT_KNOB(SYN_S1,     ACQ2006_SYN_COUNT(SITE2DX(1)));
SCOUNT_KNOB(SYN_S2,     ACQ2006_SYN_COUNT(SITE2DX(2)));
SCOUNT_KNOB(SYN_S3,     ACQ2006_SYN_COUNT(SITE2DX(3)));
SCOUNT_KNOB(SYN_S4,     ACQ2006_SYN_COUNT(SITE2DX(4)));
SCOUNT_KNOB(SYN_S5,     ACQ2006_SYN_COUNT(SITE2DX(5)));
SCOUNT_KNOB(SYN_S6,     ACQ2006_SYN_COUNT(SITE2DX(6)));


static ssize_t show_fan_percent(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned pwm = acq400rd32(adev, MOD_CON)&ACQ1001_MCR_PWM_MASK;
	unsigned fan_percent;

	/* scale to 1..100 */
	pwm >>= ACQ1001_MCR_PWM_BIT;
	if (pwm > ACQ1001_MCR_PWM_MIN){
		pwm -= ACQ1001_MCR_PWM_MIN;
	}
	fan_percent = (pwm*100)/ACQ1001_MCR_PWM_MAX;
	return sprintf(buf, "%u\n", fan_percent);
}

static ssize_t store_fan_percent(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned mod_con = acq400rd32(adev, MOD_CON);
	unsigned fan_percent;

	if (sscanf(buf, "%u", &fan_percent) == 1){
		unsigned pwm;

		if (fan_percent > 100) fan_percent = 100;
		pwm = (fan_percent*ACQ1001_MCR_PWM_MAX)/100;
		pwm += ACQ1001_MCR_PWM_MIN;
		pwm <<= ACQ1001_MCR_PWM_BIT;
		if (pwm > ACQ1001_MCR_PWM_MASK){
			pwm = ACQ1001_MCR_PWM_MASK;
		}else{
			pwm &= ACQ1001_MCR_PWM_MASK;
		}
		mod_con &= ~ACQ1001_MCR_PWM_MASK;
		mod_con |= pwm;

		dev_info(DEVP(adev), "store_fan_percent:%d write %08x\n",
				fan_percent, mod_con);

		acq400wr32(adev, MOD_CON, mod_con);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(fan_percent, S_IWUSR|S_IRUGO, show_fan_percent, store_fan_percent);

static ssize_t show_acq0000_mod_con(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned mask)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 mod_con = acq400rd32(adev, MOD_CON);

	return sprintf(buf, "%u\n", (mod_con&mask) != 0);
}



static ssize_t store_acq0000_mod_con(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	unsigned mask)
{
	unsigned enable;
	if (sscanf(buf, "%u", &enable) == 1){
		struct acq400_dev *adev = acq400_devices[dev->id];
		u32 mod_con = acq400rd32(adev, MOD_CON);

		if (enable){
			mod_con |= mask;
		}else{
			mod_con &= ~mask;
		}
		acq400wr32(adev, MOD_CON, mod_con);
		return count;
	}else{
		return -1;
	}
}

#define MODCON_KNOB(name, mask)						\
static ssize_t show_##name(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_acq0000_mod_con(dev, attr, buf, mask);		\
}									\
static ssize_t store_##name(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_acq0000_mod_con(dev, attr, buf, count, mask);	\
}									\
static DEVICE_ATTR(name, S_IRUGO|S_IWUSR, show_##name, store_##name)

MODCON_KNOB(mod_en, 	MCR_MOD_EN);
MODCON_KNOB(psu_sync, 	MCR_PSU_SYNC);
MODCON_KNOB(fan,	MCR_FAN_EN);
MODCON_KNOB(soft_trig,  MCR_SOFT_TRIG);
MODCON_KNOB(celf_power_en, ACQ1001_MCR_CELF_PSU_EN);

int get_agg_threshold_bytes(struct acq400_dev *adev, u32 agg)
{
	return ((agg>>AGG_SIZE_SHL)&AGG_SIZE_MASK)*AGG_SIZE_UNIT(adev);
}

#define AGG_SEL	"aggregator="
#define DIST_SEL "distributor="
#define TH_SEL	"threshold="

static ssize_t show_agg_reg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const unsigned offset,
	const unsigned mshift)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 regval = acq400rd32(adev, offset);
	char mod_group[80];
	int site;

	for (site = 1, mod_group[0] = '\0'; site <= 6; ++site){
		if ((regval & AGG_MOD_EN(site, mshift)) != 0){
			/* @@todo site 3/4 frig */
			int dsite = (IS_ACQ1001SC(adev) && site == 3)? 4: site;
			if (strlen(mod_group) == 0){
				strcat(mod_group, "sites=");
			}else{
				strcat(mod_group, ",");
			}

			sprintf(mod_group+strlen(mod_group), "%d", dsite);
		}
	}
	if (strlen(mod_group) == 0){
		sprintf(mod_group, "sites=none");
	}
	if (mshift == DATA_ENGINE_MSHIFT){
		sprintf(mod_group+strlen(mod_group), " %s%d",
				offset==DATA_ENGINE_1? DIST_SEL: AGG_SEL,
				regval&DATA_ENGINE_SELECT_AGG? 1: 0);
	}else if(mshift == AGGREGATOR_MSHIFT){
		sprintf(mod_group+strlen(mod_group), " %s%d", TH_SEL,
				get_agg_threshold_bytes(adev, regval));
	}

	return sprintf(buf, "reg=0x%08x %s DATA_MOVER_EN=%s\n",
			regval, mod_group, regval&DATA_MOVER_EN? "on": "off");
}



static inline void reset_fifo(struct acq400_dev *adev, int enable)
{
	u32 ctrl;
	adev->RW32_debug = reset_fifo_verbose;
	ctrl = acq400rd32(adev, ADC_CTRL);

	ctrl &= ~(ADC_CTRL_ADC_EN|ADC_CTRL_FIFO_EN);
	acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_FIFO_RST);
	ctrl = acq400rd32(adev, ADC_CTRL);
	acq400wr32(adev, ADC_CTRL, ctrl &= ~ADC_CTRL_FIFO_RST);
	if (enable){
		ctrl |= ADC_CTRL_ADC_EN;
	}
	acq400wr32(adev, ADC_CTRL, ctrl|ADC_CTRL_FIFO_EN);
	adev->RW32_debug = 0;
}

#define ENABLE	1

static void reset_fifo_enable(struct acq400_dev* adev)
{
	reset_fifo(adev, ENABLE);
}

static void reset_fifo_clr(struct acq400_dev* adev)
{
	reset_fifo(adev, 0);
}
void reset_fifo_set(struct acq400_dev* adev, struct acq400_dev* set[], int enable)
{
	acq400_visit_set(set, enable? reset_fifo_enable: reset_fifo_clr);
}



static ssize_t store_agg_reg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const unsigned offset,
	const unsigned mshift)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned regval;
	char* match;
	int pass = 0;

	dev_dbg(DEVP(adev), "store_agg_reg \"%s\"", buf);

	if ((match = strstr(buf, "sites=")) != 0){
		unsigned regval = acq400rd32(adev, offset);
		char* cursor = match+strlen("sites=");
		int site;

		regval &= ~(AGG_SITES_MASK << mshift);

		acq400_clear_aggregator_set(adev);
		if (strncmp(cursor, "none", 4) != 0){
			for (; *cursor && *cursor != ' '; ++cursor){
				switch(*cursor){
				case ',':
				case ' ':	continue;
				case '\n':  	break;
				default:
					site = acq400_get_site(adev, cursor);
					if (site > 0){
						regval |= AGG_MOD_EN(site, mshift);
						acq400_add_aggregator_set(adev, site);
						break;
					}else{
						dev_err(dev, "bad site designator: %c", *cursor);
						return -1;
					}
				}
			}
		}
		acq400wr32(adev, offset, regval);
		pass = 1;
	}
	if (mshift == DATA_ENGINE_MSHIFT){
		char *sel = strstr(buf, offset==DATA_ENGINE_1? DIST_SEL: AGG_SEL);

		if (sel){
			char *pv = strchr(sel, '=');
			if (pv == 0){
				dev_err(dev, "ERROR: %s %d", __FILE__, __LINE__);
			}else{
				int include_agg = 0;
				if (sscanf(pv+1, "%d", &include_agg) == 1){
					unsigned regval = acq400rd32(adev, offset);
					if (include_agg){
						regval |= DATA_ENGINE_SELECT_AGG;
					}else{
						regval &= ~DATA_ENGINE_SELECT_AGG;
					}
					acq400wr32(adev, offset, regval);
					pass = 1;
				}
			}
		}
	}else if (mshift == AGGREGATOR_MSHIFT){
		char *th_sel = strstr(buf, TH_SEL);
		if (th_sel){
			int thbytes = 0;
			if (sscanf(th_sel, TH_SEL"%d", &thbytes) == 1){
				unsigned regval = acq400rd32(adev, offset);
				unsigned th = thbytes/AGG_SIZE_UNIT(adev);
				if (th > AGG_SIZE_MASK) th = AGG_SIZE_MASK;
				regval &= ~(AGG_SIZE_MASK<<AGG_SIZE_SHL);
				regval |= th<<AGG_SIZE_SHL;
				acq400wr32(adev, offset, regval);
				pass = 1;
			}else{
				dev_err(dev, "arg not integer (%s)", th_sel);
				return -1;
			}
		}
	}
	if ((match = strstr(buf, "on")) != 0){
		unsigned regval = acq400rd32(adev, offset);
		regval |= DATA_MOVER_EN;
		acq400wr32(adev, offset, regval);
		pass = 1;
	}else if ((match = strstr(buf, "off")) != 0){
		unsigned regval = acq400rd32(adev, offset);
		regval &= ~DATA_MOVER_EN;
		acq400wr32(adev, offset, regval);
		pass = 1;
	}

	if (pass){
		return count;
	}else{
		/* aggregator= passes this paragraph, so put it LAST! */
		if (sscanf(buf, "%x", &regval) == 1){
			acq400wr32(adev, offset, regval);
			return count;
		}
		return -1;
	}
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

REG_KNOB(aggregator, AGGREGATOR,	AGGREGATOR_MSHIFT);
REG_KNOB(data_engine_0, DATA_ENGINE(0), DATA_ENGINE_MSHIFT);
REG_KNOB(data_engine_1, DATA_ENGINE(1), DATA_ENGINE_MSHIFT);
REG_KNOB(data_engine_2, DATA_ENGINE(2), DATA_ENGINE_MSHIFT);
REG_KNOB(data_engine_3, DATA_ENGINE(3), DATA_ENGINE_MSHIFT);

static ssize_t show_aggregator_set(
	struct device* dev,
	struct device_attribute* attr,
	char *buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return acq400_read_aggregator_set(adev, buf, 32);
}
static DEVICE_ATTR(aggregator_set, S_IRUGO, show_aggregator_set, 0);
enum DistComms {
	DC_A9 = '9', DC_COMMS_B = 'B', DC_COMMS_A = 'A', DC_A9_ALT = '0', DC_COMMS_B_ALT = '1', DC_COMMS_A_ALT = '2'
};
static enum DistComms fromDistCommsFudge(u32 reg)
{
	switch(reg&DIST_COMMS_MASK){
	case DIST_COMMS_A:
		return DC_COMMS_A;
	case DIST_COMMS_B:
		return DC_COMMS_B;
	default:
		return DC_A9;
	}
}
static u32 toDistCommsFudge(enum DistComms dc)
{
	switch(dc){
	case DC_A9:
	case DC_A9_ALT:
	default:
		return 0;
	case DC_COMMS_B:
	case DC_COMMS_B_ALT:
		return DIST_COMMS_B;
	case DC_COMMS_A:
	case DC_COMMS_A_ALT:
		return DIST_COMMS_A;
	}
}
static ssize_t show_dist_reg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const unsigned offset,
	const unsigned mshift)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 regval = acq400rd32(adev, offset);
	char mod_group[80];
	int site;
	int pad = 0;

	for (site = 1, mod_group[0] = '\0'; site <= 6; ++site){
		if ((regval & AGG_MOD_EN(site, mshift)) != 0){
			/* @@todo site 3/4 frig */
			int dsite = (IS_ACQ1001SC(adev) && site == 3)? 4: site;
			if (strlen(mod_group) == 0){
				strcat(mod_group, "sites=");
			}else{
				strcat(mod_group, ",");
			}

			sprintf(mod_group+strlen(mod_group), "%d", dsite);
		}
	}
	if (strlen(mod_group) == 0){
		sprintf(mod_group, "sites=none");
	}
	sprintf(mod_group+strlen(mod_group), " comms=%c", fromDistCommsFudge(regval));

	if ((regval&AGG_SPAD_EN) != 0){
		pad = ((regval>>AGG_SPAD_LEN_SHL)&DIST_TRASH_LEN_MASK) + 1;
	}
	sprintf(mod_group+strlen(mod_group), " pad=%d", pad);

	return sprintf(buf, "reg=0x%08x %s DATA_MOVER_EN=%s\n", regval, mod_group,
			regval&DATA_MOVER_EN? "on": "off");
}

void onDistributorEnable(struct acq400_dev *adev, const unsigned offset)
{
	unsigned regval = acq400rd32(adev, offset);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	reset_fifo_set(adev, sc_dev->distributor_set, ENABLE);

	regval &= ~(DIST_ENABLEN|DIST_FIFO_RESET);
	acq400wr32(adev, offset, regval|DIST_FIFO_RESET);
	acq400wr32(adev, offset, regval|DIST_ENABLEN);
}

void onDistributorDisable(struct acq400_dev *adev, const unsigned offset)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	unsigned regval = acq400rd32(adev, offset);

	reset_fifo_set(adev, sc_dev->distributor_set, !ENABLE);

	regval &= ~DIST_ENABLEN;
	acq400wr32(adev, offset, regval);
}

static ssize_t store_dist_reg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const unsigned offset,
	const unsigned mshift)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned regval = acq400rd32(adev, offset);
	char* match;
	int pass = 0;

	dev_dbg(DEVP(adev), "store_dist_reg \"%s\"", buf);

	if ((match = strstr(buf, "sites=")) != 0){
		char* cursor = match+strlen("sites=");
		int site;

		regval &= ~(AGG_SITES_MASK << mshift);

		acq400_clear_distributor_set(adev);
		if (strncmp(cursor, "none", 4) != 0){
			for (; *cursor && *cursor != ' '; ++cursor){
				switch(*cursor){
				case ',':
				case ' ':	continue;
				case '\n':  	break;
				default:
					site = acq400_get_site(adev, cursor);
					if (site > 0){
						regval |= AGG_MOD_EN(site%HALF_SITE, mshift);
						acq400_add_distributor_set(adev, site);
						/* for multi-digit numbers eg 101, cursor past the rest of the number */
						while (isdigit(cursor[1])){
							++cursor;
						}
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
	if (mshift == DIST_MSHIFT){
		char *th_sel = strstr(buf, TH_SEL);
		if (th_sel){
			int thbytes = 0;
			if (sscanf(th_sel, TH_SEL"%d", &thbytes) == 1){
				unsigned th = thbytes/AGG_SIZE_UNIT(adev);
				if (th > AGG_SIZE_MASK) th = AGG_SIZE_MASK;
				regval &= ~(AGG_SIZE_MASK<<AGG_SIZE_SHL);
				regval |= th<<AGG_SIZE_SHL;
				pass = 1;
			}else{
				dev_err(dev, "arg not integer (%s)", th_sel);
				return -1;
			}
		}
	}
	if ((match = strstr(buf, "comms")) != 0){
		char en;
		if (sscanf(match, "comms=%c", &en) == 1 ){
			regval &= ~DIST_COMMS_MASK;
			dev_dbg(DEVP(adev), "comms=%c set 0x%08x", en, toDistCommsFudge(en));
			regval |= toDistCommsFudge(en);
			pass = 1;
		}else{
			dev_err(dev, "comms=%%d");
			return -1;
		}
	}
	if ((match = strstr(buf, "pad")) != 0){
		int padlen;
		if (sscanf(match, "pad=%d", &padlen) == 1){
			regval &= ~DIST_TRASH_LEN_MASK << AGG_SPAD_LEN_SHL;
			if (padlen){
				regval |= AGG_SPAD_EN;
				regval |= ((padlen-1)&DIST_TRASH_LEN_MASK) <<
						AGG_SPAD_LEN_SHL;
			}else{
				regval &= ~AGG_SPAD_EN;
			}
			pass = 1;
		}
	}


	if (pass){
		acq400wr32(adev, offset, regval);
		if ((match = strstr(buf, "on")) != 0){
			onDistributorEnable(adev, offset);
		}else if ((match = strstr(buf, "off")) != 0){
			onDistributorDisable(adev, offset);
		}
		return count;
	}else{
		/* aggregator= passes this paragraph, so put it LAST! */
		if (sscanf(buf, "%x", &regval) == 1){
			acq400wr32(adev, offset, regval);
			return count;
		}
		return -1;
	}
}

#define DIST_KNOB(name, offset, mshift)					\
static ssize_t show_reg_##name(						\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_dist_reg(dev, attr, buf, offset, mshift);		\
}									\
static ssize_t store_reg_##name(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_dist_reg(dev, attr, buf, count,  offset, mshift);	\
}									\
static DEVICE_ATTR(name, 						\
	S_IRUGO|S_IWUSR, show_reg_##name, store_reg_##name)

DIST_KNOB(distributor, DISTRIBUTOR, DIST_MSHIFT);


static ssize_t show_dist_s1(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct acq400_dev *ds1 = sc_dev->distributor_set[0];
	return sprintf(buf, "%d\n", ds1? ds1->of_prams.site: 0);
}
static DEVICE_ATTR(dist_s1, S_IRUGO, show_dist_s1, 0);


static ssize_t show_decimate(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 agg = acq400rd32(adev, AGGREGATOR);
	int decimate = ((agg >>= AGG_DECIM_SHL)&AGG_DECIM_MASK) + 1;

	return sprintf(buf, "%d\n", decimate);
}

static ssize_t store_decimate(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned decimate;

	if (sscanf(buf, "%u", &decimate) == 1 && decimate){
		u32 agg = acq400rd32(adev, AGGREGATOR);
		agg &= ~(AGG_DECIM_MASK << AGG_DECIM_SHL);
		if (--decimate > AGG_DECIM_MASK) decimate = AGG_DECIM_MASK;
		agg |= (decimate&AGG_DECIM_MASK) << AGG_DECIM_SHL;
		acq400wr32(adev, AGGREGATOR, agg);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(decimate, S_IRUGO|S_IWUSR, show_decimate, store_decimate);


static ssize_t show_aggsta(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned mask)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned aggsta = acq400rd32(adev, AGGSTA);
	unsigned field = aggsta&mask;

	for (; (mask&0x1) == 0; mask >>=1, field >>= 1){
		;
	}

	return sprintf(buf, "%u\n", field);
}

#define SHOW_AGGSTA(name, mask) 					\
static ssize_t show_aggsta_##name(					\
	struct device *dev,						\
	struct device_attribute *attr,					\
	char* buf )							\
{									\
	return show_aggsta(dev, attr, buf, mask);			\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_aggsta_##name, 0)

SHOW_AGGSTA(aggsta_fifo_count, AGGSTA_FIFO_COUNT);
SHOW_AGGSTA(aggsta_fifo_stat,  AGGSTA_FIFO_STAT);
SHOW_AGGSTA(aggsta_engine_stat, AGGSTA_ENGINE_STAT);

static ssize_t store_estop(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned estop;

	if (sscanf(buf, "%d", &estop) == 1){
		if (estop){
			dev_info(DEVP(adev), "STOP");
			acq2006_estop(adev);
		}
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(estop, S_IWUSR, 0, store_estop);

static ssize_t show_bq_overruns(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int rc = sprintf(buf, "%u\n", adev->bq_overruns);
	adev->bq_overruns = 0;
	return rc;
}


static DEVICE_ATTR(bq_overruns, S_IRUGO, show_bq_overruns, 0);

static ssize_t show_bq_max(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int rc = sprintf(buf, "%u\n", adev->bq_max);
	adev->bq_max = 0;
	return rc;
}


static DEVICE_ATTR(bq_max, S_IRUGO, show_bq_max, 0);



static ssize_t show_es_enable(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, ADC_CTRL);

	return sprintf(buf, "%d\n", (ctrl&ADC_CTRL_ES_EN) != 0);
}

static ssize_t store_es_enable(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned es_enable;

	if (sscanf(buf, "%u", &es_enable) == 1){
		u32 ctrl = acq400rd32(adev, ADC_CTRL);
		if (es_enable){
			ctrl |= ADC_CTRL_ES_EN;
		}else{
			ctrl &= ~ADC_CTRL_ES_EN;
		}
		acq400wr32(adev, ADC_CTRL, ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(es_enable, S_IRUGO|S_IWUSR, show_es_enable, store_es_enable);

static int jettison = 0;

static ssize_t show_jettison_buffers_from(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return sprintf(buf, "%d\n", jettison);
}

static ssize_t store_jettison_buffers_from(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dump_from;

	if (sscanf(buf, "%u Y", &dump_from) == 1){
		dev_warn(DEVP(adev), "%s DUMPING buffers from %d", __FUNCTION__, dump_from);
		acq400_free_buffers(adev, dump_from);
		jettison = 1;
		return count;
	}else{
		dev_warn(DEVP(adev), "%s format \"first Y\"", __FUNCTION__);
		return -1;
	}
}
static DEVICE_ATTR(jettison_buffers_from, S_IRUGO|S_IWUSR,
		show_jettison_buffers_from, store_jettison_buffers_from);

static const struct attribute *rgm_attrs[] = {
	&dev_attr_rgm.attr,
	&dev_attr_rtm_translen.attr,
	&dev_attr_es_enable.attr,
	NULL
};

static ssize_t show_axi_buffers_after_event(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%d\n", adev->axi_buffers_after_event);
}

static ssize_t store_axi_buffers_after_event(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	if (sscanf(buf, "%u", &adev->axi_buffers_after_event) == 1){
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(axi_buffers_over, S_IRUGO|S_IWUSR, \
	show_axi_buffers_after_event, store_axi_buffers_after_event);


static ssize_t show_axi_dma_fail(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, DATA_ENGINE_0);

	return sprintf(buf, "%d\n", (ctrl&DE_AXI_DMA_FAIL) != 0);
}


static DEVICE_ATTR(axi_dma_fail, S_IRUGO, show_axi_dma_fail, 0);


static const struct attribute *axi64_attrs[] = {
	&dev_attr_axi_dma_fail.attr,
	&dev_attr_axi_buffers_over.attr,
	&dev_attr_AXI_DMA_len.attr,
	NULL
};

static ssize_t show_hb_last(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%d\n", adev->rt.hbm_m1->ix);
}

static DEVICE_ATTR(hb_last, S_IRUGO, \
		show_hb_last, 0);


static const struct attribute *sc_common_attrs[] = {
	&dev_attr_aggregator.attr,
	&dev_attr_aggregator_set.attr,
	&dev_attr_distributor.attr,
	&dev_attr_dist_s1.attr,
	&dev_attr_decimate.attr,
	&dev_attr_aggsta_fifo_count.attr,
	&dev_attr_aggsta_fifo_stat.attr,
	&dev_attr_aggsta_engine_stat.attr,
	&dev_attr_mod_en.attr,
	&dev_attr_psu_sync.attr,
	&dev_attr_soft_trig.attr,
	&dev_attr_celf_power_en.attr,
	&dev_attr_gpg_top_count.attr,
	&dev_attr_axi_freq.attr,
	&dev_attr_gpg_trg.attr,
	&dev_attr_gpg_clk.attr,
	&dev_attr_gpg_sync.attr,
	&dev_attr_gpg_mode.attr,
	&dev_attr_gpg_enable.attr,
	&dev_attr_gpg_debug.attr,
	&dev_attr_spad.attr,
	&dev_attr_spad0.attr,
	&dev_attr_spad1.attr,
	&dev_attr_spad2.attr,
	&dev_attr_spad3.attr,
	&dev_attr_spad4.attr,
	&dev_attr_spad5.attr,
	&dev_attr_spad6.attr,
	&dev_attr_spad7.attr,
	&dev_attr_estop.attr,
	&dev_attr_spad1_us.attr,
	&dev_attr_bq_overruns.attr,
	&dev_attr_bq_max.attr,
	&dev_attr_hb_last.attr,
	&dev_attr_has_axi_dma.attr,
	&dev_attr_has_axi_dma_stack.attr,
	&dev_attr_dist_bufferlen.attr,
	&dev_attr_jettison_buffers_from.attr,
	&dev_attr_fpga_rev.attr,
	NULL
};
static const struct attribute *acq2006sc_attrs[] = {
	&dev_attr_data_engine_0.attr,
	&dev_attr_data_engine_1.attr,
	&dev_attr_data_engine_2.attr,
	&dev_attr_data_engine_3.attr,

	&dev_attr_scount_CLK_EXT.attr,
	&dev_attr_scount_CLK_MB.attr,
	&dev_attr_scount_CLK_S1.attr,
	&dev_attr_scount_CLK_S2.attr,
	&dev_attr_scount_CLK_S3.attr,
	&dev_attr_scount_CLK_S4.attr,
	&dev_attr_scount_CLK_S5.attr,
	&dev_attr_scount_CLK_S6.attr,

	&dev_attr_scount_TRG_EXT.attr,
	&dev_attr_scount_TRG_MB.attr,
	&dev_attr_scount_TRG_S1.attr,
	&dev_attr_scount_TRG_S2.attr,
	&dev_attr_scount_TRG_S3.attr,
	&dev_attr_scount_TRG_S4.attr,
	&dev_attr_scount_TRG_S5.attr,
	&dev_attr_scount_TRG_S6.attr,

	&dev_attr_scount_EVT_EXT.attr,
	&dev_attr_scount_EVT_MB.attr,
	&dev_attr_scount_EVT_S1.attr,
	&dev_attr_scount_EVT_S2.attr,
	&dev_attr_scount_EVT_S3.attr,
	&dev_attr_scount_EVT_S4.attr,
	&dev_attr_scount_EVT_S5.attr,
	&dev_attr_scount_EVT_S6.attr,

	&dev_attr_scount_SYN_EXT.attr,
	&dev_attr_scount_SYN_MB.attr,
	&dev_attr_scount_SYN_S1.attr,
	&dev_attr_scount_SYN_S2.attr,
	&dev_attr_scount_SYN_S3.attr,
	&dev_attr_scount_SYN_S4.attr,
	&dev_attr_scount_SYN_S5.attr,
	&dev_attr_scount_SYN_S6.attr,
	NULL
};


MAKE_BITS(fpctl_acq1014_clk, FPCTL, MAKE_BITS_FROM_MASK, FPCTL_FP_1014_CLK);


static const struct attribute *acq1014sc_attrs[] = {
	&dev_attr_fpctl_acq1014_clk.attr,
	&dev_attr_scount_TRG_S6.attr,		/* S6 TRG is TRG2 for second 8 */

/* static const struct attribute *acq1001sc_attrs[] = { */
	&dev_attr_data_engine_0.attr,
	&dev_attr_data_engine_1.attr,
	&dev_attr_fan.attr,
	&dev_attr_fan_percent.attr,

	&dev_attr_scount_CLK_EXT.attr,
	&dev_attr_scount_CLK_MB.attr,
	&dev_attr_scount_CLK_S1.attr,
	&dev_attr_scount_CLK_S2.attr,

	&dev_attr_scount_TRG_EXT.attr,
	&dev_attr_scount_TRG_MB.attr,
	&dev_attr_scount_TRG_S1.attr,
	&dev_attr_scount_TRG_S2.attr,

	&dev_attr_scount_EVT_EXT.attr,
	&dev_attr_scount_EVT_MB.attr,
	&dev_attr_scount_EVT_S1.attr,
	&dev_attr_scount_EVT_S2.attr,

	&dev_attr_scount_SYN_EXT.attr,
	&dev_attr_scount_SYN_MB.attr,
	&dev_attr_scount_SYN_S1.attr,
	&dev_attr_scount_SYN_S2.attr,
	NULL
};

#define acq1001sc_attrs (acq1014sc_attrs+2)

static const struct attribute *kmcx_sc_attrs[] = {
	&dev_attr_data_engine_0.attr,
	&dev_attr_data_engine_1.attr,

	&dev_attr_scount_CLK_EXT.attr,
	&dev_attr_scount_CLK_MB.attr,
	&dev_attr_scount_CLK_S1.attr,
	&dev_attr_scount_CLK_S2.attr,

	&dev_attr_scount_TRG_EXT.attr,
	&dev_attr_scount_TRG_MB.attr,
	&dev_attr_scount_TRG_S1.attr,
	&dev_attr_scount_TRG_S2.attr,

	&dev_attr_scount_EVT_EXT.attr,
	&dev_attr_scount_EVT_MB.attr,
	&dev_attr_scount_EVT_S1.attr,
	&dev_attr_scount_EVT_S2.attr,

	&dev_attr_scount_SYN_EXT.attr,
	&dev_attr_scount_SYN_MB.attr,
	&dev_attr_scount_SYN_S1.attr,
	&dev_attr_scount_SYN_S2.attr,
	NULL
};





const struct attribute *sysfs_sc_remaining_clocks[] = {
	&dev_attr_scount_CLK_S3.attr,
	&dev_attr_scount_CLK_S4.attr,
	&dev_attr_scount_CLK_S5.attr,
	&dev_attr_scount_CLK_S6.attr,
	&dev_attr_scount_TRG_S3.attr,
	&dev_attr_scount_TRG_S4.attr,
	&dev_attr_scount_EVT_S3.attr,
	&dev_attr_scount_EVT_S4.attr,
	&dev_attr_scount_SYN_S3.attr,
	&dev_attr_scount_SYN_S4.attr,
	NULL
};


MAKE_BITS(pwm_clkdiv, PWM_SOURCE_CLK_CTRL, MAKE_BITS_FROM_MASK, 0xffff<<PWM_SOURCE_CLK_CTRL_DIV_SHL);
MAKE_SIGNAL(pwm_src, PWM_SOURCE_CLK_CTRL, PWM_SOURCE_CLK_CTRL_SHL, PWM_SOURCE_CLK_CTRL_EN, ENA, DIS, 1);


const struct attribute *pwm2_attrs[] = {
	&dev_attr_pwm_src.attr,
	&dev_attr_pwm_clkdiv.attr,
	NULL
};



#define MAXSPEC	8	/* groups of special attrs */




void acq400_createSysfs(struct device *dev)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	const struct attribute **specials[MAXSPEC];
	int nspec = 0;

	dev_info(dev, "acq400_createSysfs()");
	if (sysfs_create_files(&dev->kobj, sysfs_base_attrs)){
		dev_err(dev, "failed to create sysfs");
	}

	if (IS_DUMMY(adev)){
		return;
	}else if (IS_DIO_BISCUIT_GENERIC(adev)){
		if (IS_DIO_BISCUIT(adev)){
			specials[nspec++] = sysfs_diobiscuit_attrs;
		}else if (IS_V2F(adev)){
			specials[nspec++] = sysfs_v2f_attrs;
		}else if (IS_ACQ1014_M(adev)){
			specials[nspec++] = sysfs_acq1014_attrs;
		}else if (IS_QEN(adev)){
			dev_info(dev, "IS_QEN");
			specials[nspec++] = sysfs_qen_attrs;
		}
	}else if (IS_RAD_CELF(adev)){
		sysfs_radcelf_create_files(dev);
		return;
	}else if IS_SC(adev){
		if (sysfs_create_files(&dev->kobj, sc_common_attrs)){
			dev_err(dev, "failed to create sysfs");
		}
		if (HAS_HDMI_SYNC(adev)){
			if (sysfs_create_files(&dev->kobj, hdmi_sync_attrs)){
				dev_err(dev, "failed to create sysfs HDMI");
			}
		}
		if (IS_AXI64(adev)){
			if (sysfs_create_files(&dev->kobj, axi64_attrs)){
				dev_err(dev, "failed to create sysfs axi64");
			}
		}

		if (IS_ACQ2X06SC(adev)){
			specials[nspec++] = acq2006sc_attrs;
			if (IS_ACQ2106_WR(adev)) {
				specials[nspec++] = acq2106_wr_attrs;
			}
		}else if (IS_ACQ1001SC(adev)){
			if (IS_ACQ1014(adev)){
				dev_info(dev, "ACQ1014: loading extra knobs");
				specials[nspec++] = acq1014sc_attrs;
			}else{
				specials[nspec++] = acq1001sc_attrs;
			}
		}else if (IS_KMCx_SC(adev)){
			specials[nspec++] = kmcx_sc_attrs;
		}else if (IS_CPSC2_SC(adev)){
			specials[nspec++] = acq2006sc_attrs;
			specials[nspec++] = cpsc2_sc_attrs;
			if (IS_CPSC2_WR(adev)) {
				specials[nspec++] = acq2106_wr_attrs;
			}
		}
	}else{
		if (sysfs_create_files(&dev->kobj, sysfs_device_attrs)){
			dev_err(dev, "failed to create sysfs");
		}

		if (HAS_RGM(adev)){
			if (sysfs_create_files(&dev->kobj, rgm_attrs)){
				dev_err(dev, "failed to create rgm sysfs");
			}
		}
		if (HAS_ATD(adev)){
			dev_info(dev, "HAS_ATD");
			if (sysfs_create_files(&dev->kobj, atd_attrs)){
				dev_err(dev, "failed to create atd sysfs");
			}
		}
		if (HAS_DTD(adev)){
			dev_info(dev, "HAS_DTD");
			if (sysfs_create_files(&dev->kobj, dtd_attrs)){
				dev_err(dev, "failed to create dtd sysfs");
			}
			acq400_clearDelTrg(adev);
		}

		if (IS_ACQ423(adev)){
			specials[nspec++] = acq423_attrs;
		}else if (IS_ACQ424(adev)){
			specials[nspec++] = acq424_attrs;
		}else if (IS_ACQ42X(adev)){
			specials[nspec++] =
				IS_ACQ425(adev) ? acq425_attrs: ACQ420_ATTRS;
		}else if (IS_ACQ43X(adev)){
			specials[nspec++] = acq435_attrs;
		}else if (IS_AO42S(adev)){
			specials[nspec++] = playloop_attrs;
			specials[nspec++] = dacspi_attrs;
			if (IS_AO420_HALF436(adev)){
				specials[nspec++] = ((adev->mod_id&MOD_ID_IS_SLAVE) == 0)?
						acq436_upper_half_attrs_master:
						acq436_upper_half_attrs;
				specials[nspec++] = ao420_half_436_attrs;
			}else{
				if (IS_AO420(adev)){
					specials[nspec++] = ao420_attrs;
				}else{
					specials[nspec++] = ao42S_attrs;
					if (IS_AO428(adev)){
						specials[nspec++] = ao428_attrs;
					}else if (IS_CPSC2_DAC(adev)){
						specials[nspec++] = cpsc2_dac_attrs;
					}
				}
			}
		}else if (IS_AO424(adev)){
			specials[nspec++] = playloop_attrs;
			specials[nspec++] = ao424_attrs;
		}else if (IS_BOLO8(adev)){
			specials[nspec++] = dacspi_attrs;
			specials[nspec++] = bolo8_attrs;
		}else if (IS_DIO432X(adev)){
			specials[nspec++] = playloop_attrs;
			specials[nspec++] = dio432_attrs;
			if (IS_DIO482FMC(adev) && GET_MOD_IDV(adev)==MOD_IDV_PWM2){
				specials[nspec++] = pwm2_attrs;
			}
		}else if (IS_ACQ400T(adev)){
			specials[nspec++] = acq400t_attrs;
		}else if (IS_ACQ480(adev)){
			specials[nspec++] = HAS_FPGA_FIR(adev)?	acq480_ffir_attrs: acq480_attrs;
		}else if (IS_PIG_CELF(adev)){
			specials[nspec++] = pig_celf_attrs;
		}else if (IS_CSPC2_COMMS(adev)){
			specials[nspec++] = cpsc2_com_attrs;
		}else{
			return;
		}
	}

	BUG_ON(nspec >= MAXSPEC);
	while(nspec--){
		if (sysfs_create_files(&dev->kobj, specials[nspec])){
			dev_err(dev, "failed to create sysfs %d", nspec);
		}
	}

	if (IS_ACQ420(adev) || IS_ACQ430(adev)){
		/* @@todo should be Master site only,
		 * but these are usually solo anyway
		 */
		if (sysfs_create_files(&dev->kobj, fmc_fp_attrs)){
			dev_err(dev, "failed to create fmc_fp_attrs");
		}
	}
}

void acq400_delSysfs(struct device *dev)
{
	sysfs_remove_files(&dev->kobj, sysfs_base_attrs);
	// @@todo .. undoing the rest will be interesting .. */
}


