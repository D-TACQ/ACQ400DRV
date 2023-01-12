/* ------------------------------------------------------------------------- */
/* xo_sysfs.c ACQ420_FMC						     */
/* ------------------------------------------------------------------------- */
/* xo_sysfs.c  D-TACQ ACQ400 FMC  DRIVER
 * Project: ACQ420_FMC
 * Created: 3 May 2018
 * User: pgm
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
#include "bolo.h"
#include "hbm.h"

#include "acq400_sysfs.h"
#include "sysfs_attrs.h"


int hook_dac_gx_to_spad;
module_param(hook_dac_gx_to_spad, int, 0644);
MODULE_PARM_DESC(hook_dac_gx_to_spad, "1: writes to DAC GX mirrored to SPADx");

int ao424_16 = 0;
module_param(ao424_16, int, 0644);
MODULE_PARM_DESC(ao424_16, "ao424, 16 channel mode only");

static ssize_t show_playloop_length(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u %u %s\n",
			xo_dev->AO_playloop.length,
			xo_dev->AO_playloop.oneshot,
			xo_dev->AO_playloop.oneshot == AO_oneshot? "ONESHOT":
				xo_dev->AO_playloop.oneshot == AO_oneshot_rearm? "ONESHOT-REARM": "");
}

static ssize_t store_playloop_length(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned playloop_length;
	unsigned one_shot;
	int rc;

	dev_dbg(DEVP(adev), "store_playloop_length \"%s\"", buf);

	switch(sscanf(buf, "%u %u", &playloop_length, &one_shot)){
	case 2:
		switch(one_shot){
		default:
			return -1;
		case AO_continuous:
		case AO_oneshot:
		case AO_oneshot_rearm:
			xo_dev->AO_playloop.oneshot = one_shot; /* fall thru */
		}
	case 1:
		if (xo_dev->AO_playloop.length == 0 || playloop_length < xo_dev->AO_playloop.length){
			rc = xo400_reset_playloop(adev, playloop_length);
		}else{
			/* append in shot possible */
			xo_dev->AO_playloop.length = playloop_length;
		}
		if (xo_dev->AO_playloop.oneshot != AO_continuous){
			xo_dev->AO_playloop.repeats = 0;
		}
		if (rc == 0){
			return count;
		}else{
			return rc;
		}
	default:
		return -1;
	}
}

static DEVICE_ATTR(playloop_length,
		S_IRUGO|S_IWUSR, show_playloop_length, store_playloop_length);




static ssize_t show_playloop_oneshot(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.oneshot);
}

static ssize_t store_playloop_oneshot(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (sscanf(buf, "%u", &xo_dev->AO_playloop.oneshot) == 1){
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(playloop_oneshot,
		S_IRUGO|S_IWUSR, show_playloop_oneshot, store_playloop_oneshot);


static ssize_t show_playloop_maxlen(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.maxlen);
}

static ssize_t store_playloop_maxlen(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (sscanf(buf, "%u", &xo_dev->AO_playloop.maxlen) == 1){
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(playloop_maxlen,
		S_IRUGO|S_IWUSR, show_playloop_maxlen, store_playloop_maxlen);

static ssize_t show_playloop_maxshot(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.maxshot);
}

static ssize_t store_playloop_maxshot(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (sscanf(buf, "%u", &xo_dev->AO_playloop.maxshot) == 1){
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(playloop_maxshot,
		S_IRUGO|S_IWUSR, show_playloop_maxshot, store_playloop_maxshot);

static ssize_t show_playloop_cursor(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.cursor);
}

static ssize_t store_playloop_cursor(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (sscanf(buf, "%u", &xo_dev->AO_playloop.cursor) == 1){
		return count;
	}else{
		return -1;
	}
}




static DEVICE_ATTR(playloop_cursor,
		S_IRUGO|S_IWUSR, show_playloop_cursor, store_playloop_cursor);



static ssize_t show_pull_buf(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.pull_buf);
}

static DEVICE_ATTR(playloop_pull_buf, S_IRUGO, show_pull_buf, 0);


static ssize_t show_push_buf(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.push_buf);
}

static ssize_t store_push_buf(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (sscanf(buf, "%u", &xo_dev->AO_playloop.push_buf) == 1){
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(playloop_push_buf,
		S_IRUGO|S_IWUSR, show_push_buf, store_push_buf);

static ssize_t show_playloop_repeats(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%u\n", xo_dev->AO_playloop.cycles);
}

static ssize_t store_playloop_repeats(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (sscanf(buf, "%u", &xo_dev->AO_playloop.repeats) == 1){
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(playloop_repeats,
		S_IRUGO|S_IWUSR, show_playloop_repeats, store_playloop_repeats);


static ssize_t show_xo_buffers(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%u %u\n", adev->stats.xo.dma_buffers_out,
						adev->stats.xo.dma_buffers_in);
}
static DEVICE_ATTR(xo_buffers, S_IRUGO, show_xo_buffers, 0);

static ssize_t show_dac_fifo_sta(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 fifo_sta = acq400rd32(adev, DAC_FIFO_STA);
	acq400wr32(adev, DAC_FIFO_STA, fifo_sta);
	return sprintf(buf, "%02x\n", fifo_sta);
}
static DEVICE_ATTR(dac_fifo_sta, S_IRUGO, show_dac_fifo_sta, 0);


static ssize_t show_awg_state_arm(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dac_en =
		acq400rd32(adev, DAC_CTRL)&DAC_CTRL_DAC_EN;
	unsigned conv_active =
		acq400rd32(adev, DAC_FIFO_STA)&ADC_FIFO_STA_ACTIVE;


	return sprintf(buf, "%d\n", dac_en && !conv_active);
}
static DEVICE_ATTR(awg_state_arm, S_IRUGO, show_awg_state_arm, 0);




static ssize_t store_ao_reset_fifo(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned reset;

	if (sscanf(buf, "%u", &reset) == 1 && reset == 1){
		ao420_reset_fifo(adev);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(__reset_fifo, S_IWUSR, 0, store_ao_reset_fifo);

MAKE_BITS(awg_abort, DAC_CTRL, MAKE_BITS_FROM_MASK, DAC_CTRL_AWG_ABORT);

static ssize_t show_awg_stream_buffers(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct AOPlayloop* pl = &xo_dev->AO_playloop;

	return sprintf(buf, "awg_stream:%d,%d,%d,%d\n", pl->push_buf, pl->pull_buf, pl->first_buf, pl->last_buf);
}
static DEVICE_ATTR(awg_stream_buffers, S_IRUGO, show_awg_stream_buffers, 0);

static ssize_t show_dwg_status(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned do_count = acq400rd32(adev, DIO432_DIO_SAMPLE_COUNT);
	unsigned fifsta = acq400rd32(adev, DIO432_DO_FIFO_STATUS);
	unsigned state =
		xo_dev->AO_playloop.length > 0 && (fifsta&ADC_FIFO_STA_EMPTY)==0?
		do_count > 0? 2: 1: 0;	/* RUN, ARM, IDLE */

	return sprintf(buf, "%d %d\n", state, do_count);
}


static DEVICE_ATTR(dwg_status, S_IRUGO|S_IWUSR, show_dwg_status, 0);


const struct attribute *playloop_attrs[] = {
	&dev_attr_playloop_length.attr,
	&dev_attr_playloop_oneshot.attr,
	&dev_attr_playloop_maxshot.attr,
	&dev_attr_playloop_cursor.attr,
	&dev_attr_playloop_repeats.attr,
	&dev_attr_playloop_maxlen.attr,
	&dev_attr_playloop_push_buf.attr,
	&dev_attr_playloop_pull_buf.attr,
	&dev_attr_awg_stream_buffers.attr,
	&dev_attr_xo_buffers.attr,
	&dev_attr_awg_abort.attr,
	&dev_attr_awg_state_arm.attr,
	&dev_attr_dac_fifo_sta.attr,
	&dev_attr_dwg_status.attr,
	&dev_attr___reset_fifo.attr,
	NULL
};

static ssize_t show_dac_headroom(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{

	struct acq400_dev *adev = acq400_devices[dev->id];
	int hr_samples = ao420_getFifoHeadroom(adev);
	return sprintf(buf, "%d samples %d bytes\n",
			hr_samples, AOSAMPLES2BYTES(adev, hr_samples));
}

static DEVICE_ATTR(dac_headroom, S_IRUGO, show_dac_headroom, 0);

static ssize_t show_dac_fifo_samples(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int fifo_samples = xo_dev->xo.getFifoSamples(adev);
	return sprintf(buf, "%d samples %d bytes\n",
			fifo_samples, AOSAMPLES2BYTES(adev, fifo_samples));
}

static DEVICE_ATTR(dac_fifo_samples, S_IRUGO, show_dac_fifo_samples, 0);

static ssize_t show_dac_range(
	int chan,
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 ranges = acq400rd32(acq400_devices[dev->id], AO420_RANGE);

	return sprintf(buf, "%u\n", ranges>>chan & 1);
}

static ssize_t store_dac_range(
		int shl,
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count)
{
	int gx;

	if (sscanf(buf, "%d", &gx) == 1){
		u32 ranges = acq400rd32(acq400_devices[dev->id], AO420_RANGE);
		unsigned bit = 1 << shl;
		if (gx){
			ranges |= bit;
		}else{
			ranges &= ~bit;
		}

		dev_dbg(dev, "set gain: %02x", ranges);
		acq400wr32(acq400_devices[dev->id], AO420_RANGE, ranges);
		return count;
	}else{
		dev_warn(dev, "rejecting input args != 4");
		return -1;
	}
}

#define MAKE_DAC_RANGE(NAME, SHL)					\
static ssize_t show_dac_range##NAME(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_dac_range(SHL, dev, attr, buf);			\
}									\
									\
static ssize_t store_dac_range##NAME(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_dac_range(SHL, dev, attr, buf, count);		\
}									\
static DEVICE_ATTR(dac_range_##NAME, S_IRUGO|S_IWUSR, 			\
		show_dac_range##NAME, store_dac_range##NAME)

MAKE_DAC_RANGE(01,  ao420_physChan(1));
MAKE_DAC_RANGE(02,  ao420_physChan(2));
MAKE_DAC_RANGE(03,  ao420_physChan(3));
MAKE_DAC_RANGE(04,  ao420_physChan(4));
MAKE_DAC_RANGE(REF, 4);


/*
 * GO : Gain + Offset
 * Create a set of knobs G1..GN, D1..DN
 */

static ssize_t show_ao_GO(
	const int CH, const int SHL,
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	u32 go = acq400rd32(acq400_devices[dev->id], DAC_GAIN_OFF(CH));

	return sprintf(buf, "%u\n", go>>SHL & 0x0000ffff);
}

void set_spad_gx(struct acq400_dev* adev, int CH0, unsigned go)
/* cross couple gx setting to SPAD. CH 0..3 */
{
	if (hook_dac_gx_to_spad < 0){
		/* hardware didn't match original? */
		unsigned tmp = go&0x0000ffff;
		go >>= 16;
		go |= tmp << 16;
	}
	set_spadN(adev, CH0+4, go);
}


static ssize_t store_ao_GO(
		const int CH, const int SHL,
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count, int ao_min, int ao_max)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	int gx;

	if (sscanf(buf, "%d", &gx) == 1 || sscanf(buf, "0x%x", &gx) == 1){
		u32 go = acq400rd32(acq400_devices[dev->id], DAC_GAIN_OFF(CH));

		if (gx > ao_max) gx = ao_max;
		if (gx < ao_min) gx = ao_min;

		go &= ~(0x0000ffff << SHL);
		go |= (gx&0x0000ffff) << SHL;

		acq400wr32(adev, DAC_GAIN_OFF(CH), go);

		if (abs(hook_dac_gx_to_spad) == adev->of_prams.site){
			set_spad_gx(acq400_devices[0], CH-1, go);
		}
		return count;
	}else{
		dev_warn(dev, "rejecting input args != 4");
		return -1;
	}
}

#define _MAKE_AO_GO(NAME, SHL, CHAN, AO_MIN, AO_MAX)			\
static ssize_t show_ao_GO##NAME(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_ao_GO(CHAN, SHL, dev, attr, buf);			\
}									\
									\
static ssize_t store_ao_GO##NAME(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_ao_GO(CHAN, SHL, dev, attr, buf, count, AO_MIN, AO_MAX);\
}									\
static DEVICE_ATTR(NAME, S_IRUGO|S_IWUSR, 			        \
		show_ao_GO##NAME, store_ao_GO##NAME)

#define MAKE_AO_GO(CHAN) \
	_MAKE_AO_GO(G##CHAN, DAC_MATH_GAIN_SHL, CHAN, 0, 32768);    \
	_MAKE_AO_GO(D##CHAN, DAC_MATH_OFFS_SHL, CHAN, -32767, 32768)

MAKE_AO_GO(1);
MAKE_AO_GO(2);
MAKE_AO_GO(3);
MAKE_AO_GO(4);



static void ao420_flushImmediate(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned *src = xo_dev->AO_immediate._u.lw;
	void *fifo = adev->dev_virtaddr + AXI_FIFO;
	int imax = adev->nchan_enabled*adev->word_size/sizeof(long);
	int ii = 0;

	dev_dbg(DEVP(adev), "ao420_flushImmediate() 01 imax %d", imax);
	for (ii = 0; ii < imax; ++ii){
		dev_dbg(DEVP(adev), "fifo write: %p = 0x%08x\n",
				fifo + ii*sizeof(unsigned), src[ii]);
		iowrite32(src[ii], fifo + ii*sizeof(unsigned));
	}
	dev_dbg(DEVP(adev), "ao420_flushImmediate() 99 ii %d", ii);
}

static ssize_t show_dac_immediate(
	int chan,
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	short chx;
	int pchan = xo_dev->xo.physchan(chan);

	if (adev->word_size == sizeof(long)){
		 chx = xo_dev->AO_immediate._u.lw[pchan];
		 return sprintf(buf, "0x%08x %d\n", chx, chx);
	}else{
		chx = xo_dev->AO_immediate._u.ch[pchan];
		if (IS_AO424(adev)){
			chx = ao424_fixEncoding(adev, pchan, chx);
		}
		return sprintf(buf, "0x%04x %d\n", chx, chx);
	}
}

extern int no_ao42x_llc_ever;

static ssize_t store_dac_immediate(
		int chan,
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int chx;
	int pchan = xo_dev->xo.physchan(chan);

	if (sscanf(buf, "0x%x", &chx) == 1 || sscanf(buf, "%d", &chx) == 1){
		unsigned cr = acq400rd32(adev, DAC_CTRL);
		if (IS_AO424(adev)){
			chx = ao424_fixEncoding(adev, pchan, chx);
		}
		if (adev->word_size == sizeof(long)){
			xo_dev->AO_immediate._u.lw[pchan] = chx;
		}else{
			xo_dev->AO_immediate._u.ch[pchan] = chx;
		}
		if (!no_ao42x_llc_ever){
			acq400wr32(adev, DAC_CTRL, cr|DAC_CTRL_LL|DAC_CTRL_ENABLE_ALL);
			ao420_flushImmediate(adev);
		}else{
			dev_dbg(DEVP(adev), "store_dac_immediate STUB no_ao42x_llc_ever set");
		}
		return count;
	}else{
		dev_warn(dev, "rejecting input args != 0x%%04x or %%d");
		return -1;
	}
}

#define MAKE_DAC_IMMEDIATE(NAME, CH)					\
static ssize_t show_dac_immediate_##NAME(				\
	struct device * dev,						\
	struct device_attribute *attr,					\
	char * buf)							\
{									\
	return show_dac_immediate(CH, dev, attr, buf);			\
}									\
									\
static ssize_t store_dac_immediate_##NAME(				\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_dac_immediate(CH, dev, attr, buf, count);		\
}									\
static DEVICE_ATTR(AO_##NAME, S_IRUGO|S_IWUSR, 			\
		show_dac_immediate_##NAME, store_dac_immediate_##NAME)

MAKE_DAC_IMMEDIATE(01, 1);
MAKE_DAC_IMMEDIATE(02, 2);
MAKE_DAC_IMMEDIATE(03, 3);
MAKE_DAC_IMMEDIATE(04, 4);
MAKE_DAC_IMMEDIATE(05, 5);
MAKE_DAC_IMMEDIATE(06, 6);
MAKE_DAC_IMMEDIATE(07, 7);
MAKE_DAC_IMMEDIATE(08, 8);
MAKE_DAC_IMMEDIATE(09, 9);
MAKE_DAC_IMMEDIATE(10, 10);
MAKE_DAC_IMMEDIATE(11, 11);
MAKE_DAC_IMMEDIATE(12, 12);
MAKE_DAC_IMMEDIATE(13, 13);
MAKE_DAC_IMMEDIATE(14, 14);
MAKE_DAC_IMMEDIATE(15, 15);
MAKE_DAC_IMMEDIATE(16, 16);
MAKE_DAC_IMMEDIATE(17, 17);
MAKE_DAC_IMMEDIATE(18, 18);
MAKE_DAC_IMMEDIATE(19, 19);
MAKE_DAC_IMMEDIATE(20, 20);
MAKE_DAC_IMMEDIATE(21, 21);
MAKE_DAC_IMMEDIATE(22, 22);
MAKE_DAC_IMMEDIATE(23, 23);
MAKE_DAC_IMMEDIATE(24, 24);
MAKE_DAC_IMMEDIATE(25, 25);
MAKE_DAC_IMMEDIATE(26, 26);
MAKE_DAC_IMMEDIATE(27, 27);
MAKE_DAC_IMMEDIATE(28, 28);
MAKE_DAC_IMMEDIATE(29, 29);
MAKE_DAC_IMMEDIATE(30, 30);
MAKE_DAC_IMMEDIATE(31, 31);
MAKE_DAC_IMMEDIATE(32, 32);





#define TIMEOUT 1000

#define DACSPI(adev) (IS_BOLO8(adev)? B8_DAC_SPI: AO420_DACSPI)

static int poll_dacspi_complete(struct acq400_dev *adev, u32 wv)
{
	unsigned pollcat = 0;
	while( ++pollcat < TIMEOUT){
		u32 rv = acq400rd32_nocache(adev, DACSPI(adev));
		if ((rv&AO420_DACSPI_WC) != 0){
			dev_dbg(DEVP(adev),
			"poll_dacspi_complete() success after %d\n", pollcat);
			wv &= ~(AO420_DACSPI_CW|AO420_DACSPI_WC);
			acq400wr32(adev, DACSPI(adev), wv);
			return 0;
		}
		if ((pollcat%100)==0){
			dev_warn(DEVP(adev),
					"poll_dacspi_complete %d %08x %08x\n",
					pollcat, wv, rv);
		}
	}
	dev_err(DEVP(adev), "poll_dacspi_complete() giving up, no completion");
	return -1;
}


static ssize_t show_dacspi(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 dacspi = acq400rd32(adev, DACSPI(adev));

	return sprintf(buf, "0x%08x\n", dacspi);
}

static ssize_t store_dacspi(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dacspi;
	if (sscanf(buf, "0x%x", &dacspi) == 1 || sscanf(buf, "%d", &dacspi) == 1){
		dacspi |= AO420_DACSPI_CW;
		acq400wr32(adev, DACSPI(adev), dacspi);

		if ((dacspi&AO420_DACSPI_CW) != 0){
			if (poll_dacspi_complete(adev, dacspi)){
				return -1;
			}
		}
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(dacspi, S_IWUSR, show_dacspi, store_dacspi);

static ssize_t show_delay66(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 delay66 = acq400rd32(adev, AO_DELAY66);

	return sprintf(buf, "0x%02x\n", delay66&AO_DELAY66_MASK);
}

static ssize_t store_delay66(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned delay66;
	if (sscanf(buf, "0x%x", &delay66) == 1 || sscanf(buf, "%d", &delay66) == 1){
		if (delay66 > AO_DELAY66_MASK) delay66 = AO_DELAY66_MASK;
		acq400wr32(adev, AO_DELAY66, delay66);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(delay66, S_IWUSR|S_IRUGO, show_delay66, store_delay66);


static ssize_t show_dac_dec(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 acc_dec = acq400rd32(adev, DAC_DEC);

	return sprintf(buf, "%u\n", (acc_dec&ADC_ACC_DEC_LEN)+1);
}

static ssize_t store_dac_dec(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned nacc = 0;

	if (sscanf(buf, "%u", &nacc) >= 1){
		nacc = min(nacc, ADC_MAX_NACC);
		acq400wr32(adev, DAC_DEC, nacc-1);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(dac_dec, S_IWUSR|S_IRUGO, show_dac_dec, store_dac_dec);

static ssize_t show_read_latency(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 read_lat_ac = acq400rd32(adev, DAC_READ_LAT_AC);
	u32 read_lat_mm = acq400rd32(adev, DAC_READ_LAT_MM);

	return sprintf(buf, "%d,%d,%d,%d\n",
			read_lat_ac>>16, read_lat_ac&0xffff,
			read_lat_mm>>16, read_lat_mm&0xffff);
}

static DEVICE_ATTR(read_latency, S_IRUGO, show_read_latency, 0);


MAKE_BITS(snoopsel, DAC_CTRL, AO424_DAC_CTRL_SNOOPSEL_SHL, AO424_DAC_CTRL_SNOOPSEL_MSK);

#define DAC_CTRL_RESET(adev)	(IS_BOLO8(adev)? B8_DAC_CON: DAC_CTRL)

static ssize_t show_dac_ctrl(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned bit)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dac_ctrl = acq400rd32(adev, DAC_CTRL_RESET(adev));

	return sprintf(buf, "%u\n", (dac_ctrl&bit) != 0);
}

static ssize_t store_dac_ctrl(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	unsigned bit)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dac_ctrl = acq400rd32(adev, DAC_CTRL_RESET(adev));
	unsigned dacreset;

	dac_ctrl |= ADC_CTRL_MODULE_EN;

	if (sscanf(buf, "%d", &dacreset) == 1){
		if (dacreset){
			dac_ctrl |= bit;
		}else{
			dac_ctrl &= ~bit;
		}
		acq400wr32(adev, DAC_CTRL_RESET(adev), dac_ctrl);
		return count;
	}else{
		return -1;
	}
}

#define MAKE_DAC_CTRL_BIT(NAME, BIT)							\
static ssize_t show_dac_ctrl_##NAME(							\
	struct device * dev,								\
	struct device_attribute *attr,							\
	char * buf)									\
{											\
	return show_dac_ctrl(dev, attr, buf, BIT);					\
}											\
static ssize_t store_dac_ctrl_##NAME(							\
	struct device * dev,								\
	struct device_attribute *attr,							\
	const char * buf,								\
	size_t count)									\
{											\
	return store_dac_ctrl(dev, attr, buf, count, BIT);				\
}											\
static DEVICE_ATTR(NAME, S_IRUGO|S_IWUSR, show_dac_ctrl_##NAME, store_dac_ctrl_##NAME)

MAKE_DAC_CTRL_BIT(dac_reset, 	ADC_CTRL_ADC_RST);
MAKE_DAC_CTRL_BIT(dacreset, 	ADC_CTRL_ADC_RST);
MAKE_DAC_CTRL_BIT(dac_enable, 	ADC_CTRL_ADC_EN);

static ssize_t show_dacreset_device(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dac_ctrl = acq400rd32(adev, DAC_CTRL_RESET(adev));

	return sprintf(buf, "%u\n", (dac_ctrl&ADC_CTRL_ADC_RST) != 0);
}

static ssize_t store_dacreset_device(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned dacreset_device;

	if (sscanf(buf, "%d", &dacreset_device) == 1 ||
	    sscanf(buf, "0x%u", &dacreset_device) == 1  ){
		if (ao424_set_spans(adev)){
			return -1;
		}
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(dacreset_device, S_IWUSR, show_dacreset_device, store_dacreset_device);

static ssize_t show_dac_encoding(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];

	return sprintf(buf, "%s\n",
		IS_AO420(adev)||IS_AO428(adev)? "signed": IS_AO424(adev)? "unsigned": "unknown");
}


static DEVICE_ATTR(dac_encoding, S_IRUGO, show_dac_encoding, 0);


static ssize_t show_odd_channels(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 cgen = acq400rd32(adev, DAC_424_CGEN);
	return sprintf(buf, "%d\n",  (cgen&DAC_424_CGEN_ODD_CHANS) != 0);
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
		ao424_set_odd_channels(adev, odd_chan_en);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(odd_channels,
		S_IRUGO|S_IWUSR, show_odd_channels, store_odd_channels);

static ssize_t show_twos_comp_encoding(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%d\n",  xo_dev->ao424_device_settings.encoded_twocmp);
}
static ssize_t store_twos_comp_encoding(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
/* user mask: 1=enabled. Compute nchan_enabled BEFORE inverting MASK */
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int twocmp;

	if (sscanf(buf, "%d", &twocmp) == 1){
		u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);
		xo_dev->ao424_device_settings.encoded_twocmp = twocmp != 0;
		if (xo_dev->ao424_device_settings.encoded_twocmp){
			dac_ctrl |= DAC_CTRL_TWOCMP;
		}else{
			dac_ctrl &= ~DAC_CTRL_TWOCMP;
		}

		acq400wr32(adev, DAC_CTRL, dac_ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(twos_comp_encoding,
		S_IRUGO|S_IWUSR, show_twos_comp_encoding, store_twos_comp_encoding);




const struct attribute* dacspi_attrs[] = {
	&dev_attr_dacspi.attr,
	&dev_attr_dacreset.attr,
	&dev_attr_dac_reset.attr,
	&dev_attr_dac_enable.attr,
	NULL
};

MAKE_DNUM(offset_01, AO428_OFFSET_1, 0x000fffff);
MAKE_DNUM(offset_02, AO428_OFFSET_2, 0x000fffff);
MAKE_DNUM(offset_03, AO428_OFFSET_3, 0x000fffff);
MAKE_DNUM(offset_04, AO428_OFFSET_4, 0x000fffff);
MAKE_DNUM(offset_05, AO428_OFFSET_5, 0x000fffff);
MAKE_DNUM(offset_06, AO428_OFFSET_6, 0x000fffff);
MAKE_DNUM(offset_07, AO428_OFFSET_7, 0x000fffff);
MAKE_DNUM(offset_08, AO428_OFFSET_8, 0x000fffff);

MAKE_BITS(dac_mux, DAC_MUX, 0,0x00000fff);

static ssize_t show_dac_mux_master(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "%03x\n",  acq400rd32(adev, DAC_MUX));
}

static void set_mux_action(struct acq400_dev *adev, void* arg)
{
	dev_dbg(DEVP(adev), "set_mux_action() %s %x",
			IS_AO420(adev) && IS_AO420_HALF436(adev)? "ACT": "---", (unsigned)arg);
	if (IS_AO420(adev) && IS_AO420_HALF436(adev)){
		acq400wr32(adev, DAC_MUX, (unsigned)arg);
	}
}
static ssize_t store_dac_mux_master(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
/* user mask: 1=enabled. Compute nchan_enabled BEFORE inverting MASK */
{
	struct acq400_sc_dev* sc_dev = container_of(acq400_devices[0], struct acq400_sc_dev, adev);
	unsigned mux;

	if (sscanf(buf, "%x", &mux) == 1){
		acq400_visit_set_arg(sc_dev->distributor_set, set_mux_action, (void*)mux);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(dac_mux_master,
		S_IRUGO|S_IWUSR, show_dac_mux_master, store_dac_mux_master);




const struct attribute *ao428_attrs[] = {
	&dev_attr_dacreset_device.attr,
	&dev_attr_dac_headroom.attr,
	&dev_attr_dac_fifo_samples.attr,
	&dev_attr_dac_encoding.attr,
	&dev_attr_AO_01.attr,
	&dev_attr_AO_02.attr,
	&dev_attr_AO_03.attr,
	&dev_attr_AO_04.attr,
	&dev_attr_AO_05.attr,
	&dev_attr_AO_06.attr,
	&dev_attr_AO_07.attr,
	&dev_attr_AO_08.attr,
	&dev_attr_offset_01.attr,
	&dev_attr_offset_02.attr,
	&dev_attr_offset_03.attr,
	&dev_attr_offset_04.attr,
	&dev_attr_offset_05.attr,
	&dev_attr_offset_06.attr,
	&dev_attr_offset_07.attr,
	&dev_attr_offset_08.attr,
	&dev_attr_delay66.attr,
	NULL
};



const struct attribute *ao420_attrs[] = {
	&dev_attr_G3.attr, &dev_attr_D3.attr, &dev_attr_AO_03.attr, &dev_attr_dac_range_03.attr,
	&dev_attr_G4.attr, &dev_attr_D4.attr, &dev_attr_AO_04.attr, &dev_attr_dac_range_04.attr,

/* subset 2 channels only .. */
	&dev_attr_G1.attr, &dev_attr_D1.attr, &dev_attr_AO_01.attr, &dev_attr_dac_range_01.attr,
	&dev_attr_G2.attr, &dev_attr_D2.attr, &dev_attr_AO_02.attr, &dev_attr_dac_range_02.attr,

	&dev_attr_dac_range_REF.attr,
	&dev_attr_dacreset_device.attr,
	&dev_attr_dac_headroom.attr,
	&dev_attr_dac_fifo_samples.attr,
	&dev_attr_dac_encoding.attr,
	&dev_attr_delay66.attr,
	&dev_attr_read_latency.attr,
	&dev_attr_dac_dec.attr,
	NULL
};





static const struct attribute *ao424_attrs[] = {
	&dev_attr_AO_17.attr,
	&dev_attr_AO_18.attr,
	&dev_attr_AO_19.attr,
	&dev_attr_AO_20.attr,
	&dev_attr_AO_21.attr,
	&dev_attr_AO_22.attr,
	&dev_attr_AO_23.attr,
	&dev_attr_AO_24.attr,
	&dev_attr_AO_25.attr,
	&dev_attr_AO_26.attr,
	&dev_attr_AO_27.attr,
	&dev_attr_AO_28.attr,
	&dev_attr_AO_29.attr,
	&dev_attr_AO_30.attr,
	&dev_attr_AO_31.attr,
	&dev_attr_AO_32.attr,
	&dev_attr_AO_01.attr,
	&dev_attr_AO_02.attr,
	&dev_attr_AO_03.attr,
	&dev_attr_AO_04.attr,
	&dev_attr_AO_05.attr,
	&dev_attr_AO_06.attr,
	&dev_attr_AO_07.attr,
	&dev_attr_AO_08.attr,
	&dev_attr_AO_09.attr,
	&dev_attr_AO_10.attr,
	&dev_attr_AO_11.attr,
	&dev_attr_AO_12.attr,
	&dev_attr_AO_13.attr,
	&dev_attr_AO_14.attr,
	&dev_attr_AO_15.attr,
	&dev_attr_AO_16.attr,

	&dev_attr_dacreset.attr,
	&dev_attr_dacreset_device.attr,
	&dev_attr_dac_headroom.attr,
	&dev_attr_dac_fifo_samples.attr,
	&dev_attr_dac_encoding.attr,
	&dev_attr_twos_comp_encoding.attr,
	&dev_attr_bank_mask.attr,
	&dev_attr_odd_channels.attr,
	&dev_attr_delay66.attr,
	&dev_attr_snoopsel.attr,
	&dev_attr_read_latency.attr,
	&dev_attr_dac_dec.attr,

	/*
	&dev_attr_G1.attr, &dev_attr_D1.attr,
	&dev_attr_G2.attr, &dev_attr_D2.attr,
	&dev_attr_G3.attr, &dev_attr_D3.attr,
	&dev_attr_G4.attr, &dev_attr_D4.attr,
	... 32
	*/
	NULL
};

const struct attribute **get_ao424_attrs(void) {
	return ao424_attrs + (ao424_16? 16: 0);
}

const struct attribute *acq436_upper_half_attrs_master[] = {
	&dev_attr_dac_mux_master.attr,
	&dev_attr_dac_mux.attr,
	NULL
};

MAKE_BITS(diob_src,    DIOUSB_CTRL,      MAKE_BITS_FROM_MASK,	DIOUSB_CTRL_BUS_MASK);

const struct attribute *sysfs_diobiscuit_attrs[] = {
	&dev_attr_diob_src.attr,
	NULL
};

extern void dio432_set_direction(struct acq400_dev *adev, unsigned byte_is_output);
extern u32 dio432_get_direction(struct acq400_dev *adev);

static ssize_t show_byte_is_output(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	u32 byte_is_output = IS_DIO482_PG(adev)? dio432_get_direction(adev): xo_dev->dio432.byte_is_output;

	return sprintf(buf, "%d,%d,%d,%d\n",
			byte_is_output&DIO432_CPLD_CTRL_OUTPUT(0),
			byte_is_output&DIO432_CPLD_CTRL_OUTPUT(1),
			byte_is_output&DIO432_CPLD_CTRL_OUTPUT(2),
			byte_is_output&DIO432_CPLD_CTRL_OUTPUT(3)
	);
}

static ssize_t store_byte_is_output(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);

	if (xo_dev->dio432.mode != DIO432_DISABLE){
		return -EBUSY;
	}else{
		int bytes[4];

		if (sscanf(buf, "%d,%d,%d,%d", bytes+0, bytes+1, bytes+2, bytes+3) == 4){
			unsigned byte_is_output = 0;
			int ib = 0;
			for (ib = 0; ib <= 3; ++ib){
				if (bytes[ib]){
					byte_is_output |= DIO432_CPLD_CTRL_OUTPUT(ib);
				}
			}
			if (IS_DIO482_PG(adev)){
				dio432_set_direction(adev, byte_is_output);
			}else{
				xo_dev->dio432.byte_is_output = byte_is_output;
			}
			return count;
		}else{
			return -EINVAL;
		}
	}
}

DEVICE_ATTR(byte_is_output, S_IRUGO|S_IWUSR, show_byte_is_output, store_byte_is_output);



static ssize_t show_ext_clk_from_sync(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	u32 ctrl = acq400rd32(adev, DIO432_CTRL);
	return sprintf(buf, "%d\n",
			(ctrl&DIO432_CTRL_EXT_CLK_SYNC) != 0);

}

static ssize_t store_ext_clk_from_sync(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned ext_clk_from_sync = 0;

	if (sscanf(buf, "%u", &ext_clk_from_sync) == 1){
		u32 ctrl = acq400rd32(adev, DIO432_CTRL);
		if (ext_clk_from_sync){
			ctrl |= DIO432_CTRL_EXT_CLK_SYNC;
		}else{
			ctrl &= ~DIO432_CTRL_EXT_CLK_SYNC;
		}
		acq400wr32(adev, DIO432_CTRL, ctrl);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(ext_clk_from_sync, S_IRUGO|S_IWUSR, show_ext_clk_from_sync, store_ext_clk_from_sync);

static ssize_t show_DO32(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "0x%08x\n", xo_dev->dio432.DO32);
}

static ssize_t store_DO32(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned DO32 = 0;

	if (sscanf(buf, "0x%x", &DO32) == 1 || sscanf(buf, "%u", &DO32) == 1){
		xo_dev->dio432.DO32 = DO32;
		if (xo_dev->dio432.mode == DIO432_IMMEDIATE){
			wake_up_interruptible(&adev->w_waitq);
			yield();
		}else{
			/* it's clocking, just send it thru. for case clocked IN, immediate OUT */
			acq400wr32(adev, DIO432_FIFO, xo_dev->dio432.DO32);
			xo_dev->dio432.DI32 = acq400rd32(adev, DIO432_FIFO);
		}
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(DO32, S_IRUGO|S_IWUSR, show_DO32, store_DO32);

static ssize_t show_DI32(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "0x%08x\n", xo_dev->dio432.DI32);
}

static ssize_t store_DI32(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned DI32 = 0;

	if (sscanf(buf, "%u", &DI32) == 1){
		xo_dev->dio432.DI32 = DI32;
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(DI32, S_IRUGO|S_IWUSR, show_DI32, store_DI32);

static ssize_t show_mode(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	return sprintf(buf, "%d %s\n", xo_dev->dio432.mode,
			dio32mode2str(xo_dev->dio432.mode));
}

static ssize_t store_mode(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned mode = 0;

	if (sscanf(buf, "%u", &mode) == 1){
		dio432_set_mode(adev, mode, 0);
		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(mode, S_IRUGO|S_IWUSR, show_mode, store_mode);

MAKE_BITS(dio_en,       DIO432_CTRL, MAKE_BITS_FROM_MASK,	DIO432_CTRL_DIO_EN);
MAKE_BITS(dpg_abort,    DIO432_CTRL, MAKE_BITS_FROM_MASK,	DIO432_CTRL_AWG_ABORT);
MAKE_BITS(active_low, 	DIO432_CTRL, MAKE_BITS_FROM_MASK, DIO432_CTRL_INVERT);

const struct attribute *dio_attrs[] = {
	&dev_attr_active_low.attr,
	&dev_attr_dio_en.attr,
	&dev_attr_byte_is_output.attr,
	NULL
};


const struct attribute *dio432_attrs[] = {
	&dev_attr_dpg_abort.attr,
	&dev_attr_DI32.attr,
	&dev_attr_DO32.attr,
	&dev_attr_mode.attr,

	&dev_attr_ext_clk_from_sync.attr,
	NULL
};


MAKE_BITS(dio422_config, DIO422_OE_CONFIG, MAKE_BITS_FROM_MASK, TDC_CH_MASK_CH1);
MAKE_BITS(dio422_TxENn,  DIO422_OE_CONFIG, MAKE_BITS_FROM_MASK, DIO422_OE_CONFIG_Tx_ENn);
MAKE_BITS(dio422_TxEN,  DIO422_OE_CONFIG, MAKE_BITS_FROM_MASK, DIO422_OE_CONFIG_Tx_EN);
MAKE_BITS(dio422_snoop_tx, DIO432_DI_SNOOP, MAKE_BITS_FROM_MASK, 0xff<<16);
MAKE_BITS(dio422_snoop_rx, DIO432_DI_SNOOP, MAKE_BITS_FROM_MASK, 0xff<<0);

const struct attribute *dio422_attrs[] = {
	&dev_attr_dio422_config.attr,
	&dev_attr_dio422_TxENn.attr,
	&dev_attr_dio422_TxEN.attr,
	&dev_attr_dio422_snoop_tx.attr,
	&dev_attr_dio422_snoop_rx.attr,
	0
};


MAKE_BITS(pig_psu_en,     PIG_CTL,    		0, PIG_CTL_PSU_EN);
MAKE_BITS(pig_master, 	  PIG_CTL,		0, PIG_CTL_MASTER);
MAKE_BITS(pig_imu_rst,    PIG_CTL,              0, PIG_CTL_IMU_RST);
MAKE_BITS(dds_dac_clkdiv, PC_DDS_DAC_CLKDIV, 	0, 0x0000ffff);
MAKE_BITS(adc_clkdiv,	  PC_ADC_CLKDIV,	0, 0x0000ffff);
MAKE_BITS(dds_phase_inc,  PC_DDS_PHASE_INC,	0, 0xffffffff);


const struct attribute *pig_celf_attrs[] = {
	&dev_attr_pig_psu_en.attr,
	&dev_attr_pig_master.attr,
	&dev_attr_pig_imu_rst.attr,
	&dev_attr_dds_dac_clkdiv.attr,
	&dev_attr_adc_clkdiv.attr,
	&dev_attr_dds_phase_inc.attr,
	NULL
};


static ssize_t show_ACQ400T_out(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "0x%08x 0x%08x\n",
		acq400rd32(adev, ACQ400T_DOA), acq400rd32(adev, ACQ400T_DOB));
}

static ssize_t store_ACQ400T_out(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned doa, dob;

	if (sscanf(buf, "%x %x", &doa, &dob) == 2){
		unsigned mcr = acq400rd32(adev, MCR);
		int pollcat = 0;

		mcr = DAC_CTRL_MODULE_EN;
		acq400wr32(adev, MCR, mcr);
		acq400wr32(adev, ACQ400T_DOA, doa);
		acq400wr32(adev, ACQ400T_DOB, dob);
		acq400wr32(adev, MCR, mcr| (1<<ACQ400T_SCR_SEND_START_BIT));

		while ((mcr = acq400rd32(adev, MCR)) &&
			(!(mcr & (1<<ACQ400T_SCR_TEST_DATA_DONE_BIT)))){
			yield();

			if ((++pollcat&0xfff) == 0){
				dev_dbg(DEVP(adev), "store_ACQ400T_out %d 0x%08x", pollcat, mcr);
			}
		}
		dev_dbg(DEVP(adev), "store_ACQ400T_out %d 0x%08x DONE", pollcat, mcr);
		acq400wr32(adev, MCR, mcr = DAC_CTRL_MODULE_EN);

		return count;
	}else{
		return -1;
	}
}

static DEVICE_ATTR(ACQ400T_out, S_IRUGO|S_IWUSR, show_ACQ400T_out, store_ACQ400T_out);

static ssize_t show_ACQ400T_in(
	struct device * dev,
	struct device_attribute *attr,
	char * buf)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	return sprintf(buf, "0x%08x 0x%08x\n",
		acq400rd32(adev, ACQ400T_DIA), acq400rd32(adev, ACQ400T_DIB));
}


static DEVICE_ATTR(ACQ400T_in, S_IRUGO, show_ACQ400T_in, 0);


const struct attribute *acq400t_attrs[] = {
	&dev_attr_ACQ400T_out.attr,
	&dev_attr_ACQ400T_in.attr,
	NULL
};


