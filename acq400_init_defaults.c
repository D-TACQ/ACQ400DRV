/* ------------------------------------------------------------------------- */
/* acq400_init_defaults.c  D-TACQ ACQ400 FMC  DRIVER                                   
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

#include "acq400.h"
#include "bolo.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"


int acq424_adc_conv_time = 0x2d;
module_param(acq424_adc_conv_time, int, 0444);

int adc_18b = 0;
module_param(adc_18b, int, 0444);
MODULE_PARM_DESC(adc_18b, "set TRUE on load if 18 bit devices fitted [1]");

int data_32b = 0;
module_param(data_32b, int, 0444);
MODULE_PARM_DESC(data_32b, "set TRUE on load if 32 bit data required [0]");

int adc_conv_time = ADC_CONV_TIME_1000;
module_param(adc_conv_time, int, 0444);
MODULE_PARM_DESC(adc_conv_time, "hardware tweak, change at load only");

int hitide = HITIDE;
module_param(hitide, int, 0644);
MODULE_PARM_DESC(hitide, "hitide value (words)");

int lotide = HITIDE-4;
module_param(lotide, int, 0644);
MODULE_PARM_DESC(lotide, "lotide value (words)");

int measure_ao_fifo_ok = 0;
module_param(measure_ao_fifo_ok, int, 0644);
MODULE_PARM_DESC(measure_ao_fifo_ok, "stubs ao fifo measure, cause of blowout on ao428");

int act_on_fiferr = 1;
module_param(act_on_fiferr, int, 0644);
MODULE_PARM_DESC(act_on_fiferr, "0: log, don't act. 1: abort on error");

int ao420_mapping[AO_CHAN] = { 4, 3, 2, 1 };
int ao420_mapping_count = 4;
module_param_array(ao420_mapping, int, &ao420_mapping_count, 0644);

int no_ao42x_llc_ever = 0;
module_param(no_ao42x_llc_ever, int, 0644);
MODULE_PARM_DESC(no_ao42x_llc_ever, "refuse to set LLC mode on exit AWG. Won't work anyway if distributor set");

/* KLUDGE ALERT .. remove me now? */
int dio432_rowback = 256/4;
module_param(dio432_rowback, int, 0644);
MODULE_PARM_DESC(dio432_rowback, "stop short filling FIFO by this much");


void acq420_onStart(struct acq400_dev *adev);
void acq480_onStart(struct acq400_dev *adev);
void acq43X_onStart(struct acq400_dev *adev);
void ao420_onStart(struct acq400_dev *adev);
static void acq420_disable_fifo(struct acq400_dev *adev);

static void _ao420_onStart(struct acq400_dev *adev);
static void _ao420_onStop(struct acq400_dev *adev);
static void _dio432_DO_onStart(struct acq400_dev *adev);

int ao428_physChan(int lchan /* 1..8 */ )
{
	return lchan-1;
}
int ao424_physChan(int lchan /* 1..32 */ )
{
	return lchan-1;
}

u32 acq420_set_fmt(struct acq400_dev *adev, u32 adc_ctrl)
/* DOES NOT ACTUALLY WRITE HARDWARE! */
{
	if (adev->adc_18b){
		adc_ctrl |= ADC_CTRL_420_18B;
	}else{
		adc_ctrl &= ~ADC_CTRL_420_18B;
	}
	if (adev->data32){
		adc_ctrl |= ADC_CTRL32B_data;
	}else{
		adc_ctrl &= ~ADC_CTRL32B_data;
	}
	return adc_ctrl;
}

void acq400_getID(struct acq400_dev *adev)
{
	u32 modid;

	dev_info(DEVP(adev), "About to read MODID from %p\n", adev->dev_virtaddr+MOD_ID);

	modid = acq400rd32(adev, MOD_ID);
	adev->mod_id = modid;

	dev_info(DEVP(adev), "Device MODID %08x", modid);
}

int acq420_convActive(struct acq400_dev *adev)
{
	u32 fifsta = acq400rd32(adev, ADC_FIFO_STA);
	u32 active = fifsta&ADC_FIFO_STA_ACTIVE;

	dev_dbg(DEVP(adev), "acq420_convActive() %08x %s",
			fifsta, active? "YES": "no");
	return active;
}
int acq480_train_fail(struct acq400_dev *adev)
{
	if ((adev->mod_id&MOD_ID_REV_MASK) >= 0xa){
		return (acq400rd32(adev, ADC_CTRL)&ADC480_CTRL_TRAIN_OK) == 0;
	}else{
		return -1;
	}
}


void ao420_clear_fifo_flags(struct acq400_dev *adev)
{
	acq400wr32(adev, ADC_FIFO_STA, ADC_FIFO_STA_EMPTY|ADC_FIFO_STA_ERR);
}
void ao420_reset_fifo(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, DAC_CTRL);
	acq400wr32(adev, DAC_CTRL, ctrl &= ~ADC_CTRL_FIFO_EN);
	acq400wr32(adev, DAC_CTRL, ctrl | ADC_CTRL_FIFO_RST);
	ao420_clear_fifo_flags(adev);
	acq400wr32(adev, DAC_CTRL, ctrl |= ADC_CTRL_FIFO_EN);
}

static void acq420_reset_fifo(struct acq400_dev *adev)
/* Raise and Release reset */
{
	u32 ctrl = acq400rd32(adev, ADC_CTRL);

	acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_FIFO_RST);
	acq400wr32(adev, ADC_CTRL, ctrl);
	/** clear FIFO flags .. workaround hw bug */
	acq400wr32(adev, ADC_FIFO_STA, ADC_FIFO_STA_ERR);

}

static void acq420_enable_fifo(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, ADC_CTRL);
	if (adev->ramp_en){
		ctrl |= ADC_CTRL_RAMP_EN;
	}else{
		ctrl &= ~ADC_CTRL_RAMP_EN;
	}
	if (IS_ACQ42X(adev)){
		ctrl = acq420_set_fmt(adev, ctrl);
	}
	acq400wr32(adev, ADC_CTRL, ctrl|ADC_CTRL_ENABLE_FIFO);
}


void _ao420_stop(struct acq400_dev* adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned cr = acq400rd32(adev, DAC_CTRL);
	x400_disable_interrupt(adev);

	dev_dbg(DEVP(adev), "_ao420_stop() reset FIFO and clear histo\n");

	cr &= ~DAC_CTRL_DAC_EN;
	if (IS_AO42X(adev)){
		dev_dbg(DEVP(adev), "_ao420_stop() AO_playloop.length %d\n", xo_dev->AO_playloop.length);
		if (xo_dev->AO_playloop.length == 0){
			if (!no_ao42x_llc_ever){
				cr |= DAC_CTRL_LL;
			}else{
				dev_dbg(DEVP(adev), "_ao420_stop() STUB setting LL\n");
			}
		}
		if (adev->data32){
			cr |= ADC_CTRL32B_data;
		}else{
			cr &= ~ADC_CTRL32B_data;
		}
	}
	//adev->AO_playloop.length = 0;
	xo_dev->AO_playloop.cursor = 0;
	acq400wr32(adev, DAC_CTRL, cr);
	release_dma_channels(adev);

	ao420_reset_fifo(adev);
	acq400_clear_histo(adev);
}


static void _ao420_onStop(struct acq400_dev *adev)
{
	_ao420_stop(adev);
}


extern int xo_use_distributor;

void _dio432_DO_onStart(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (xo_use_distributor){
		acq400wr32(adev, DIO432_DO_LOTIDE, adev->lotide);
		dev_dbg(DEVP(adev), "_dio432_DO_onStart() 05");
	}else if (xo_dev->AO_playloop.oneshot == 0 ||
		xo_dev->AO_playloop.length > adev->lotide){
		dev_dbg(DEVP(adev), "_dio432_DO_onStart() 10");
		acq400wr32(adev, DIO432_DO_LOTIDE, adev->lotide);
		x400_enable_interrupt(adev);
	}else{
		acq400wr32(adev, DIO432_DO_LOTIDE, 0);
		dev_dbg(DEVP(adev), "_dio432_DO_onStart() 20");
	}
	acq400wr32(adev, DIO432_DIO_CTRL, acq400rd32(adev, DIO432_DIO_CTRL)|DIO432_CTRL_DIO_EN);
	dev_dbg(DEVP(adev), "_dio432_DO_onStart() 99");
}

void dio432_onStop(struct acq400_dev *adev)
{
	x400_disable_interrupt(adev);
	dio432_disable(adev);
	//dio432_init_clocked(adev);
}

void acq420_onStart(struct acq400_dev *adev)
{
	u32 ctrl;
	u32 status = acq400rd32(adev, ADC_FIFO_STA);

	dev_dbg(DEVP(adev), "acq420_onStart()");

	if ((status&ADC_FIFO_STA_ACTIVE) != 0){
		dev_err(DEVP(adev), "ERROR: ADC_FIFO_STA_ACTIVE set %08x", status);
		acq420_disable_fifo(adev);
	}

	acq400wr32(adev, ADC_HITIDE, 	adev->hitide);
	acq420_enable_fifo(adev);
	ctrl = acq400rd32(adev, ADC_CTRL);
	acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_ADC_RST);
	acq420_reset_fifo(adev);
	acq400wr32(adev, ADC_CTRL, ctrl);
	adev->fifo_isr_done = 0;
	//acq420_enable_interrupt(adev);
}

void acq480_onStart(struct acq400_dev *adev)
{
	dev_dbg(DEVP(adev), "acq480_onStart()");
	acq400wr32(adev, ADC_HITIDE, 	adev->hitide);
	acq420_enable_fifo(adev);

	acq420_reset_fifo(adev);

	adev->fifo_isr_done = 0;
	//acq420_enable_interrupt(adev);
}


void acq420_disable_fifo(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, ADC_CTRL);
	dev_dbg(DEVP(adev), "acq420_disable_fifo() %08x -> %08x",
			ctrl, ctrl & ~ADC_CTRL_ENABLE_CAPTURE);
	acq400wr32(adev, ADC_CTRL, ctrl & ~ADC_CTRL_ENABLE_CAPTURE);
	acq420_reset_fifo(adev);
}


static void acq420_init_defaults(struct acq400_dev *adev)
{
	u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);

	dev_info(DEVP(adev), "ACQ420 device init");
	acq400wr32(adev, ADC_CONV_TIME, IS_ACQ424(adev)? acq424_adc_conv_time: adc_conv_time);
	adev->data32 = data_32b;
	adev->adc_18b = adc_18b;
	adc_ctrl |= acq420_set_fmt(adev, adc_ctrl);
	acq400wr32(adev, ADC_CTRL, ADC_CTRL_MODULE_EN|adc_ctrl);
	adev->nchan_enabled = IS_ACQ424(adev)? 32:
			      IS_ACQ423(adev)? 32:
			      IS_ACQ425(adev)? 16:
			      IS_ACQ427(adev)? 8: 4;
	adev->word_size = adev->data32? 4: 2;
	adev->hitide = hitide;
	adev->lotide = lotide;
	adev->onStart = acq420_onStart;
	adev->onStop = acq420_disable_fifo;

	if (IS_ACQ424(adev)){
		acq400wr32(adev, ADC_CLKDIV, 66);
	}
	if (IS_ACQ423(adev)){
		acq400wr32(adev, ADC_CLKDIV, 500);
	}
}

static void acq480_init_defaults(struct acq400_dev *adev)
{
	dev_info(DEVP(adev), "ACQ480 device init: skeleton");

	adev->data32 = 0;
	adev->adc_18b = 0;

	acq400wr32(adev, ADC_CTRL, ADC_CTRL_MODULE_EN);
	adev->nchan_enabled = 8;
	adev->word_size = 2;
	adev->hitide = hitide;
	adev->lotide = lotide;
	adev->onStart = acq480_onStart;
	adev->onStop = acq420_disable_fifo;
	adev->clkdiv_mask = 0x0001;		/* disable clkdiv */
}


static u32 _v2f_init(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, V2F_CTRL);

	if (adev->data32){
		ctrl &= ~V2F_CTRL_DATA_PACKED;
	}else{
		ctrl |= V2F_CTRL_DATA_PACKED;
	}
	acq400wr32(adev, V2F_CTRL, ctrl&=~ V2F_CTRL_EN);
	acq400wr32(adev, V2F_CTRL, ctrl|V2F_CTRL_RST);
	acq400wr32(adev, V2F_CTRL, ctrl);
	return ctrl;
}
static void _v2f_onStop(struct acq400_dev *adev)
{
	_v2f_init(adev);
}

static void _v2f_onStart(struct acq400_dev *adev)
{
	u32 ctrl = _v2f_init(adev);
	acq400wr32(adev, V2F_CTRL, ctrl|V2F_CTRL_EN);
}

static void v2f_init_defaults(struct acq400_dev *adev)
{
	adev->data32 = 0;
	adev->word_size = 4;
	adev->nchan_enabled = 0;
	acq400wr32(adev, V2F_CHAN_SEL, 0x04030201);
	acq400wr32(adev, V2F_CTRL, ADC_CTRL_MODULE_EN|V2F_CTRL_EN);
	adev->onStart = _v2f_onStart;
	adev->onStop = _v2f_onStop;
}

static void _qen_onStop(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, QEN_CTRL);
	ctrl &= ~(QEN_CTRL_FIFO_EN|QEN_CTRL_EN);
	acq400wr32(adev, QEN_CTRL, ctrl);
}

static void _qen_onStart(struct acq400_dev *adev)
{
	acq420_enable_fifo(adev);
	acq420_reset_fifo(adev);
}

static void qen_init_defaults(struct acq400_dev *adev)
{
	struct acq400_dev *adev1 = acq400_devices[1];
	if (adev1 && adev1->data32 == 0){
		adev->data32 = 0;
		adev->word_size = 2;
		adev->nchan_enabled = 2;
	}else{
		adev->data32 = 1;
		adev->word_size = 1;
		adev->nchan_enabled = 1;
	}

	acq400wr32(adev, QEN_CTRL, QEN_CTRL_MODULE_EN);
	adev->onStart = _qen_onStart;
	adev->onStop = _qen_onStop;
}

static void acq1014_init_defaults(struct acq400_dev *adev)
{
	acq400wr32(adev, DIO1014_CR,
		DIO1014_CR_CLK_LO|DIO1014_CR_TRG_SOFT|DIO1014_MOD_EN);
}
static void pig_celf_init_defaults(struct acq400_dev *adev)
{
	adev->data32 = 0;
	/* data is 4 x 32 bits, but I+Q surely 16 bit each? */
	adev->word_size = 2;
	adev->nchan_enabled = 8;
	adev->clkdiv_mask = 0;	/* unfortunate alias of clkdiv reg. stub it */
	dev_info(DEVP(adev), "pig_celf_init_defaults()");
	acq400wr32(adev, MCR, MCR_MOD_EN);
	adev->onStart = acq420_onStart;
	adev->onStop = acq420_disable_fifo;
	acq400wr32(adev, PC_DDS_DAC_CLKDIV, 	0x5);
	acq400wr32(adev, PC_ADC_CLKDIV, 	0x14);
	acq400wr32(adev, PC_DDS_PHASE_INC, 	0x80000);
}

static void rad_celf_init_defaults(struct acq400_dev *adev)
{
	dev_info(DEVP(adev), "rad_celf_init_defaults()");
	acq400wr32(adev, MCR, MCR_MOD_EN);
}

static void pmodadc1_init_defaults(struct acq400_dev *adev)
{
	u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);

	dev_info(DEVP(adev), "PMODADC1 device init");
	if (FPGA_REV(adev) < 5){
		dev_warn(DEVP(adev), "OUTDATED FPGA PERSONALITY, please update");
	}
	acq400wr32(adev, ADC_CONV_TIME, adc_conv_time);
	adev->data32 = 0;
	adev->adc_18b = 0;
	adc_ctrl |= PMODADC1_CTRL_DIV|PMODADC1_CTRL_EXT_CLK_FROM_SYNC;
	acq400wr32(adev, ADC_CTRL, ADC_CTRL_MODULE_EN|adc_ctrl);
	/** nchan_enabled @@todo fudge for use in D32 (acq435 systems) */
	adev->nchan_enabled = 1;
	adev->word_size = 4;
	adev->hitide = hitide;
	adev->lotide = lotide;
	adev->onStart = acq420_onStart;
	adev->onStop = acq420_disable_fifo;
}

int xtd_watchdog(void *data)
{
	struct acq400_dev *adev = (struct acq400_dev *)data;
	int nzcount;
	u32 trg_src0 = 0;
	u32 trg_src;

	while(1) {
		msleep(10);
		trg_src = acq400rd32(adev, ATD_TRIGGERED);

		if (trg_src == 0){
			nzcount = 0;
			trg_src0 = 0;
		}else if (trg_src0 != 0 && (trg_src&trg_src0) == trg_src0){
			if (++nzcount >= 3){
				dev_info(DEVP(adev), "xtd_watchdog clears %08x", trg_src);
				acq400_clearDelTrgEvent(adev);
				acq400wr32(adev, ATD_TRIGGERED, trg_src);
				trg_src0 = 0;
			}
		}else{
			trg_src0 = trg_src;
		}
	}
}


static void start_xtd_watchdog(struct acq400_dev *adev)
{
	kthread_run(xtd_watchdog, adev, "%s.xtd", adev->dev_name);
}
static void acq43X_init_defaults(struct acq400_dev *adev)
{
	u32 adc_ctrl = acq400rd32(adev, ADC_CTRL);
	u32 banksel = 0;
	const char* devname;



	if (IS_ACQ430(adev)){
		adev->nchan_enabled = 8;
		banksel = ACQ430_BANKSEL;
		devname = "ACQ430";
	}else if (IS_ACQ437(adev)){
		adev->nchan_enabled = 16;
		banksel = ACQ437_BANKSEL;
		devname = "ACQ437";
	}else if (IS_ACQ436(adev)){
		adev->nchan_enabled = 24;
		banksel = ACQ436_BANKSEL;
		devname = "ACQ436";
	}else{
		adev->nchan_enabled = 32;	// 32 are you sure?.
		devname = "ACQ435";
	}
	dev_info(DEVP(adev), "%s device init", devname);

	acq400wr32(adev, ACQ435_MODE, banksel);
	adev->data32 = 1;
	adev->word_size = 4;
	adev->hitide = 128;
	adev->lotide = adev->hitide - 4;
	acq400wr32(adev, ADC_CLKDIV, 16);
	acq400wr32(adev, ADC_CTRL, adc_ctrl|ADC_CTRL_MODULE_EN);
	adev->onStart = acq43X_onStart;
	adev->onStop = acq420_disable_fifo;
	if (HAS_XTD(adev)){
		start_xtd_watchdog(adev);
	}
}

int _ao420_getFifoSamples(struct acq400_dev* adev) {
	return acq400rd32(adev, DAC_FIFO_SAMPLES)&DAC_FIFO_SAMPLES_MASK;
}

int _dio432_DO_getFifoSamples(struct acq400_dev* adev) {
	return acq400rd32(adev, DIO432_DO_FIFO_COUNT)&DAC_FIFO_SAMPLES_MASK;
}


static void ao428_init_defaults(struct acq400_dev *adev)
/* dac_celf mounts DAC20, assy known as "ao428elf" */
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);
	dev_info(DEVP(adev), "AO420 device init");

	adev->data32 = 1;
	adev->nchan_enabled = 8;
	adev->word_size = 4;
	adev->cursor.hb = &adev->hb[0];

	adev->sysclkhz = SYSCLK_M66;
	adev->onStart = _ao420_onStart;
	adev->onStop = _ao420_onStop;
	xo_dev->xo.physchan = ao428_physChan;
	xo_dev->xo.getFifoSamples = _ao420_getFifoSamples;
	xo_dev->xo.fsr = DAC_FIFO_STA;
	adev->lotide = 1024;

	dac_ctrl |= ADC_CTRL_MODULE_EN;
	acq400wr32(adev, DAC_CTRL, dac_ctrl);
	if (measure_ao_fifo_ok){
		measure_ao_fifo(adev);
	}
}
static void ao420_init_defaults(struct acq400_dev *adev, int data32)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);

	adev->data32 = data32;
	adev->nchan_enabled = IS_AO420_HALF436(adev)? 2: 4;
	adev->word_size = data32? 4: 2;
	adev->cursor.hb = &adev->hb[0];
	adev->sysclkhz = SYSCLK_M66;
	adev->onStart = _ao420_onStart;
	adev->onStop = _ao420_onStop;
	xo_dev->xo.physchan = ao420_physChan;
	xo_dev->xo.getFifoSamples = _ao420_getFifoSamples;
	xo_dev->xo.fsr = DAC_FIFO_STA;
	adev->lotide = 16384;

	dev_info(DEVP(adev), "AO420 device init NCHAN %d", adev->nchan_enabled);
	{
		u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);
		dac_ctrl |= ADC_CTRL_MODULE_EN;
		acq400wr32(adev, DAC_CTRL, dac_ctrl);
	}
	measure_ao_fifo(adev);
}

static void ao424_init_defaults(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);

	/* default to +/-10V bipolar. That's what 99% of the people want .. */
	dev_info(DEVP(adev), "AO424 device init default SPAN=3");
	ao424_setspan_defaults(adev);
	adev->data32 = 0;
	adev->nchan_enabled = 32;
	adev->word_size = 2;
	adev->cursor.hb = &adev->hb[0];

	adev->sysclkhz = SYSCLK_M66;
	adev->onStart = _ao420_onStart;
	adev->onStop = _ao420_onStop;
	xo_dev->xo.physchan = ao424_physChan;
	xo_dev->xo.getFifoSamples = _ao420_getFifoSamples;
	xo_dev->xo.fsr = DAC_FIFO_STA;
	adev->lotide = 1024;

	dac_ctrl |= ADC_CTRL_MODULE_EN;
	xo_dev->ao424_device_settings.encoded_twocmp = 0;
	acq400wr32(adev, DAC_CTRL, dac_ctrl);
	ao424_set_spans(adev);
	if (measure_ao_fifo_ok){
	    measure_ao_fifo(adev);
	}
}


static void _ao420_onStart(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	u32 ctrl = acq400rd32(adev, DAC_CTRL);

	ctrl &= ~DAC_CTRL_LL;
	if (xo_dev->AO_playloop.oneshot == 0 ||
			xo_dev->AO_playloop.length > adev->lotide){
		dev_dbg(DEVP(adev), "_ao420_onStart() set lotide:%d", adev->lotide);
		acq400wr32(adev, DAC_LOTIDE, adev->lotide);
		x400_enable_interrupt(adev);
	}else{
		acq400wr32(adev, DAC_LOTIDE, 0);
	}
	acq400wr32(adev, DAC_CTRL, ctrl |= DAC_CTRL_DAC_EN);
}

void _ao420_stop(struct acq400_dev* adev);


void acq400_enable_adc(struct acq400_dev *adev){
	u32 ctrl = acq400rd32(adev, ADC_CTRL);
	acq400wr32(adev, ADC_CTRL, ctrl  |= ADC_CTRL_ADC_EN);
}

extern int fiferr;		/* FIXME */
extern int FIFERR;


int acq420_isFifoError(struct acq400_dev *adev)
{
	u32 fifsta = acq400rd32(adev, ADC_FIFO_STA);
	int err = (fifsta&fiferr) != 0;

	acq400wr32(adev, ADC_FIFO_STA, fifsta);

	if (err){
		u32 fifsta2 = acq400rd32(adev, ADC_FIFO_STA);
		adev->stats.fifo_errors++;
		dev_warn(DEVP(adev), "FIFERR after %d buffers, mask:%08x cmp:%08x actual:%08x %s\n",
				acq400_lookupSite(0)->rt.nput,
				FIFERR, fiferr, fifsta,
				(fifsta2&fiferr) != 0? "NOT CLEARED": "clear");
	}

	return act_on_fiferr? err: 0;
}

int bolo8_isFifoError(struct acq400_dev *adev)
{
	u32 fifsta = acq400rd32(adev, B8_ADC_FIFO_STA);
	int err = (fifsta&fiferr) != 0;

	acq400wr32(adev, B8_ADC_FIFO_STA, fifsta);

	if (err){
		u32 fifsta2 = acq400rd32(adev, B8_ADC_FIFO_STA);
		adev->stats.fifo_errors++;
		dev_warn(DEVP(adev), "BOLO8 FIFERR after %d buffers, mask:%08x cmp:%08x actual:%08x %s\n",
				acq400_lookupSite(0)->rt.nput,
				FIFERR, fiferr, fifsta,
				(fifsta2&fiferr) != 0? "NOT CLEARED": "clear");
	}

	return act_on_fiferr? err: 0;
}


int dio432_isFifoError(struct acq400_dev *adev)
{
	u32 fifsta = acq400rd32(adev, ADC_FIFO_STA);
	int err = (fifsta&fiferr) != 0;
	int rc = 0;

	acq400wr32(adev, ADC_FIFO_STA, fifsta);

	if (err){
		u32 fifsta2 = acq400rd32(adev, ADC_FIFO_STA);
		int nbuf = acq400_lookupSite(0)->rt.nput;
		adev->stats.fifo_errors++;
		dev_warn(DEVP(adev), "FIFERR after %d buffers, mask:%08x cmp:%08x actual:%08x %s\n",
				nbuf, 				FIFERR, fiferr, fifsta,
				(fifsta2&fiferr) != 0? "NOT CLEARED": "clear");

		rc = nbuf>1 && act_on_fiferr? err: 0;
		if (err && !act_on_fiferr && rc == 0){
			dev_info(DEVP(adev), "initial error ignored");
		}
	}
	return rc;
}

void acq43X_onStart(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, ADC_CTRL);

	dev_dbg(DEVP(adev), "acq435_onStart() ctrl=%08x", ctrl);

	acq400wr32(adev, ADC_HITIDE, 	adev->hitide);
	ctrl &= ~ADC_CTRL_FIFO_EN|ADC_CTRL_ADC_EN;
	acq400wr32(adev, ADC_CTRL, ctrl |= ADC_CTRL_MODULE_EN);

	if (HAS_DTD(adev)){
		acq400_clearDelTrgEvent(adev);
	}
	if (adev->ramp_en){
		ctrl |= ADC_CTRL_RAMP_EN;
	}else{
		ctrl &= ~ADC_CTRL_RAMP_EN;
	}

	// set mode (assume done)
	// set clkdiv (assume done)
	// set timing bus (assume done)
	/** clear FIFO flags .. workaround hw bug */
	acq400wr32(adev, ADC_FIFO_STA, ADC_FIFO_STA_ERR);

	acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_FIFO_RST);
	acq400wr32(adev, ADC_CTRL, ctrl);
	if (acq420_get_fifo_samples(adev)){
		dev_warn(DEVP(adev), "ERROR: reset fifo but it's not empty!");
	}

	adev->fifo_isr_done = 0;
	//acq420_enable_interrupt(adev);

	acq400wr32(adev, ADC_CTRL, ctrl  |= ADC_CTRL_FIFO_EN);

	/* next: valid Master, Standalone only. @@todo slave? */
	acq400wr32(adev, ADC_CTRL, ctrl | ADC_CTRL_ADC_RST);
	acq400wr32(adev, ADC_CTRL, ctrl);

	dev_dbg(DEVP(adev), "acq435_onStart() 99");
}


static void dio432_init_defaults(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);

	dev_info(DEVP(adev), "dio432_init_defaults() 01");
	adev->data32 = 1;
	adev->nchan_enabled = 1;
	adev->word_size = 4;
	adev->cursor.hb = &adev->hb[0];
	adev->hitide = 2048;
	adev->lotide = 0x1998;
	adev->onStart = _dio432_DO_onStart;
	adev->onStop = dio432_onStop;
	xo_dev->xo.getFifoSamples = _dio432_DO_getFifoSamples;
	xo_dev->xo.fsr = DIO432_DO_FIFO_STATUS;
	adev->isFifoError = dio432_isFifoError;
	if ((IS_DIO432FMC(adev)||IS_DIO432PMOD(adev)) && FPGA_REV(adev) < 5){
		dev_warn(DEVP(adev), "OUTDATED FPGA PERSONALITY, please update");
	}
	//set_debugs("on");
	dac_ctrl |= IS_DIO432PMOD(adev)?
		DIO432_CTRL_SHIFT_DIV_PMOD: DIO432_CTRL_SHIFT_DIV_FMC;
	dac_ctrl |= DIO432_CTRL_MODULE_EN | DIO432_CTRL_DIO_EN;
	acq400wr32(adev, DAC_CTRL, dac_ctrl);

	dev_info(DEVP(adev), "dio432_init_defaults %d dac_ctrl=%08x",
			__LINE__, acq400rd32(adev, DAC_CTRL));
	//acq400wr32(adev, DAC_INT_CSR, 0);
	acq400wr32(adev, DAC_CTRL, dac_ctrl|DIO432_CTRL_FIFO_RST|DIO432_CTRL_DIO_RST);
	acq400wr32(adev, DAC_CTRL, dac_ctrl|DIO432_CTRL_FIFO_EN);

	if (measure_ao_fifo_ok){
		dev_info(DEVP(adev), "dio432_init_defaults() 60 measure_ao_fifo()");
		measure_ao_fifo(adev);
		dev_info(DEVP(adev), "dio432 max fifo samples %d", xo_dev->xo.max_fifo_samples);
		if (dio432_rowback){
			xo_dev->xo.max_fifo_samples -= dio432_rowback;
			dev_info(DEVP(adev), "dio432 max fifo samples %d dio432_rowback",
					xo_dev->xo.max_fifo_samples);
		}
	}

	acq400wr32(adev, DIO432_DI_FIFO_STATUS, DIO432_FIFSTA_CLR);
	acq400wr32(adev, DIO432_DO_FIFO_STATUS, DIO432_FIFSTA_CLR);
	//set_debugs("off");
	dev_info(DEVP(adev), "dio432_init_defaults %d dac_ctrl=%08x",
			__LINE__, acq400rd32(adev, DAC_CTRL));
	//@@todo dev_info(DEVP(adev), "dio432_init_defaults() 99 cursor %d", adev->cursor.hb[0]->ix);
}



/* correct for FPGA mismatch with front panel connectors */
int ao420_physChan(int lchan /* 1..4 */ )
{
	int ip;
	for (ip = 0; ip < AO_CHAN; ++ip){
		if (ao420_mapping[ip] == lchan){
			return ip;
		}
	}
	BUG();
}


void measure_ao_fifo(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned osam = 0xffffffff;
	unsigned sam;
	int values_per_lw = adev->data32? 1: 2;
	unsigned cr;
	int nblocks;

	if (measure_ao_fifo_ok == 0){
		dev_info(DEVP(adev), "measure_ao_fifo() stubbed");
		return;
	}
	dev_dbg(DEVP(adev), "measure_ao_fifo() 01");

	cr = acq400rd32(adev, DAC_CTRL);
	acq400wr32(adev, DAC_CTRL, cr &= ~(DAC_CTRL_LL|DAC_CTRL_DAC_EN));


	ao420_reset_fifo(adev);
	for (nblocks = 0; (sam = xo_dev->xo.getFifoSamples(adev)) != osam;
			osam = sam, ++nblocks){
		dev_dbg(DEVP(adev), "xo400_write_fifo(16384)");
		xo400_write_fifo(adev, 0, 1024);
	}

	dev_info(DEVP(adev), "measure_ao_fifo() write %d k, measure:%d samples\n",
			nblocks, sam);

	if (IS_AO42X(adev)){
		u32 dac_fifo_samples = acq400rd32(adev, DAC_FIFO_SAMPLES);
		int lwords = (dac_fifo_samples>>16)&DAC_FIFO_SAMPLES_MASK;

	/* MATCH lwords..samples is not exact */
		if ((lwords - sam*adev->nchan_enabled/values_per_lw) < 32){
			dev_info(DEVP(adev), "MATCHES: %08x lwords:%d samples:%d nchan:%d values_per_lw:%d\n",
				dac_fifo_samples,
				lwords, sam, adev->nchan_enabled, values_per_lw);
		}else{
			dev_warn(DEVP(adev), "MISMATCH: %08x lwords:%d samples:%d nchan:%d values_per_lw:%d\n",
				dac_fifo_samples,
				lwords, sam, adev->nchan_enabled, values_per_lw);
		}
	}
	xo_dev->xo.max_fifo_samples = sam;
	adev->hitide = sam - 32;
	if (adev->nchan_enabled > 8){
		adev->lotide = 9*sam/10;
	}else{
		adev->lotide = 8*sam/10;
	}

	for (xo_dev->xo.hshift = 0;
		(sam >> xo_dev->xo.hshift) > 256; ++xo_dev->xo.hshift)
		;
	dev_info(DEVP(adev), "setting max_fifo_samples:%u hshift:%u lotide:%d",
			sam, xo_dev->xo.hshift, adev->lotide);

	ao420_reset_fifo(adev);
}


void _acq400_mod_init_defaults(struct acq400_dev* adev)
{
	adev->sysclkhz = SYSCLK_M100;
}
void acq400_mod_init_defaults(struct acq400_dev* adev)
{
	_acq400_mod_init_defaults(adev);

	if (IS_ACQ42X(adev)){
		acq420_init_defaults(adev);
	}else if (IS_DIO432X(adev)){
		dio432_init_defaults(adev);
	}else{
		switch(GET_MOD_ID(adev)){
		case MOD_ID_ACQ430FMC:
		case MOD_ID_ACQ435ELF:
		case MOD_ID_ACQ436ELF:
		case MOD_ID_ACQ437ELF:
			acq43X_init_defaults(adev);
			break;
		case MOD_ID_AO420FMC:
		case MOD_ID_AO420FMC_CS2:
			ao420_init_defaults(adev, GET_MOD_ID(adev)==MOD_ID_AO420FMC_CS2);
			break;
		case MOD_ID_AO424ELF:
			ao424_init_defaults(adev);
			break;
		case MOD_ID_BOLO8:
		case MOD_ID_BOLO8B:
			bolo8_init_defaults(adev);
			break;
		case MOD_ID_PMODADC1:
			pmodadc1_init_defaults(adev);
			break;
		case MOD_ID_ACQ480FMC:
			acq480_init_defaults(adev);
			break;
		case MOD_ID_DIO_BISCUIT:
			switch(GET_MOD_IDV(adev)){
			case MOD_IDV_V2F:
				v2f_init_defaults(adev);
				break;
			case MOD_IDV_DIO:
			case MOD_IDV_QEN:
				qen_init_defaults(adev);
				break;
			case MOD_IDV_ACQ1014:
				acq1014_init_defaults(adev);
				break;
			}
			break;
		case MOD_ID_PIG_CELF:
			pig_celf_init_defaults(adev);
			break;
		case MOD_ID_RAD_CELF:
			rad_celf_init_defaults(adev);
			break;
		case MOD_ID_DAC_CELF:
		case MOD_ID_CPSC2_DAC:
			ao428_init_defaults(adev);
			break;
		default:
			dev_warn(DEVP(adev), "no custom init for module type %x",
						(adev)->mod_id>>MOD_ID_TYPE_SHL);
		}
	}
}




