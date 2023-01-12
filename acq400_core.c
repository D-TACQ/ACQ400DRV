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
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"

int debcount;
module_param(debcount, int, 0644);
MODULE_PARM_DESC(debcount, "NZ if counter debounce ever .. happened");


int histo_poll_ms = 10;
module_param(histo_poll_ms, int, 0644);
MODULE_PARM_DESC(histo_poll_ms, "histogram poll rate msec");

int axi_dma_agg32 = 0;
module_param(axi_dma_agg32, int, 0644);
MODULE_PARM_DESC(histo_poll_ms, "transitional for AGG32 gaining AXI DMA");

int trap_dump_stack = 3;
module_param(trap_dump_stack, int, 0644);

int enable_adc_ctrl_trap = 0;
module_param(enable_adc_ctrl_trap, int, 0644);

int _enable_adc_ctrl_trap = 0;

int aggregator_enabled = 0;

int enable_write_trap = 0;
module_param(enable_write_trap, int, 0644);

int aggsta_skip_ok = 1;
module_param(aggsta_skip_ok, int, 0644);

#define ADC_ENAX  (ADC_CTRL_ADC_EN|ADC_CTRL_FIFO_EN)
#define ADC_RSTX  (ADC_CTRL_ADC_RST|ADC_CTRL_FIFO_RST)

#define AGG_TRAP 	0
#define ADC_CTRL_TRAP 	0
#define DAC_TRAP 	0
#define ACQ482_CMAP_TRAP 0


void acq400wr32(struct acq400_dev *adev, int offset, u32 value)
{
	int trap = 0;
#if AGG_TRAP

	int agg_trap = adev->of_prams.site==0 && offset==AGGREGATOR && (value&(AGG_SITES_MASK<<AGGREGATOR_MSHIFT))==0;
	int trap = adc_ctrl_trap||agg_trap;

	if (agg_trap){
		u32 agg = acq400rd32(adev, offset);
		if ((agg&(AGG_SITES_MASK<<AGGREGATOR_MSHIFT))==0){
			// it's OK to clear sites because they are already clear ..
			dev_info(DEVP(adev), "acq400wr32() avert agg trap was:%08x set:%08x already clear", agg, value);
			trap = 0;
		}
		dev_err(DEVP(adev), "acq400wr32()  trap clearing sites: was %08x set:%08x", agg, value);
	}
#endif
#if ADC_CTRL_TRAP
	int adc_ctrl_trap = _enable_adc_ctrl_trap && aggregator_enabled && offset==ADC_CTRL;


	if (adc_ctrl_trap){
		switch(adev->of_prams.site){
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			trap = ((value&ADC_ENAX)!=ADC_ENAX||(value&ADC_RSTX)!=0);
		break;
		}
	}
#endif
#if DAC_TRAP
#warning "DAC_TRAP ENABLED"
	if (enable_write_trap){
		switch(adev->of_prams.site){
		case 0:
			trap = offset == DATA_ENGINE_1; break;
		case 1:
			trap = offset == DAC_CTRL; break;
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		break;
		}
	}
#endif
#if ACQ482_CMAP_TRAP
	if (offset == ADC_CTRL && adev->of_prams.site>=1 && adev->of_prams.site<=6){
		u32 old = acq400rd32(adev, offset)&ADC_CTRL_482_CMAP;
		u32 new = value&ADC_CTRL_482_CMAP;
		trap = old && !new;
	}
#endif
	if (adev->RW32_debug || trap){
		dev_info(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
		if (trap && trap_dump_stack){
			dump_stack();
			trap_dump_stack--;
		}
	}else{
		dev_dbg(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}
	if (dev_rc_write(&adev->ctrl_reg_cache, offset, value)){
		iowrite32(value, adev->dev_virtaddr + offset);
	}
}

u32 _acq400rd32(struct acq400_dev *adev, int offset, int nocache)
{
	u32 rc;

	if (nocache || dev_rc_read(&adev->ctrl_reg_cache, offset, &rc)){
		rc = ioread32(adev->dev_virtaddr + offset);
	}
	if (adev->RW32_debug > 1){
		dev_info(DEVP(adev), "acq400rd32 %p [0x%02x] = %08x\n",
			adev->dev_virtaddr + offset, offset, rc);
	}else{
		dev_dbg(DEVP(adev), "acq400rd32 %p [0x%02x] = %08x\n",
			adev->dev_virtaddr + offset, offset, rc);
	}
	return rc;
}

u32 acq400rd32(struct acq400_dev *adev, int offset)
{
	return _acq400rd32(adev, offset, 0);
}

u32 acq400rd32_nocache(struct acq400_dev *adev, int offset)
{
	return _acq400rd32(adev, offset, 1);
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

void set_XOspadN(struct acq400_dev* adev, int n, u32 value)
{
	acq400wr32(adev, XO_SPADN(n), value);

}
u32 get_XOspadN(struct acq400_dev* adev, int n)
{
	return acq400rd32(adev, XO_SPADN(n));
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

	/* extreme sandtrap @@REMOVEME PLEASE! */
	if ((agg&(AGG_SITES_MASK<<AGGREGATOR_MSHIFT))==0){
		u32 agg2 = acq400rd32(adev, AGGREGATOR);
		u32 setmask = acq400_convert_aggregator_set_to_register_mask(adev);

		dev_warn(DEVP(adev), "acq2106_aggregator_reset()zero aggregator spotted first read:%08x second:%08x demand:%08x",
				agg, agg2, setmask);

		if ((agg2&(AGG_SITES_MASK<<AGGREGATOR_MSHIFT)) != setmask){
			dev_warn(DEVP(adev), "acq2106_aggregator_reset() emergency combover");
			agg2 |= setmask;
		}
		agg = agg2;
	}
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

void sc_data_engine_disable(unsigned dex)
{
	struct acq400_dev *adev = acq400_devices[0];
	u32 DEX = acq400rd32(adev, dex);
	acq400wr32(adev, dex, DEX &= ~(DE_ENABLE));
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
			if (!axi_dma_agg32 && IS_AXI64(adev)){
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
	aggregator_enabled = 1;
}
void acq2006_aggregator_disable(struct acq400_dev *adev)
{
	u32 agg = acq400rd32(adev, AGGREGATOR);
	acq400wr32(adev, AGGREGATOR, agg & ~AGG_ENABLE);
	aggregator_enabled = 0;
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

/* global :-( */

u64 acq400_trigger_ns;

int fifo_monitor(void* data)
{
	struct acq400_dev *devs[MAXDEVICES+1];
	struct acq400_dev *adev = (struct acq400_dev *)data;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct acq400_dev *m1;		/* first module in agg set aka master */
	int idev = 0;
	int cursor;
	int i1 = 0;		/* randomize start time */
	int conv_active_detected = 0;
	char message_from_active[2][132];
	int aggsta_skip_reported = 0;

	message_from_active[0][0] = '\0';
	message_from_active[1][0] = '\0';

	acq400_trigger_ns = 0;

	devs[idev++] = adev;
	adev->get_fifo_samples = aggregator_get_fifo_samples;
	for (cursor = 0; cursor < MAXDEVICES; ++cursor){
		struct acq400_dev* module = sc_dev->aggregator_set[cursor];
		if (module){
			devs[idev++] = module;
			module->get_fifo_samples = acq420_get_fifo_samples;
		}
	}
	histo_clear_all(devs, idev);
	m1 = devs[idev>1?1:0];

	dev_info(DEVP(adev), "fifo_monitor() 01 idev:%d adev->site_no %s", idev, adev->site_no);

	while(!kthread_should_stop()) {
		unsigned aggsta, m1_cr, m1_sr;
		m1_cr = acq400rd32(m1, ADC_CTRL);
		m1_sr = acq400rd32(m1, ADC_FIFO_STA);
		aggsta = acq400rd32(adev, AGGSTA);

		if (!m1->sod_mode && !aggsta_skip_reported && (aggsta&AGGSTA_FIFO_ANYSKIP) != 0 && (aggsta&AGGSTA_FIFO_EMPTY) == 0){
			sc_dev->adev.rt.status = -10;
			snprintf(sc_dev->status_message, MAX_RT_STATUS_MESSAGE, "%s loss of data detected: AGGSTA:%08x AXI wakeups:%d",
					__FUNCTION__, aggsta, sc_dev->adev.rt.axi64_wakeups);
			dev_warn(DEVP(adev), sc_dev->status_message);
			if (aggsta_skip_ok){
				;
			}else if (adev->task_active && !IS_ERR_OR_NULL(adev->w_task)){
				adev->rt.please_stop = 1;
			}else{
				dev_err(DEVP(adev), "%s unable to stop work adev: s:%d ta:%d", __FUNCTION__, adev->of_prams.site, adev->task_active);
			}
			aggsta_skip_reported = 1;
		}
		//if (acq420_convActive(m1)){
		if ((m1_sr&ADC_FIFO_STA_ACTIVE) != 0){
			if (enable_adc_ctrl_trap) _enable_adc_ctrl_trap = 1;
			if (acq400_trigger_ns == 0){
				acq400_trigger_ns = ktime_get_real_ns();
			}
			histo_add_all(devs, idev, i1);
			conv_active_detected = 1;
			snprintf(message_from_active[0], 132, "conv_active good after %d ms aggsta %08x adc_cr:%08x adc_fsta:%08x",
					(unsigned)(ktime_get_real_ns()-acq400_trigger_ns)/1000000, aggsta, m1_cr, m1_sr);
		}else{
			if (conv_active_detected == 0){
				;
			}else if (conv_active_detected++ == 1){
				snprintf(message_from_active[1], 132, "conv_active lost after %d ms aggsta %08x adc_cr:%08x adc_fsta:%08x",
							(unsigned)(ktime_get_real_ns()-acq400_trigger_ns)/1000000, aggsta, m1_cr, m1_sr);
			}else if (conv_active_detected > 3){
				dev_warn(DEVP(adev), message_from_active[0]);
				dev_warn(DEVP(adev), message_from_active[1]);
			}
		}
		msleep(histo_poll_ms);
	}
	if (enable_adc_ctrl_trap) _enable_adc_ctrl_trap = 0;
	acq400_trigger_ns = 0;

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

void acq400_soft_trigger(unsigned enable)
{
	struct acq400_dev *adev = acq400_sites[0];
	unsigned mcr = acq400rd32(adev, MCR);
	if (enable){
		mcr |= MCR_SOFT_TRIG;
	}else{
		mcr &= ~MCR_SOFT_TRIG;
	}
	acq400wr32(adev, MCR, mcr);
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
EXPORT_SYMBOL_GPL(acq400rd32);
EXPORT_SYMBOL_GPL(acq400wr32);
EXPORT_SYMBOL_GPL(acq400_soft_trigger);
