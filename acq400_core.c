/* ------------------------------------------------------------------------- */
/* acq400_core.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 29 Jul 2017  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2017 Peter Milne, D-TACQ Solutions Ltd         *
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

#include <uapi/linux/sched/types.h>
#include <linux/sched/rt.h>

#include "acq400.h"
#include "bolo.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"

int debcount;
module_param(debcount, int, 0644);
MODULE_PARM_DESC(debcount, "NZ if counter debounce ever .. happened");


int histo_poll_ms = 10;
module_param(histo_poll_ms, int, 0644);
MODULE_PARM_DESC(histo_poll_ms, "histogram poll rate msec");

void acq400wr32(struct acq400_dev *adev, int offset, u32 value)
{
	if (adev->RW32_debug){
		dev_info(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}else{
		dev_dbg(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}

	iowrite32(value, adev->dev_virtaddr + offset);
}



u32 acq400rd32(struct acq400_dev *adev, int offset)
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

void acq400setbits(struct acq400_dev *adev, int offset, u32 bits)
{
	u32 rv = acq400rd32(adev, offset);
	acq400wr32(adev, offset, rv | bits);
}
void acq400clrbits(struct acq400_dev *adev, int offset, u32 bits)
{
	u32 rv = acq400rd32(adev, offset);
	acq400wr32(adev, offset, rv & ~bits);
}

void write32(volatile u32* to, volatile u32* from, int nwords)
{
	int ii;

	for (ii = 0; ii < nwords; ++ii){
		iowrite32(from[ii], to);
	}
}

int xo400_write_fifo(struct acq400_dev* adev, int frombyte, int bytes)
{
	unsigned len = adev->cursor.hb[0]->len;
	unsigned ib = frombyte/len;
	unsigned offset = frombyte - ib*len;
	/* WARNING: assumes NEVER overlaps HB end .. */
	dev_dbg(DEVP(adev), "write32(%p = %p+%d, %d",
			adev->dev_virtaddr+AXI_FIFO,
			adev->cursor.hb[ib]->va,
			offset/sizeof(u32),
			bytes/sizeof(u32)
	);

	write32(adev->dev_virtaddr+AXI_FIFO,
		adev->cursor.hb[ib]->va+offset/sizeof(u32),
		bytes/sizeof(u32));
	return bytes;
}

u32 acq400rd32_upcount(struct acq400_dev *adev, int offset)
{
	u32 c1 = acq400rd32(adev, offset);
	u32 c2;

	while((c2 = acq400rd32(adev, offset)) < c1){
		c1 = c2;
		++debcount;
	}
	return c2;
}

void set_spadN(struct acq400_dev* adev, int n, u32 value)
{
	acq400wr32(adev, SPADN(n), value);

}
u32 get_spadN(struct acq400_dev* adev, int n)
{
	return acq400rd32(adev, SPADN(n));
}

static bool filter_true(struct dma_chan *chan, void *param)
{
	return true;
}


static int _get_dma_chan(struct acq400_dev *adev, int ic)
{
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);

	adev->dma_chan[ic] = dma_request_channel(mask, filter_true, NULL);
	if (adev->dma_chan[ic] == 0){
		dev_err(DEVP(adev), "%p id:%d dma_find_channel set zero",
					adev, adev->pdev->dev.id);
	}else{
		dev_dbg(DEVP(adev), "dma chan[%d] selected: %d",
				ic, adev->dma_chan[ic]->chan_id);
	}
	return adev->dma_chan[ic] == 0 ? -1: 0;
}

extern int xo_use_distributor;		/* REMOVE ME */

int get_dma_channels(struct acq400_dev *adev)
{
	if (IS_AXI64(adev)){
		return 0;
	}else if (IS_AO42X(adev) || IS_DIO432X(adev)){
		if (xo_use_distributor){
			int rc = _get_dma_chan(adev, 0) || _get_dma_chan(adev, 1);
			return rc;
		}else{
			return _get_dma_chan(adev, 0);
		}
	}else{
		int rc = _get_dma_chan(adev, 0) || _get_dma_chan(adev, 1);
		return rc;
	}
}

static void _release_dma_chan(struct acq400_dev *adev, int ic)
{
	if (adev->dma_chan[ic]){
		dma_release_channel(adev->dma_chan[ic]);
		adev->dma_chan[ic] = 0;
	}
}
void release_dma_channels(struct acq400_dev *adev)
{
	if (!IS_AXI64(adev)){
		_release_dma_chan(adev, 0);
		_release_dma_chan(adev, 1);
	}
}

void acq400_clear_histo(struct acq400_dev *adev)
{
	memset(adev->fifo_histo, 0, FIFO_HISTO_SZ*sizeof(u32));
}

void add_fifo_histo(struct acq400_dev *adev, u32 status)
{
	adev->fifo_histo[STATUS_TO_HISTO(status)]++;
}

void add_fifo_histo_ao42x(struct acq400_dev *adev, unsigned samples)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	(adev->fifo_histo[samples >> xo_dev->xo.hshift])++;
}

u32 aggregator_get_fifo_samples(struct acq400_dev *adev)
{
	return acq400rd32(adev, AGGSTA);
}


int check_fifo_statuses(struct acq400_dev *adev)
{
	if (IS_SC(adev)){
		int islave = 0;
		struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

		for (islave = 0; islave < MAXDEVICES; ++islave){
			struct acq400_dev* slave = sc_dev->aggregator_set[islave];
			if (!slave) break;

			if (slave->isFifoError(slave)){
				dev_err(DEVP(adev), "FIFO ERROR slave %d", SITE(*slave));
				slave->rt.refill_error = true;
				goto fail;
			} else if (IS_ACQ480(adev) && adev->rt.nget != 0 && acq480_train_fail(slave) == 1){
				dev_err(DEVP(adev), "LINK TRAINING ERROR slave %d", SITE(*slave));
				if (acq480_train_fail(slave) == 1){
					dev_err(DEVP(adev), "LINK TRAINING ERROR slave %d 2nd strike", SITE(*slave));
					slave->rt.refill_error = true;
					goto fail;
				}
			} else {
				continue;
			}
		}
	}else{
		return adev->isFifoError(adev);
	}
	return 0;
fail:
	adev->rt.refill_error = true;
	dev_err(DEVP(adev), "ERROR: quit on FIFERR set refill_error");
	wake_up_interruptible_all(&adev->refill_ready);
	return 1;
}


void acq2106_distributor_reset_enable(struct acq400_dev *adev)
{
	u32 dst = acq400rd32(adev, DISTRIBUTOR);
	acq400wr32(adev, DISTRIBUTOR, dst &= ~(AGG_FIFO_RESET|AGG_ENABLE));
	acq400wr32(adev, DISTRIBUTOR, dst | AGG_FIFO_RESET);
	acq400wr32(adev, DISTRIBUTOR, dst | AGG_ENABLE);
	acq400rd32(adev, DISTRIBUTOR);
	dev_dbg(DEVP(adev), "%s 99", __FUNCTION__);
}

void acq2106_aggregator_reset(struct acq400_dev *adev)
{
	u32 agg = acq400rd32(adev, AGGREGATOR);
	acq400wr32(adev, AGGREGATOR, agg &= ~(AGG_FIFO_RESET|AGG_ENABLE));
	acq400wr32(adev, AGGREGATOR, agg | AGG_FIFO_RESET);
	acq400wr32(adev, AGGREGATOR, agg);
	acq400rd32(adev, AGGREGATOR);
	dev_dbg(DEVP(adev), "%s 99", __FUNCTION__);
}


int acq400_set_AXI_DMA_len(struct acq400_dev *adev, int len)
{
	int blocks = len/AXI_DMA_BLOCK;

	dev_dbg(DEVP(adev), "%s len %d blocks %d", __FUNCTION__, len, blocks);
	if (blocks > AXI_DMA_ENGINE_DATA_MAX_BLOCKS){
		blocks = AXI_DMA_ENGINE_DATA_MAX_BLOCKS;
	}
	if (blocks < 1) blocks = 1;
	acq400wr32(adev, AXI_DMA_ENGINE_DATA, blocks-1);
	return blocks*AXI_DMA_BLOCK;
}
int acq400_get_AXI_DMA_len(struct acq400_dev *adev)
{
	u32 blocks = acq400rd32(adev, AXI_DMA_ENGINE_DATA) + 1;
	return blocks*AXI_DMA_BLOCK;
}
void sc_data_engine_reset_enable(unsigned dex)
{
	struct acq400_dev *adev = acq400_devices[0];
	u32 DEX = acq400rd32(adev, dex);
	acq400wr32(adev, dex, DEX &= ~(DE_ENABLE));
	acq400wr32(adev, dex, DEX | DE_ENABLE);
}
void acq2006_aggregator_enable(struct acq400_dev *adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	u32 agg = acq400rd32(adev, AGGREGATOR);
	agg &= ~AGG_SPAD_ALL_MASK;

	switch(sc_dev->spad.spad_en){
	default:
		goto agg99;
	case SP_EN: {
			int len = sc_dev->spad.len;
			if (IS_AXI64(adev)){
				len /= 2;    /* SPADS double spaced in 64 bit */
			}
			agg |= SET_AGG_SPAD_LEN(len);
			agg |= AGG_SPAD_EN;
		}
		break;
	case SP_FRAME:
		agg |= AGG_SPAD_EN|AGG_SPAD_FRAME;
		break;
	}
	switch(sc_dev->spad.diX){
	case SD_DI4:
		agg |= SET_AGG_SNAP_DIx(AGG_SNAP_DI_4); break;
	case SD_DI32:
		agg |= SET_AGG_SNAP_DIx(AGG_SNAP_DI_32); break;
	}
 agg99:
	acq400wr32(adev, AGGREGATOR, agg | AGG_ENABLE);
}
void acq2006_aggregator_disable(struct acq400_dev *adev)
{
	u32 agg = acq400rd32(adev, AGGREGATOR);
	acq400wr32(adev, AGGREGATOR, agg & ~AGG_ENABLE);
}

void acq400_enable_trg_if_master(struct acq400_dev *adev)
{
	dev_dbg(DEVP(adev), "acq400_enable_trg_if_master %d = %d",
			adev->of_prams.site, ((adev->mod_id&MOD_ID_IS_SLAVE) == 0));

	if ((adev->mod_id&MOD_ID_IS_SLAVE) == 0){
		acq400_enable_trg(adev, 1);
	}
	acq400_enable_adc(adev);
}

int acq400_enable_trg(struct acq400_dev *adev, int enable)
{
	u32 timcon = acq400rd32(adev, TIM_CTRL);
	int was_enabled = (timcon&TIM_CTRL_MODE_HW_TRG_EN) != 0;
	dev_dbg(DEVP(adev), "acq400_enable_trg() 0x%08x (bits=%02x) %s 01", timcon, TIM_CTRL_MODE_HW_TRG_EN, enable? "ON": "off");
	if (adev->sod_mode){
		if (was_enabled){
			dev_err(DEVP(adev), "sod_mode is enabled, but so is TRG");
		}
		return was_enabled;
	}
	if (enable){
		if (was_enabled){
			dev_dbg(DEVP(adev), "acq400_enable_trg already enabled ..");
		}
		timcon |= TIM_CTRL_MODE_HW_TRG_EN;
	}else{
		timcon &= ~TIM_CTRL_MODE_HW_TRG_EN;
	}
	dev_dbg(DEVP(adev), "acq400_enable_trg() 0x%08x (bits=%02x) %s 99", timcon, TIM_CTRL_MODE_HW_TRG_EN, enable? "ON": "off");
	acq400wr32(adev, TIM_CTRL, timcon);
	return was_enabled;
}

void histo_clear_all(struct acq400_dev *devs[], int maxdev)
{
	int cursor;
	for (cursor = 0; cursor < maxdev; ++cursor){
		acq400_clear_histo(devs[cursor]);
	}
}

void histo_add_all(struct acq400_dev *devs[], int maxdev, int i1)
{
	int cursor;
	for (cursor = i1; cursor < maxdev; ++cursor){
		struct acq400_dev *adev = devs[cursor];
		add_fifo_histo(adev, adev->get_fifo_samples(adev));
	}
	for (cursor = 0; cursor < i1; ++cursor){
		struct acq400_dev *adev = devs[cursor];
		add_fifo_histo(adev, adev->get_fifo_samples(adev));
	}
}

int fifo_monitor(void* data)
{
	struct acq400_dev *devs[MAXDEVICES+1];
	struct acq400_dev *adev = (struct acq400_dev *)data;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int idev = 0;
	int cursor;
	int i1 = 0;		/* randomize start time */

	devs[idev++] = adev;
	adev->get_fifo_samples = aggregator_get_fifo_samples;
	for (cursor = 0; cursor < MAXDEVICES; ++cursor){
		struct acq400_dev* slave = sc_dev->aggregator_set[cursor];
		if (slave){
			devs[idev++] = slave;
			slave->get_fifo_samples = acq420_get_fifo_samples;
		}
	}
	histo_clear_all(devs, idev);

	while(!kthread_should_stop()) {
		if (acq420_convActive(devs[idev>1?1:0])){
			histo_add_all(devs, idev, i1);
			msleep(histo_poll_ms);
		}
//		if (++i1 >= idev) i1 = 0;
	}

	return 0;
}

struct acq400_dev* acq400_lookupSite(int site)
{
	int is;
	for (is = 0; is < MAXDEVICES+1; ++is){
		struct acq400_dev* adev = acq400_devices[is];
		if (adev != 0 && adev->of_prams.site == site){
			return adev;
		}
	}
	return 0;
}

void acq400_enable_event0(struct acq400_dev *adev, int enable)
{
	u32 timcon = acq400rd32(adev, TIM_CTRL);
	if (enable){
		timcon |= TIM_CTRL_MODE_EV0_EN;
	}else{
		timcon &= ~TIM_CTRL_MODE_EV0_EN;
	}
	dev_dbg(DEVP(adev), "acq400_enable_event0(%d) 0x%08x", enable, timcon);
	acq400wr32(adev, TIM_CTRL, timcon);
}

void acq400_timer_init(
	struct hrtimer* timer,
	enum hrtimer_restart (*function)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer->function = function;
}

void go_rt(int prio)
{
	struct task_struct *task = current;
	struct sched_param param = { };

	param.sched_priority = prio;
	sched_setscheduler(task, SCHED_FIFO, &param);
}

void acq400_set_peripheral_SPI_CS(unsigned csword)
{
	struct acq400_dev* adev = acq400_devices[0];

	dev_dbg(DEVP(adev), "acq400_set_peripheral_SPI_CS() %08x\n", csword);
	acq400wr32(adev, SPI_PERIPHERAL_CS, csword);
}

EXPORT_SYMBOL_GPL(acq400_set_peripheral_SPI_CS);

