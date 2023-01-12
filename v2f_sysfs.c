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

static ssize_t show_chan_sel(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 cs = acq400rd32(adev, V2F_CHAN_SEL);
	return sprintf(buf, "%d,%d,%d,%d\n",
			V2F_CHAN_SEL_DEC(cs, 1), V2F_CHAN_SEL_DEC(cs, 2),
			V2F_CHAN_SEL_DEC(cs, 3), V2F_CHAN_SEL_DEC(cs, 4));
}
static ssize_t store_chan_sel(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int sel[4];
	if (sscanf(buf, "%u,%u,%u,%u", sel+0, sel+1, sel+2, sel+3) == 4){
		u32 cs = V2F_CHAN_SEL_ENC(1, sel[0]) +
			 V2F_CHAN_SEL_ENC(2, sel[1]) +
			 V2F_CHAN_SEL_ENC(3, sel[2]) +
			 V2F_CHAN_SEL_ENC(4, sel[3]);
		acq400wr32(adev, V2F_CHAN_SEL, cs);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(chan_sel,
		S_IRUGO|S_IWUSR, show_chan_sel, store_chan_sel);

static ssize_t show_hf(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, V2F_CTRL);
	return sprintf(buf, "%d\n", (ctrl&V2F_CTRL_RANGE_HI) != 0);
}
static ssize_t store_hf(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned hf;
	if (sscanf(buf, "%u", &hf) == 1){
		u32 ctrl = acq400rd32(adev, V2F_CTRL);
		if (hf){
			ctrl |= V2F_CTRL_RANGE_HI;
		}else{
			ctrl &= ~V2F_CTRL_RANGE_HI;
		}
		acq400wr32(adev, V2F_CTRL, ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(hf, S_IRUGO|S_IWUSR, show_hf, store_hf);

/*
#define V2F_OFFSET_PACKED_1M 	3277
#define V2F_OFFSET_UNPACKED_1M 	838861

static ssize_t show_freq_offset(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 freq_off = acq400rd32(adev, V2F_FREQ_OFF);
	unsigned mstep = adev->data32? V2F_OFFSET_UNPACKED_1M: V2F_OFFSET_PACKED_1M;

	return sprintf(buf, "%u\n", freq_off/mstep);
}


static ssize_t store_freq_offset(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned mstep = adev->data32? V2F_OFFSET_UNPACKED_1M: V2F_OFFSET_PACKED_1M;
	u32 freq_off;

	if (sscanf(buf, "%u", &freq_off) == 1){
		if (freq_off > 4) freq_off = 4;

		acq400wr32(adev, V2F_FREQ_OFF, freq_off*mstep);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(freq_offset,
		S_IRUGO|S_IWUSR, show_freq_offset, store_freq_offset);
*/
static ssize_t show_freq_offset_raw(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 freq_off = acq400rd32(adev, V2F_FREQ_OFF);

	return sprintf(buf, "%u\n", freq_off);
}


static ssize_t store_freq_offset_raw(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 freq_off;

	if (sscanf(buf, "%u", &freq_off) == 1){
		if (freq_off > V2F_FREQ_OFF_MAX) freq_off = V2F_FREQ_OFF_MAX;
		acq400wr32(adev, V2F_FREQ_OFF, freq_off);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(freq_offset_raw,
		S_IRUGO|S_IWUSR, show_freq_offset_raw, store_freq_offset_raw);


static ssize_t show_reg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int reg_off)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 reg = acq400rd32(adev, reg_off);
	return sprintf(buf, "%u\n", reg);
}

static ssize_t store_reg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int reg_off,
	const unsigned RMAX)
{
	u32 reg;
	if (sscanf(buf, "0x%x", &reg) == 1 || sscanf(buf, "%u", &reg) == 1){
		struct acq400_dev* adev = acq400_devices[dev->id];
		if (reg > RMAX) reg = RMAX;
		acq400wr32(adev, reg_off, reg);
		return count;
	}else{
		return -1;
	}
}

#define MAKE_REG_CAL(kname, REG, MAX)							\
static ssize_t show_reg_##kname(							\
	struct device *dev,								\
	struct device_attribute *attr,							\
	char* buf)									\
{											\
	return show_reg(dev, attr, buf, REG);						\
}											\
static ssize_t store_reg_##kname(							\
	struct device *dev, 								\
	struct device_attribute *attr,							\
	const char* buf,								\
	size_t count)									\
{											\
	return store_reg(dev, attr, buf, count, REG, MAX);				\
}											\
static DEVICE_ATTR(kname, S_IRUGO|S_IWUSR, show_reg_##kname, store_reg_##kname)


MAKE_REG_CAL(v2f_freq_off_1, V2F_FREQ_OFF+0x0, V2F_FREQ_OFF_MAX);
MAKE_REG_CAL(v2f_freq_off_2, V2F_FREQ_OFF+0x4, V2F_FREQ_OFF_MAX);
MAKE_REG_CAL(v2f_freq_off_3, V2F_FREQ_OFF+0x8, V2F_FREQ_OFF_MAX);
MAKE_REG_CAL(v2f_freq_off_4, V2F_FREQ_OFF+0xc, V2F_FREQ_OFF_MAX);

MAKE_REG_CAL(v2f_freq_slo_1, V2F_FREQ_SLO+0x0, V2F_FREQ_SLO_MAX);
MAKE_REG_CAL(v2f_freq_slo_2, V2F_FREQ_SLO+0x4, V2F_FREQ_SLO_MAX);
MAKE_REG_CAL(v2f_freq_slo_3, V2F_FREQ_SLO+0x8, V2F_FREQ_SLO_MAX);
MAKE_REG_CAL(v2f_freq_slo_4, V2F_FREQ_SLO+0xc, V2F_FREQ_SLO_MAX);

const struct attribute *sysfs_v2f_attrs[] = {
	&dev_attr_hf.attr,
	//&dev_attr_freq_offset.attr,
	&dev_attr_freq_offset_raw.attr,
	&dev_attr_chan_sel.attr,

	&dev_attr_v2f_freq_off_1.attr,
	&dev_attr_v2f_freq_off_2.attr,
	&dev_attr_v2f_freq_off_3.attr,
	&dev_attr_v2f_freq_off_4.attr,

	&dev_attr_v2f_freq_slo_1.attr,
	&dev_attr_v2f_freq_slo_2.attr,
	&dev_attr_v2f_freq_slo_3.attr,
	&dev_attr_v2f_freq_slo_4.attr,
	NULL
};

static long long qen_count64;		/* @@TODO MOVE me to special acq400_dev subclass */

static ssize_t show_qen_count64(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	return sprintf(buf, "%lld\n", qen_count64);
}
static DEVICE_ATTR(qen_count64, S_IRUGO, show_qen_count64, 0);

static ssize_t show_qen_count(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 count = acq400rd32(adev, QEN_ENC_COUNT);
	qen_count64 += count;				/* hmm, how do we count downwards .. */
	return sprintf(buf, "%u\n", count);
}

static ssize_t store_qen_count(
	struct device *dev,
	struct device_attribute *attr,
	const char* buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, QEN_CTRL);
	acq400wr32(adev, QEN_CTRL, ctrl | QEN_CTRL_RESET);
	acq400wr32(adev, QEN_CTRL, ctrl);
	qen_count64 = 0;
	return count;
}
static DEVICE_ATTR(qen_count, S_IRUGO|S_IWUSR, show_qen_count, store_qen_count);

MAKE_BITS(ctr_reset,  	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_CTR_RESET);
MAKE_BITS(msb_direct, 	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_MSBDIRECT);
MAKE_BITS(phaseA_en,  	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_PA_EN);
MAKE_BITS(phaseB_en,  	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_PB_EN);
MAKE_BITS(Zcount_en,  	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_ZCOUNT);
MAKE_BITS(DImon_snap, 	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_SNAP32);
MAKE_BITS(index_home_en, QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_ZSEL );
MAKE_BITS(dio_outputs,	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_DIR_OUT);
MAKE_BITS(DO4,	      	QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_DO_IMM);
MAKE_BITS(di4_mon,    	QEN_DI_MON,   MAKE_BITS_FROM_MASK, 0x0f);

MAKE_BITS(abs_trg_en, QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_ABS_TRG_EN);
MAKE_BITS(prd_trg_en, QEN_DIO_CTRL, MAKE_BITS_FROM_MASK, QEN_DIO_CTRL_PRD_TRG_EN);

MAKE_DNUM(zcount, 	 QEN_ZCOUNT, 		0xffffffff);
MAKE_DNUM(ecount, 	 QEN_ECOUNT, 		0xffffffff);
MAKE_DNUM(abs_trg_count, QEN_POS_ABS_TRG,	0xffffffff);
MAKE_DNUM(prd_trg_count, QEN_POS_PRD_TRG,	0xffffffff);
MAKE_DNUM(prd_hyst_count, QEN_POS_PRD_HYST,	0xffffffff);
MAKE_DNUM(pos_prd_count, QEN_POS_PRD_CNT, 	0xffffffff);
MAKE_DNUM(index_home,    QEN_INDEX_HOME,        0xffffffff);

const struct attribute *sysfs_qen_attrs[] = {
	&dev_attr_dio_outputs.attr,
	&dev_attr_DO4.attr,

	&dev_attr_phaseA_en.attr,
	&dev_attr_phaseB_en.attr,
	&dev_attr_index_home_en.attr,
	&dev_attr_ctr_reset.attr,
	&dev_attr_Zcount_en.attr,
	&dev_attr_msb_direct.attr,
	&dev_attr_DImon_snap.attr,
	&dev_attr_abs_trg_en.attr,
	&dev_attr_prd_trg_en.attr,

	&dev_attr_di4_mon.attr,
	&dev_attr_qen_count.attr,
	&dev_attr_qen_count64.attr,
	&dev_attr_zcount.attr,
	&dev_attr_ecount.attr,
	&dev_attr_abs_trg_count.attr,
	&dev_attr_prd_trg_count.attr,
	&dev_attr_prd_hyst_count.attr,
	&dev_attr_pos_prd_count.attr,
	&dev_attr_index_home.attr,
	NULL
};


#define ACQ1014_REG(FUN, fun)							\
static ssize_t show_acq1014_##fun(						\
	struct device * dev,							\
	struct device_attribute *attr,						\
	char * buf)								\
{										\
	struct acq400_dev *adev = acq400_devices[dev->id];			\
	unsigned cr = acq400rd32(adev, DIO1014_CR);				\
	unsigned sr = acq400rd32(adev, DIO1014_SR);				\
										\
	return sprintf(buf, "%d\n", (sr&DIO1014_SR_RP_CONN)? ACQ1014_RP:	\
				    (cr&DIO1014_CR_##FUN)?   ACQ1014_LOC:	\
						    	     ACQ1014_FP);	\
}										\
										\
static ssize_t store_acq1014_##fun(						\
	struct device *dev,							\
	struct device_attribute *attr,						\
	const char* buf,							\
	size_t count)								\
{										\
	struct acq400_dev *adev = acq400_devices[dev->id];			\
	unsigned cr = acq400rd32(adev, DIO1014_CR);				\
	int en;									\
	if (sscanf(buf, "%d", &en) == 1){					\
		if (en){							\
			cr |= DIO1014_CR_##FUN;					\
		}else{								\
			cr &= ~DIO1014_CR_##FUN;				\
		}								\
		acq400wr32(adev, DIO1014_CR, cr);				\
		return count;							\
	}									\
	return -1;								\
}										\
static DEVICE_ATTR(acq1014_##fun, S_IRUGO|S_IWUSR, show_acq1014_##fun, store_acq1014_##fun);

ACQ1014_REG(CLK, clk);
ACQ1014_REG(TRG, trg);


const struct attribute *sysfs_acq1014_attrs[] = {
	&dev_attr_acq1014_clk.attr,
	&dev_attr_acq1014_trg.attr,
	NULL
};

static ssize_t store_DELTRGN(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int ix)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	int threshold;
	if (sscanf(buf, "%d", &threshold) == 1){
		return acq400_setDelTrg(adev, ix, threshold) == 0? count: -1;
	}
	return -1;
}

static ssize_t show_DELTRGN(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int ix)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	int threshold;
	if (acq400_getDelTrg(adev, ix, &threshold) == 0){
		return sprintf(buf, "%d\n", threshold);
	}else{
		return -1;
	}
	return 0;
}

#define MAKE_DELTRG(IXO, IX)						\
static ssize_t show_DELTRG##IXO(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_DELTRGN(dev, attr, buf, IX-1);			\
}									\
									\
static ssize_t store_DELTRG##IXO(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_DELTRGN(dev, attr, buf, count, IX-1);		\
}									\
static DEVICE_ATTR(deltrg_##IXO, S_IRUGO|S_IWUSR, 			\
		show_DELTRG##IXO, store_DELTRG##IXO)


MAKE_DELTRG(01, 1);
MAKE_DELTRG(02, 2);
MAKE_DELTRG(03, 3);
MAKE_DELTRG(04, 4);
MAKE_DELTRG(05, 5);
MAKE_DELTRG(06, 6);
MAKE_DELTRG(07, 7);
MAKE_DELTRG(08, 8);


MAKE_BITS(atd_triggered, ATD_TRIGGERED, 0, 0xffffffff);
MAKE_BITS(atd_OR, 	 ATD_MASK_OR,   0, 0xffffffff);
MAKE_BITS(atd_AND, 	 ATD_MASK_AND,  0, 0xffffffff);
MAKE_BITS(dtd_ZN, 	 DTD_CTRL,  	0, DTD_CTRL_ZN);
MAKE_BITS(dtd_CLR,       DTD_CTRL,  	0, DTD_CTRL_CLR);





static ssize_t show_atd_triggered_display(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XTD_dev *xtd_dev = container_of(adev, struct XTD_dev, adev);

	return sprintf(buf, "0x%08x\n", xtd_dev->atd_display.event_source);

}
static DEVICE_ATTR(atd_triggered_display, S_IRUGO, show_atd_triggered_display, 0);

const struct attribute *atd_attrs[] = {
	&dev_attr_atd_triggered.attr,
	&dev_attr_atd_triggered_display.attr,
	&dev_attr_atd_OR.attr,
	&dev_attr_atd_AND.attr,
	NULL
};

const struct attribute *dtd_attrs[] = {
	&dev_attr_deltrg_01.attr,
	&dev_attr_deltrg_02.attr,
	&dev_attr_deltrg_03.attr,
	&dev_attr_deltrg_04.attr,
	&dev_attr_deltrg_05.attr,
	&dev_attr_deltrg_06.attr,
	&dev_attr_deltrg_07.attr,
	&dev_attr_deltrg_08.attr,
	&dev_attr_dtd_ZN.attr,
	&dev_attr_dtd_CLR.attr,
	&dev_attr_atd_triggered.attr,
	&dev_attr_atd_triggered_display.attr,
	NULL
};



