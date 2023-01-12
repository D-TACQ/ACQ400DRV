/* wr_sysfs.c */
/* ------------------------------------------------------------------------- */
/* wr_sysfs.c D-TACQ ACQ400 series driver, sysfs (knobs)	             */
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

static ssize_t show_wr_tai_cur_raw(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 tl = acq400rd32(adev, WR_TAI_CUR_L);

	return sprintf(buf, "0x%08x %x\n", tl, tl&0x7);
}

static DEVICE_ATTR(wr_tai_cur_raw, S_IRUGO, show_wr_tai_cur_raw, 0);

static ssize_t _store_wr_tai_trg(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	unsigned reg)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	u32 tai_trg;
	u32 tai_sec, clocks;

	if (sscanf(buf, "%u,%u", &tai_sec, &clocks) == 2){
		tai_trg = tai_sec << 28 | clocks;
		if (tai_trg != 0){
			tai_trg |= WR_TAI_TRG_EN;
		}
		acq400wr32(adev, reg, tai_trg);
		return count;
	} else if (sscanf(buf, "%u", &tai_trg) == 1){
		if (tai_trg != 0){
			tai_trg |= WR_TAI_TRG_EN;
		}
		acq400wr32(adev, reg, tai_trg);
		return count;
	}else{
		return -1;
	}
}

static ssize_t _show_wr_tai_trg(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned reg)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	unsigned tai_trg = acq400rd32(adev, reg);
	return sprintf(buf, "0x%08x %u %u\n", tai_trg, (tai_trg>>28)&0x7, tai_trg&0x0fffffff);
}

#define MAKE_WR_TAI_TRG(name, reg)								\
static ssize_t store_wr_tai_##name(								\
	struct device * dev,									\
	struct device_attribute *attr,								\
	const char * buf,									\
	size_t count)										\
{												\
	return _store_wr_tai_trg(dev, attr, buf, count, reg);					\
}												\
static ssize_t show_wr_tai_##name(								\
	struct device * dev,									\
	struct device_attribute *attr,								\
	char * buf)										\
{												\
	return _show_wr_tai_trg(dev, attr, buf, reg);						\
}												\
static DEVICE_ATTR(wr_tai_##name, S_IRUGO|S_IWUSR, show_wr_tai_##name, store_wr_tai_##name)

MAKE_WR_TAI_TRG(trg, WR_TAI_TRG0);
MAKE_WR_TAI_TRG(trg1, WR_TAI_TRG1);

static ssize_t show_wr_stamp(
	struct device * dev,
	struct device_attribute *attr,
	char * buf, u32 reg)
{
	struct acq400_dev* adev = acq400_devices[dev->id];
	unsigned tai_stamp = acq400rd32(adev, reg);
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

MAKE_BITS(wr_link_up, 	 WR_TAI_CUR_H, MAKE_BITS_FROM_MASK, WR_TAI_CUR_H_LINKUP);
MAKE_BITS(wr_time_valid, WR_TAI_CUR_H, MAKE_BITS_FROM_MASK, WR_TAI_CUR_H_TIMEVALID);

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
MAKE_WR_EVENT_COUNT(wrtt_client0);
MAKE_WR_EVENT_COUNT(wrtt_client1);

const struct attribute *acq2106_wr_attrs[] = {
	&dev_attr_wr_clk_pv.attr,
	&dev_attr_wr_clk_pv3.attr,
	&dev_attr_wr_trg_src.attr,
	&dev_attr_wr_tai_cur.attr,
	&dev_attr_wr_tai_cur_raw.attr,
	&dev_attr_wr_tai_trg.attr,
	&dev_attr_wr_tai_trg1.attr,
	&dev_attr_wr_tai_stamp.attr,
	&dev_attr_wr_cur_vernier.attr,
	&dev_attr_wr_pps_client_count.attr,
	&dev_attr_wr_ts_client_count.attr,
	&dev_attr_wr_wrtt_client0_count.attr,
	&dev_attr_wr_wrtt_client1_count.attr,

	&dev_attr_wr_link_up.attr,
	&dev_attr_wr_time_valid.attr,
	NULL
};


#define MAKE_TIGA_COUNT(site, FUN) \
static ssize_t store_wr_##FUN##_s##site##count(							\
	struct device * dev,									\
	struct device_attribute *attr,								\
	const char * buf,									\
	size_t count)										\
{												\
	struct acq400_dev* adev = acq400_devices[dev->id];					\
	struct acq400_tiga_dev* tiga_dev = container_of(adev, struct acq400_tiga_dev, sc_dev.adev);\
	unsigned clear;										\
	if (sscanf(buf, "%u", &clear) == 1 && clear==1){					\
		tiga_dev->FUN##_clients[site-1].wc_count = 0;					\
		return count;									\
	}else{											\
		return -1;									\
	}											\
}												\
static ssize_t show_wr_##FUN##_s##site##count(							\
	struct device * dev,									\
	struct device_attribute *attr,								\
	char * buf)										\
{												\
	struct acq400_dev* adev = acq400_devices[dev->id];					\
	struct acq400_tiga_dev* tiga_dev = container_of(adev, struct acq400_tiga_dev, sc_dev.adev);\
	return sprintf(buf, "%u\n", tiga_dev->FUN##_clients[site-1].wc_count);			\
}												\
static DEVICE_ATTR(wr_##FUN##_s##site##_count, S_IRUGO|S_IWUSR, show_wr_##FUN##_s##site##count, store_wr_##FUN##_s##site##count)

#define MAKE_TIGA_REGS(site) \
MAKE_WR_TAI_TRG(trg_s##site, WR_TT_S(site));							\
static ssize_t show_wr_tai_stamp_s##site(							\
	struct device * dev,									\
	struct device_attribute *attr,								\
	char * buf)										\
{												\
	return show_wr_stamp(dev, attr, buf, WR_TS_S(site));					\
}												\
static DEVICE_ATTR(wr_tai_stamp_s##site, S_IRUGO, show_wr_tai_stamp_s##site, 0);		\
MAKE_TIGA_COUNT(site, ts);									\
MAKE_TIGA_COUNT(site, tt)


MAKE_TIGA_REGS(1);
MAKE_TIGA_REGS(2);
MAKE_TIGA_REGS(3);
MAKE_TIGA_REGS(4);
MAKE_TIGA_REGS(5);
MAKE_TIGA_REGS(6);

const struct attribute *acq2106_tiga_attrs[] = {
	&dev_attr_wr_tai_trg_s1.attr,
	&dev_attr_wr_tai_trg_s2.attr,
	&dev_attr_wr_tai_trg_s3.attr,
	&dev_attr_wr_tai_trg_s4.attr,
	&dev_attr_wr_tai_trg_s5.attr,
	&dev_attr_wr_tai_trg_s6.attr,
	&dev_attr_wr_tai_stamp_s1.attr,
	&dev_attr_wr_tai_stamp_s2.attr,
	&dev_attr_wr_tai_stamp_s3.attr,
	&dev_attr_wr_tai_stamp_s4.attr,
	&dev_attr_wr_tai_stamp_s5.attr,
	&dev_attr_wr_tai_stamp_s6.attr,

	&dev_attr_wr_ts_s1_count.attr,
	&dev_attr_wr_ts_s2_count.attr,
	&dev_attr_wr_ts_s3_count.attr,
	&dev_attr_wr_ts_s4_count.attr,
	&dev_attr_wr_ts_s5_count.attr,
	&dev_attr_wr_ts_s6_count.attr,
	&dev_attr_wr_tt_s1_count.attr,
	&dev_attr_wr_tt_s2_count.attr,
	&dev_attr_wr_tt_s3_count.attr,
	&dev_attr_wr_tt_s4_count.attr,
	&dev_attr_wr_tt_s5_count.attr,
	&dev_attr_wr_tt_s6_count.attr,
	NULL
};

