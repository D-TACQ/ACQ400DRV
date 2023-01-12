/* ------------------------------------------------------------------------- *
 * acq400_debugfs.c  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 9 Mar 2014  
 *    Author: pgm                                                         
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
#include "bolo.h"
#include "acq400_debugfs.h"
#include "acq400_debugfs_internal.h"


void bolo8_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	struct dentry* chdir = debugfs_create_dir("current", adev->debug_dir);

	DBG_REG_CREATE(B8_SYS_CON);
	DBG_REG_CREATE(B8_TIM_CON);
	DBG_REG_CREATE(B8_INT_CSR);
	DBG_REG_CREATE_NAME_N(B8_CLK_CTR);
	DBG_REG_CREATE(B8_CLKDIV);
	DBG_REG_CREATE(B8_ADC_CON);
	DBG_REG_CREATE(B8_ADC_HITIDE);
	DBG_REG_CREATE(B8_ADC_FIFO_CNT);
	DBG_REG_CREATE_RW(B8_ADC_FIFO_STA);
	DBG_REG_CREATE_NAME_N(B8_ADC_SAMPLE_CNT);
	DBG_REG_CREATE(B8_DAC_CON);
	DBG_REG_CREATE(B8_DAC_WAVE_TOP);
	DBG_REG_CREATE(B8_DAC_FIFO_STA);
	DBG_REG_CREATE(B8_DAC_SAMPLE_CNT);
	DBG_REG_CREATE_RW(B8_DAC_SPI);
	DBG_REG_CREATE(B8_DAC_SPI_RBK);
/* Current ADc */
	DBG_REG_CREATE(B8_CAD_CON);
	DBG_REG_CREATE(B8_CAD_DELAY);

	CH_REG_CREATE(1, B8_CAD_1);
	CH_REG_CREATE(2, B8_CAD_2);
	CH_REG_CREATE(3, B8_CAD_3);
	CH_REG_CREATE(4, B8_CAD_4);
	CH_REG_CREATE(5, B8_CAD_5);
	CH_REG_CREATE(6, B8_CAD_6);
	CH_REG_CREATE(7, B8_CAD_7);
	CH_REG_CREATE(8, B8_CAD_8);
	CH_REG_CREATE(A1, B8_CAD_A1);
	CH_REG_CREATE(A2, B8_CAD_A2);
	CH_REG_CREATE(A3, B8_CAD_A3);
	CH_REG_CREATE(A4, B8_CAD_A4);
	CH_REG_CREATE(A5, B8_CAD_A5);
	CH_REG_CREATE(A6, B8_CAD_A6);
	CH_REG_CREATE(A7, B8_CAD_A7);
	CH_REG_CREATE(A8, B8_CAD_A8);
/* Offset DAc */
	DBG_REG_CREATE(B8_ODA_CON);
	DBG_REG_CREATE(B8_ODA_DATA);
}

/* ao424 ch knobs: working with a cache buffer, use regular accessor */
char * _ao424_create_ch_knobs(
	struct acq400_dev* adev, const char* name, u16 values[],
	char* pcursor)
{
	struct dentry* dir = debugfs_create_dir(name, adev->debug_dir);
	int ic;

	for (ic = 0; ic < AO424_MAXCHAN; ++ic){
		int nc = sprintf(pcursor, "%02d", ic+1);
		debugfs_create_x16(pcursor, S_IWUGO|S_IRUGO, dir, values+ic);
		pcursor += nc+1;
	}


	return pcursor;
}
void ao424_create_spans(struct acq400_dev* adev, char* pcursor)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	pcursor = _ao424_create_ch_knobs(adev, "spans",
			xo_dev->ao424_device_settings.u.ch.ao424_spans, pcursor);
	pcursor = _ao424_create_ch_knobs(adev, "init",
			xo_dev->ao424_device_settings.u.ch.ao424_initvals, pcursor);
}
void ao420_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE_CTRL(DAC_CTRL);
	DBG_REG_CREATE_CTRL(TIM_CTRL);
	DBG_REG_CREATE_CTRL(DAC_LOTIDE);
	DBG_REG_CREATE(DAC_FIFO_SAMPLES);
	DBG_REG_CREATE_RW(DAC_FIFO_STA);
	DBG_REG_CREATE(DAC_INT_CSR);
	DBG_REG_CREATE_NAME_N(DAC_CLK_CTR);
	DBG_REG_CREATE_NAME_N(DAC_SAMPLE_CTR);
	DBG_REG_CREATE(DAC_CLKDIV);
	if (IS_AO420(adev)||IS_AO428(adev)){
		DBG_REG_CREATE(AO420_RANGE);
		DBG_REG_CREATE_RW(AO420_DACSPI);
	}
	if (IS_AO420(adev)||IS_AO424(adev)){
		DBG_REG_CREATE(ADC_TRANSLEN);
		DBG_REG_CREATE(DAC_DEC);
		DBG_REG_CREATE(DAC_READ_LAT_AC);
		DBG_REG_CREATE(DAC_READ_LAT_MM);
	}
	if (IS_AO420(adev)){
		DBG_REG_CREATE(DAC_GAIN_OFF(1));
		DBG_REG_CREATE(DAC_GAIN_OFF(2));
		if (!IS_AO420_HALF436(adev)){
			DBG_REG_CREATE(DAC_GAIN_OFF(3));
			DBG_REG_CREATE(DAC_GAIN_OFF(4));
		}else{
			DBG_REG_CREATE(DAC_MUX);
		}

	}

	if (IS_AO428(adev)){
		DBG_REG_CREATE(AO428_OFFSET_1);
		DBG_REG_CREATE(AO428_OFFSET_2);
		DBG_REG_CREATE(AO428_OFFSET_3);
		DBG_REG_CREATE(AO428_OFFSET_4);
		DBG_REG_CREATE(AO428_OFFSET_5);
		DBG_REG_CREATE(AO428_OFFSET_6);
		DBG_REG_CREATE(AO428_OFFSET_7);
		DBG_REG_CREATE(AO428_OFFSET_8);
	}
	if (IS_AO424(adev)){
		DBG_REG_CREATE(DAC_424_CGEN);

		DBG_REG_CREATE(DAC_424_SNOOP);
		ao424_create_spans(adev, pcursor);
	}
	DBG_REG_CREATE(AO_DELAY66);
}

void adc_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE_CTRL(ADC_CTRL);
	DBG_REG_CREATE_CTRL(TIM_CTRL);
	DBG_REG_CREATE_CTRL(ADC_HITIDE);
	DBG_REG_CREATE(ADC_FIFO_SAMPLES);
	DBG_REG_CREATE_RW(ADC_FIFO_STA);
	DBG_REG_CREATE(ADC_INT_CSR);
	DBG_REG_CREATE_NAME_N(ADC_CLK_CTR);
	DBG_REG_CREATE_NAME_N(ADC_SAMPLE_CTR);
	DBG_REG_CREATE_NAME_N(ADC_SAMPLE_CLK_CTR);
	DBG_REG_CREATE(ADC_CLKDIV);
	if (IS_ACQ425(adev)){
		DBG_REG_CREATE(ACQ425_BANK);
	}
	if (IS_ACQ423(adev)){
		DBG_REG_CREATE(ACQ423_BANK);
	}
	DBG_REG_CREATE(ADC_ACC_DEC);
}

void acq420_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	adc_createDebugfs(adev, pcursor);
	DBG_REG_CREATE(EVT_SC_LATCH);
	if (IS_ACQ420(adev)){
		DBG_REG_CREATE(ADC_GAIN);
	}

	DBG_REG_CREATE(ADC_CONV_TIME);
	DBG_REG_CREATE(ADC_TRANSLEN);
	if (IS_ACQ423(adev)){
		DBG_REG_CREATE(ACQ423_SPAN_A);
		DBG_REG_CREATE(ACQ423_SPAN_B);
		DBG_REG_CREATE(ACQ423_SPAN_C);
		DBG_REG_CREATE(ACQ423_SPAN_D);
	}else if (IS_ACQ424(adev)){
		DBG_REG_CREATE(ACQ424_SHOT_LENGTH);
		DBG_REG_CREATE(ACQ424_CLK_MIN_MAX);
	}
}

void acq480_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	adc_createDebugfs(adev, pcursor);
	DBG_REG_CREATE_CTRL(ADC_TRANSLEN);
	DBG_REG_CREATE(ACQ480_TRAIN_CTRL);
	DBG_REG_CREATE(ACQ480_TRAIN_HI_VAL);
	DBG_REG_CREATE(ACQ480_TRAIN_LO_VAL);
	DBG_REG_CREATE(ACQ480_ADC_MULTIRATE);
	if (HAS_FPGA_FIR(adev)){
		DBG_REG_CREATE(ACQ480_FIRCO_LOAD);
		DBG_REG_CREATE(ACQ480_FIRCO_CSR);
	}
}
void pmodadc1_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	adc_createDebugfs(adev, pcursor);
}
void acq43x_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	adc_createDebugfs(adev, pcursor);

	switch(GET_MOD_ID(adev)){
	case MOD_ID_ACQ435ELF:
	case MOD_ID_ACQ436ELF:
	case MOD_ID_ACQ437ELF:
		DBG_REG_CREATE(ACQ435_SW_EMB_WORD1);
		DBG_REG_CREATE(ACQ435_SW_EMB_WORD2);
		break;
	case MOD_ID_ACQ430FMC:
		if (!(adev->mod_id&MOD_ID_IS_SLAVE)){
			DBG_REG_CREATE(FMC_DSR);
		}
	}
	DBG_REG_CREATE(EVT_SC_LATCH);
	DBG_REG_CREATE(ACQ435_MODE);
	DBG_REG_CREATE(ADC_TRANSLEN);
	DBG_REG_CREATE(ATD_TRIGGERED);
	DBG_REG_CREATE(ATD_MASK_AND);
	DBG_REG_CREATE(ATD_MASK_OR);
}

void acq465_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	adc_createDebugfs(adev, pcursor);
	DBG_REG_CREATE(ACQ465_LCS);
	DBG_REG_CREATE(ACQ465_BANK_MODE);
	DBG_REG_CREATE(ADC_TRANSLEN);
}

void acq494_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE(MOD_ID);
	DBG_REG_CREATE(TDC_CR);
	DBG_REG_CREATE(TDC_FIFO_COUNT);
	DBG_REG_CREATE(TDC_FIFO_STATUS);
	DBG_REG_CREATE_NAME_N(TDC_CH1_EVT_COUNT);
	DBG_REG_CREATE_NAME_N(TDC_CH2_EVT_COUNT);
	DBG_REG_CREATE_NAME_N(TDC_CH3_EVT_COUNT);
	DBG_REG_CREATE_NAME_N(TDC_CH4_EVT_COUNT);
	DBG_REG_CREATE(TDC_TRAIN_HI_VAL);
	DBG_REG_CREATE(TDC_TRAIN_LO_VAL);
	DBG_REG_CREATE(TDC_LOADED_CALIB);
	DBG_REG_CREATE(TDC_CH_MASK);
}

void dio432_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE( DIO432_CTRL);
	DBG_REG_CREATE( DIO432_TIM_CTRL);
	if (!IS_DIO482_PG(adev)){
		DBG_REG_CREATE( DIO432_DI_HITIDE);
		DBG_REG_CREATE( DIO432_DI_FIFO_COUNT);
		DBG_REG_CREATE_RW( DIO432_DI_FIFO_STATUS);
	}
	DBG_REG_CREATE( DIO432_DIO_ICR);
	DBG_REG_CREATE_NAME_N( ADC_CLK_CTR);
	if (IS_DIO482TD_PG(adev)){
		DBG_REG_CREATE_NAME_N(DIO482_PG_FPTRG_COUNT);
	}
	if (IS_DIO482_PG(adev)){
		DBG_REG_CREATE_NAME_N(DIO482_PG_WDT);
	}
	DBG_REG_CREATE( DIO_CLKDIV);
	if (IS_DIO422ELF(adev)){
		DBG_REG_CREATE(DIO422_OE_CONFIG);
	}else{
		DBG_REG_CREATE( DIO432_DIO_CPLD_CTRL);
	}

	DBG_REG_CREATE_NAME_N( DIO432_DIO_SAMPLE_COUNT );
	DBG_REG_CREATE( DIO432_DI_SNOOP );

	if (IS_DIO482PPW(adev)){
		int px;
		DBG_REG_CREATE(DIO482_PG_IMM_MASK);
		for (px = PPW_MIN; px <= PPW_MAX; ++px){
			DBG_REG_CREATE_NAME_NC_NUM("ppw", px, "TRG", PPW_TRG(px));
			DBG_REG_CREATE_NAME_NC_NUM("ppw", px, "PWM", PPW_PWM(px));
			DBG_REG_CREATE_NAME_NC_NUM("ppw", px, "REP", PPW_REP(px));
			DBG_REG_CREATE_NAME_NC_NUM("ppw", px, "STA", PPW_STA(px));
		}
		DBG_REG_CREATE(DIO482_PG_IMM_DO);
		return;
	}
	if (IS_DIO432FMC(adev)){
		DBG_REG_CREATE(DIO432_DEBUG);
		DBG_REG_CREATE(DIO432_CMD_DEBUG);
	}
	if (IS_DIO482FMC(adev) && !IS_DIO482_PG(adev)){
		DBG_REG_CREATE(DIO482_COS_STA);
		DBG_REG_CREATE(DIO482_COS_EN);
	}
	if (!IS_DIO482_PG(adev)){
		DBG_REG_CREATE( DIO432_DO_LOTIDE);
		DBG_REG_CREATE( DIO432_DO_FIFO_COUNT);
		DBG_REG_CREATE( DIO432_DO_FIFO_STATUS);
	}
	if (IS_DIO482FMC(adev) && GET_MOD_IDV(adev)==MOD_IDV_PWM2){
		DBG_REG_CREATE(PWM_SOURCE_CLK_CTRL);
	}

	if (IS_DIO482_PG(adev)){
		DBG_REG_CREATE(DIO482_PG_GPGCR);
		DBG_REG_CREATE(DIO482_PG_GPGDR);
		DBG_REG_CREATE(DIO482_PG_IMM_MASK);
		DBG_REG_CREATE(DIO482_PG_IMM_DO);
	}
}

#define V2F_FREQ_OFF_1		V2F_FREQ_OFF
#define V2F_FREQ_OFF_2		(V2F_FREQ_OFF_1+0x4)
#define V2F_FREQ_OFF_3		(V2F_FREQ_OFF_1+0x8)
#define V2F_FREQ_OFF_4		(V2F_FREQ_OFF_1+0xc)

#define V2F_FREQ_SLO_1		V2F_FREQ_SLO
#define V2F_FREQ_SLO_2		(V2F_FREQ_SLO+0x4)
#define V2F_FREQ_SLO_3		(V2F_FREQ_SLO+0x8)
#define V2F_FREQ_SLO_4		(V2F_FREQ_SLO+0xc)


void v2f_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE( V2F_CTRL );
	DBG_REG_CREATE( V2F_STAT );
	DBG_REG_CREATE( V2F_CHAN_SEL );
	DBG_REG_CREATE( V2F_FREQ_OFF_1 );
	DBG_REG_CREATE( V2F_FREQ_OFF_2 );
	DBG_REG_CREATE( V2F_FREQ_OFF_3 );
	DBG_REG_CREATE( V2F_FREQ_OFF_4 );
	DBG_REG_CREATE( V2F_FREQ_SLO_1 );
	DBG_REG_CREATE( V2F_FREQ_SLO_2 );
	DBG_REG_CREATE( V2F_FREQ_SLO_3 );
	DBG_REG_CREATE( V2F_FREQ_SLO_4 );
}

void dio_biscuit_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE(DIOUSB_CTRL);
	DBG_REG_CREATE(DIOUSB_STAT);
}
void qen_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE(QEN_CTRL);
	DBG_REG_CREATE(TIM_CTRL);
	DBG_REG_CREATE(QEN_FIFO_SAMPLES);
	DBG_REG_CREATE(QEN_FIFO_STA);
	DBG_REG_CREATE(QEN_INT_CSR);
	DBG_REG_CREATE(QEN_CLK_CTR);
	DBG_REG_CREATE(QEN_SAMPLE_CTR);
	DBG_REG_CREATE(QEN_SAMPLE_CLK_CTR);
	DBG_REG_CREATE(QEN_CLKDIV);
	DBG_REG_CREATE(QEN_TRANSLEN);
	DBG_REG_CREATE(QEN_INDEX_HOME);

	DBG_REG_CREATE(QEN_DIO_CTRL);
	DBG_REG_CREATE(QEN_ENC_COUNT);
	DBG_REG_CREATE(QEN_ZCOUNT);
	DBG_REG_CREATE(QEN_DI_MON);
	DBG_REG_CREATE(QEN_ECOUNT);
	DBG_REG_CREATE(QEN_POS_ABS_TRG);
	DBG_REG_CREATE(QEN_POS_PRD_TRG);
	DBG_REG_CREATE(QEN_POS_PRD_HYST);
	DBG_REG_CREATE(QEN_POS_PRD_CNT);
}

void acq1014_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE(DIO1014_CR);
	DBG_REG_CREATE(DIO1014_SR);
}

void pig_celf_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE(ADC_CTRL);
	DBG_REG_CREATE(TIM_CTRL);
	DBG_REG_CREATE(ADC_HITIDE);
	DBG_REG_CREATE(ADC_FIFO_SAMPLES);
	DBG_REG_CREATE_RW(ADC_FIFO_STA);
	DBG_REG_CREATE(ADC_INT_CSR);
	DBG_REG_CREATE(ADC_CLK_CTR);
	DBG_REG_CREATE(ADC_SAMPLE_CTR);
	DBG_REG_CREATE(ADC_SAMPLE_CLK_CTR);
	DBG_REG_CREATE(PC_DDS_DAC_CLKDIV);
	DBG_REG_CREATE(PC_ADC_CLKDIV);
	DBG_REG_CREATE(PC_DDS_PHASE_INC);
}

void rad_celf_createDebugfs(struct acq400_dev* adev, char* pcursor)
{
	DBG_REG_CREATE(RAD_CTL);
	DBG_REG_CREATE(RAD_DDS_A);
	DBG_REG_CREATE(RAD_DDS_B);
	DBG_REG_CREATE(RAD_DDS_AB);
	DBG_REG_CREATE(RAD_DDS_C);
	DBG_REG_CREATE_NAME_N(RAD_CLK_PPS_LATCH);
}

void acq400_createDebugfs(struct acq400_dev* adev)
{
	char* pcursor;
	unsigned *dev_rc_cache_data;
	if (!acq400_debug_root){
		acq400_debug_root = debugfs_create_dir("acq400", 0);
		if (!acq400_debug_root){
			dev_warn(&adev->pdev->dev, "failed create dir acq420");
			return;
		}
	}

	dev_rc_cache_data = dev_rc_alloc_cache();
	dev_rc_init(adev, &adev->clk_reg_cache, adev->dev_virtaddr, adev->of_prams.site, dev_rc_cache_data);
	dev_rc_init(adev, &adev->ctrl_reg_cache, adev->dev_virtaddr, adev->of_prams.site, dev_rc_cache_data);
	pcursor = adev->debug_names = kmalloc(4096, GFP_KERNEL);


	adev->debug_dir = debugfs_create_dir(adev->dev_name, acq400_debug_root);

	if (!adev->debug_dir){
		dev_warn(&adev->pdev->dev, "failed create dir acq400.x");
		return;
	}
	DBG_REG_CREATE(MOD_ID);

	if (IS_ACQ42X(adev)){
		acq420_createDebugfs(adev, pcursor);
	}else if (IS_DIO432X(adev) || IS_DIO482TD_PG(adev)){
		dio432_createDebugfs(adev, pcursor);
	}else if (IS_DIO422AQB(adev)){
		qen_createDebugfs(adev, pcursor);
	}else{
		switch(GET_MOD_ID(adev)){
		case MOD_ID_BOLO8:
		case MOD_ID_BOLO8B:
			bolo8_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_AO420FMC:
		case MOD_ID_AO420FMC_CS2:
		case MOD_ID_AO424ELF:
		case MOD_ID_DAC_CELF:
			ao420_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_ACQ435ELF:
		case MOD_ID_ACQ436ELF:
		case MOD_ID_ACQ437ELF:
		case MOD_ID_ACQ430FMC:
		case MOD_ID_TIMBUS:
			acq43x_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_ACQ465ELF:
			acq465_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_ACQ494FMC:
			acq494_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_ACQ480FMC:
			acq480_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_PMODADC1:
			pmodadc1_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_ACQ400T_FMC:
		case MOD_ID_ACQ400T_ELF:
			break;
		case MOD_ID_DIO_BISCUIT:
			switch(GET_MOD_IDV(adev)){
			case MOD_IDV_V2F:
				return v2f_createDebugfs(adev, pcursor);
			case MOD_IDV_DIO:
				return dio_biscuit_createDebugfs(adev, pcursor);
			case MOD_IDV_QEN:
				return qen_createDebugfs(adev, pcursor);
			case MOD_IDV_ACQ1014:
				return acq1014_createDebugfs(adev, pcursor);
			}
			break;
		case MOD_ID_PIG_CELF:
			pig_celf_createDebugfs(adev, pcursor);
			break;
		case MOD_ID_RAD_CELF:
		case MOD_ID_DDS_WERA:
			rad_celf_createDebugfs(adev, pcursor);
			break;
		default:
			dev_warn(&adev->pdev->dev, "unsupported MOD_ID:%02x",
				GET_MOD_ID(adev));
		}
	}

	dev_rc_finalize(&adev->clk_reg_cache, adev->of_prams.site, RC_HAS_TIMER);
	dev_rc_finalize(&adev->ctrl_reg_cache, adev->of_prams.site, !RC_HAS_TIMER);
}

void acq400_removeDebugfs(struct acq400_dev* adev)
{
	debugfs_remove_recursive(adev->debug_dir);
	kfree(adev->debug_names);
}


void acq2006_createDebugfs(struct acq400_dev* adev)
{
	char* pcursor;
	int site;
	int sites =
		IS_ACQ2X06SC(adev)? 6:
		IS_ACQ1001SC(adev)? 6:	/* special case counters eg RADCELF */
		IS_KMCx_SC(adev)  ? 2: 0;

	unsigned* dev_rc_cache_data = dev_rc_alloc_cache();
	dev_rc_init(adev, &adev->clk_reg_cache,
			adev->dev_virtaddr, adev->of_prams.site, dev_rc_cache_data);
	dev_rc_init(adev, &adev->ctrl_reg_cache,
			adev->dev_virtaddr, adev->of_prams.site, dev_rc_cache_data);
	if (!acq400_debug_root){
		acq400_debug_root = debugfs_create_dir("acq400", 0);
		if (!acq400_debug_root){
			dev_warn(&adev->pdev->dev, "failed create dir acq420");
			return;
		}
	}
	pcursor = adev->debug_names = kmalloc(4096, GFP_KERNEL);


	adev->debug_dir = debugfs_create_dir(adev->dev_name, acq400_debug_root);

	if (!adev->debug_dir){
		dev_warn(&adev->pdev->dev, "failed create dir acq400.x");
		return;
	}
	DBG_REG_CREATE(MOD_ID);
	DBG_REG_CREATE_CTRL(MOD_CON);
	DBG_REG_CREATE_CTRL(AGGREGATOR);
	DBG_REG_CREATE(AGGSTA);
	DBG_REG_CREATE(DATA_ENGINE_0);
	DBG_REG_CREATE(DATA_ENGINE_1);
	if (IS_ACQ2X06SC(adev)){
		if (IS_AXI64(adev)){
			DBG_REG_CREATE(AXI_DMA_ENGINE_DATA);
		}else{
			DBG_REG_CREATE(DATA_ENGINE_2);
			DBG_REG_CREATE(DATA_ENGINE_3);
		}
	}else if(IS_AXI64(adev)){
		DBG_REG_CREATE(AXI_DMA_ENGINE_DATA);
	}

	DBG_REG_CREATE_CTRL(GPG_CONTROL);
	DBG_REG_CREATE(HDMI_SYNC_DAT);
	DBG_REG_CREATE(HDMI_SYNC_OUT_SRC);
	DBG_REG_CREATE(EVT_BUS_SRC);
	DBG_REG_CREATE(SIG_SRC_ROUTE);
	DBG_REG_CREATE(FPCTL);
	DBG_REG_CREATE(SYS_CLK);
	DBG_REG_CREATE(AGG_DEBUG);
	DBG_REG_CREATE(DISTRIBUTOR);
	DBG_REG_CREATE(DISTRIBUTOR_STA);
	DBG_REG_CREATE(GPG_DEBUG);
	DBG_REG_CREATE(USEC_CCR);
	DBG_REG_CREATE(SPI_PERIPHERAL_CS);
	DBG_REG_CREATE(DIST_DBG);

	DBG_REG_CREATE(AXI_DMA_DEBUG_0);
	DBG_REG_CREATE(AXI_DMA_DEBUG_1);
	DBG_REG_CREATE(AXI_DMA_DEBUG_2);
	DBG_REG_CREATE(AXI_DMA_DEBUG_3);


	DBG_REG_CREATE_NAME("CLK_EXT", ACQ2006_CLK_COUNT(EXT_DX));
	DBG_REG_CREATE_NAME("CLK_MB",  ACQ2006_CLK_COUNT(MB_DX));

	for (site = 1; site <= sites + IS_ACQ1001SC(adev); ++site){
		char name[20];
		sprintf(name, "CLK_%d", site);
		DBG_REG_CREATE_NAME(name, ACQ2006_CLK_COUNT(SITE2DX(site)));
	}

	DBG_REG_CREATE_NAME("TRG_EXT", ACQ2006_TRG_COUNT(EXT_DX));
	DBG_REG_CREATE_NAME("TRG_MB",  ACQ2006_TRG_COUNT(MB_DX));
	for (site = 1; site <= sites; ++site){
		char name[20];
		sprintf(name, "TRG_%d", site);
		DBG_REG_CREATE_NAME(name, ACQ2006_TRG_COUNT(SITE2DX(site)));
	}

	DBG_REG_CREATE_NAME("SYN_EXT", ACQ2006_SYN_COUNT(EXT_DX));
	DBG_REG_CREATE_NAME("SYN_MB",  ACQ2006_SYN_COUNT(MB_DX));
	for (site = 1; site <= sites; ++site){
		char name[20];
		sprintf(name, "SYN_%d", site);
		DBG_REG_CREATE_NAME(name, ACQ2006_SYN_COUNT(SITE2DX(site)));
	}
	DBG_REG_CREATE_NAME("EVT_EXT", ACQ2006_EVT_COUNT(EXT_DX));
	DBG_REG_CREATE_NAME("EVT_MB",  ACQ2006_EVT_COUNT(MB_DX));
	for (site = 1; site <= sites; ++site){
		char name[20];
		sprintf(name, "EVT_%d", site);
		DBG_REG_CREATE_NAME(name, ACQ2006_EVT_COUNT(SITE2DX(site)));
	}

	if (IS_SC(adev) && !(IS_ACQ2006SC(adev) && FPGA_REV(adev)<8)){
		char name[16];
		int ii;
		for (ii = 0; ii < SPADMAX; ++ii){
			snprintf(name, 16, "spad%d", ii);
			DBG_REG_CREATE_NAME_NC(name, SPADN(ii));
		}
		for (ii = 0; ii < SPADMAX; ++ii){
			snprintf(name, 16, "xo_spad%d", ii);
			DBG_REG_CREATE_NAME_NC(name, XO_SPADN(ii));
		}
	}
	if (IS_ACQ2X06SC(adev) && IS_ACQ2106_WR(adev)){
		DBG_REG_CREATE(WR_CTRL);
		DBG_REG_CREATE(WR_CLK_GEN);
		DBG_REG_CREATE(WR_TAI_CUR_L);
		DBG_REG_CREATE(WR_TAI_CUR_H);
		DBG_REG_CREATE(WR_TAI_TRG0);
		DBG_REG_CREATE(WR_TAI_STAMP);
		DBG_REG_CREATE(WR_CUR_VERNR);
		DBG_REG_CREATE(WR_TAI_TRG1);

		if (IS_ACQ2106_TIGA(adev)){
			DBG_REG_CREATE(WR_TS_S1);
			DBG_REG_CREATE(WR_TS_S2);
			DBG_REG_CREATE(WR_TS_S3);
			DBG_REG_CREATE(WR_TS_S4);
			DBG_REG_CREATE(WR_TS_S5);
			DBG_REG_CREATE(WR_TS_S6);

			DBG_REG_CREATE(WR_TT_S1);
			DBG_REG_CREATE(WR_TT_S2);
			DBG_REG_CREATE(WR_TT_S3);
			DBG_REG_CREATE(WR_TT_S4);
			DBG_REG_CREATE(WR_TT_S5);
			DBG_REG_CREATE(WR_TT_S6);

			DBG_REG_CREATE(WR_TIGA_CSR);
		}
	}
	dev_rc_finalize(&adev->clk_reg_cache, adev->of_prams.site, RC_HAS_TIMER);
	dev_rc_finalize(&adev->ctrl_reg_cache, adev->of_prams.site, !RC_HAS_TIMER);
}
