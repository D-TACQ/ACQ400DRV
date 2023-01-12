/*                                                                           *
 * ao424_drv.c                                                              *
 *                                                                           *
 *  Created on: 24 Sep 2014                                                  *
 *      Author: pgm                                                          *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                    *
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
#include "acq400_lists.h"

#include "dmaengine.h"
#include <linux/jiffies.h>

typedef unsigned short ushort;

int set_spans_timeout = 10;
module_param(set_spans_timeout, int, 0644);
MODULE_PARM_DESC(set_spans_timeout, "timeout on setting spans pollcount 10*.1 = 1s");

int allow_spans_in_odds_mode = 1;
module_param(allow_spans_in_odds_mode, int, 0644);


#define IS_UNIPOLAR(span) ((span) == 0 || (span) == 1)

#define UP_ZERO	0x0000
#define BP_ZERO 0x8000

short ao424_fixEncoding(struct acq400_dev *adev, int pchan, short value)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (IS_UNIPOLAR(xo_dev->ao424_device_settings.u.ch.ao424_spans[pchan])){
		return value;
	}else if (xo_dev->ao424_device_settings.encoded_twocmp){
		return value;
	}else{
		return value^0x8000;
	}
}

static void _acq400wr32(struct acq400_dev *adev, int offset, u32 value)
{
	if (adev->RW32_debug){
		dev_info(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}else{
		dev_dbg(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}

	if (dev_rc_write(&adev->ctrl_reg_cache, offset, value)){
		iowrite32(value, adev->dev_virtaddr + offset);
	}
}

static u32 _acq400rd32(struct acq400_dev *adev, int offset)
{
	u32 rc = ioread32(adev->dev_virtaddr + offset);
	if (adev->RW32_debug > 1){
		dev_info(DEVP(adev), "acq400rd32 %p [0x%02x] = %08x\n",
			adev->dev_virtaddr + offset, offset, rc);
	}else{
		dev_dbg(DEVP(adev), "acq400rd32 %p [0x%02x] = %08x\n",
			adev->dev_virtaddr + offset, offset, rc);
	}
	return rc;
}

#define acq400rd32 _acq400rd32
#define acq400wr32 _acq400wr32

void ao424_init_on_setspan(struct acq400_dev* adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct AO424 *ao424ch = &xo_dev->ao424_device_settings;
	int ic;

	for (ic = 0; ic < adev->nchan_enabled; ++ic){
		ao424ch->u.ch.ao424_initvals[ic] =
		IS_UNIPOLAR(ao424ch->u.ch.ao424_spans[ic])? UP_ZERO: BP_ZERO;
		dev_dbg(DEVP(adev), "ao424_init_on_setspan() span:%d %s %04x",
				ao424ch->u.ch.ao424_spans[ic],
				IS_UNIPOLAR(ao424ch->u.ch.ao424_spans[ic])? "UP" : "BP",
				ao424ch->u.ch.ao424_initvals[ic]);
	}
}

int _ao424_set_spans(struct acq400_dev* adev, unsigned ctrl)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned stat1;
	unsigned stat2;
	unsigned fifo_before;
	unsigned fifo_during;

	int pollcat = 0;
	int rc = 0;

	//u32 cgen = acq400rd32(adev, DAC_424_CGEN);

	ao424_init_on_setspan(adev);

	dev_dbg(DEVP(adev), "_ao424_set_spans 01");

	ctrl &= ~(DAC_CTRL_DAC_EN|DAC_CTRL_FIFO_EN);
	ctrl &= ~DAC_CTRL_LL;			/** PGM desperate */
	ctrl |= DAC_CTRL_MODULE_EN;
	acq400wr32(adev, DAC_CTRL, ctrl);


	acq400wr32(adev, DAC_INT_CSR, 0);
	//acq400wr32(adev, DAC_424_CGEN, 0);

	acq400wr32(adev, DAC_CTRL, ctrl |  ADC_CTRL_FIFO_RST|DAC_CTRL_DAC_RST);
	acq400wr32(adev, DAC_CTRL, ctrl |= ADC_CTRL_FIFO_EN);

	fifo_before = xo_dev->xo.getFifoSamples(adev);
	write32(adev->dev_virtaddr+AXI_FIFO,
			xo_dev->ao424_device_settings.u.lw, adev->nchan_enabled);
	fifo_during = xo_dev->xo.getFifoSamples(adev);

	dev_dbg(DEVP(adev), "_ao424_set_spans after write %d lw fifo count:%d",
			adev->nchan_enabled, fifo_during);

	acq400wr32(adev, DAC_CTRL, ctrl |= AO424_DAC_CTRL_SPAN);
	stat1 = acq400rd32(adev, DAC_FIFO_STA);

	while(((stat2 = acq400rd32(adev, DAC_FIFO_STA))&AO424_DAC_FIFO_STA_SWC) == 0){
		msleep(pollcat < 5? 1: 100);
		if (++pollcat > set_spans_timeout){
			// @todo .. seems bogus acq400wr32(adev, DAC_CTRL, ctrl|ADC_CTRL_FIFO_EN|AO424_DAC_CTRL_SPAN);
			dev_err(DEVP(adev), "_ao424_set_spans() SWC timeout at pollcat %d ctrl w:%08x r:%08x fsta:%08x",
					pollcat, ctrl, acq400rd32(adev, DAC_CTRL), stat2);
			rc = -1;
			break;
		}
	}

	acq400wr32(adev, DAC_FIFO_STA, acq400rd32(adev, DAC_FIFO_STA));
	acq400wr32(adev, DAC_CTRL, ctrl &= ~AO424_DAC_CTRL_SPAN);

	dev_info(DEVP(adev), "AO424_DAC_CTRL_SPAN set SWC stat1=%08x  stat2=%08x now: %08x pollcat:%d",
			stat1, stat2, acq400rd32(adev, DAC_FIFO_STA), pollcat);
	//acq400wr32(adev, DAC_424_CGEN, cgen);
	dev_info(DEVP(adev), "fifo samples before:%u during:%u after:%u",
			fifo_before, fifo_during, xo_dev->xo.getFifoSamples(adev));
	return rc;
}

int ao424_set_spans(struct acq400_dev* adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned ctrl = acq400rd32(adev, DAC_CTRL);
	int was_enabled = 0;
	int was_c16 = 0;
	int rc = 0;

	u32 cgen = acq400rd32(adev, DAC_424_CGEN);

	if (adev->task_active){
		dev_err(DEVP(adev), "ao424_set_spans not allowed when task_active");
		return -1;
	}
	if ((was_c16 = (cgen&DAC_424_CGEN_ODD_CHANS) != 0)){
		if (!allow_spans_in_odds_mode){
			dev_err(DEVP(adev), "ao424_set_spans not allowed in ODD chans mode");
			return -1;
		}else{
			ao424_set_odd_channels(adev, 0);
		}
	}
	if (xo_dev->ao424_device_settings.encoded_twocmp == 1){
		xo_dev->ao424_device_settings.encoded_twocmp = 0;
		ctrl &= ~DAC_CTRL_TWOCMP;
		acq400wr32(adev, DAC_CTRL, ctrl);
	}
	if ((ctrl&ADC_CTRL_ADC_EN) != 0){
		if (xo_dev->AO_playloop.length > 0){
			dev_err(DEVP(adev), "ao424_set_spans ADC_CTRL_ADC_EN AND playloop_length no change");
			return -1;
		}else{
			was_enabled = 1;
		}
	}

	rc = _ao424_set_spans(adev, ctrl);

	if (rc == 0){
		struct AO424 *ao424ch = &xo_dev->ao424_device_settings;
		int is_unipolar = 0;
		int ic;
		for (ic = 0; ic < adev->nchan_enabled; ++ic){
			if (IS_UNIPOLAR(ao424ch->u.ch.ao424_spans[ic])){
				++is_unipolar;
			}
		}
		if (is_unipolar && is_unipolar != adev->nchan_enabled){
			dev_warn(DEVP(adev),
				"MIXED unipolar bipolar system: all encoding has to be offset binary");
		}
		if (!is_unipolar){
			xo_dev->ao424_device_settings.encoded_twocmp = 1;
			ctrl |= DAC_CTRL_TWOCMP;
		}
		if (was_c16){
			ao424_set_odd_channels(adev, 1);
		}
		if (was_enabled){
			dev_dbg(DEVP(adev), "enable ADC_CTRL_ADC_EN");
		}
		acq400wr32(adev, DAC_CTRL, ctrl);
	}
	return rc;
}

/* default to +/-10V */
void ao424_setspan_defaults(struct acq400_dev* adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct AO424 *ao424ch = &xo_dev->ao424_device_settings;
	int ic;

	for (ic = 0; ic < AO424_MAXCHAN; ++ic){
		ao424ch->u.ch.ao424_spans[ic] = 3;
	}
}
