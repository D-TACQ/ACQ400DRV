/*
 * acq420FMC.h
 *
 *  Created on: Mar 11, 2013
 *      Author: pgm
 */

#ifndef ACQ420FMC_H_
#define ACQ420FMC_H_

#include "acq400_includes.h"
#include "acq400_mod_id.h"


/* Offsets for control registers in the AXI MM2S FIFO */

#define AXI_FIFO              	0x1000
#define AXI_FIFO_LEN          	0x1000
#define AXI_ATD_RAM	      	0xe000		/* Threshold regs */
#define AXI_ATD_LEN		0x1000

#define OF_IRQ_HITIDE		1	/* index of HITIDE in dtb */
#define OF_IRQ_COUNT		3	/* number of items */
#define OF_IRQ_MAGIC		32	/* add to the OF number to get actual */

#define ADC_BASE		0x0000
#define MOD_ID			(ADC_BASE+0x00)

#define MOD_CON			(ADC_BASE+0x04)
#define MCR			MOD_CON
#define ADC_CTRL		MOD_CON
#define DAC_CTRL		MOD_CON
#define TIM_CTRL		(ADC_BASE+0x08)
#define ADC_HITIDE		(ADC_BASE+0x0C)
#define DAC_LOTIDE		ADC_HITIDE
#define ADC_FIFO_SAMPLES	(ADC_BASE+0x10)
#define DAC_FIFO_SAMPLES	ADC_FIFO_SAMPLES
#define ADC_FIFO_STA		(ADC_BASE+0x14)
#define DAC_FIFO_STA		ADC_FIFO_STA
#define DIOUSB_STA		ADC_FIFO_STA
#define ADC_INT_CSR		(ADC_BASE+0x18)
#define DAC_INT_CSR		ADC_INT_CSR
#define ADC_CLK_CTR		(ADC_BASE+0x1C)
#define DAC_CLK_CTR		ADC_CLK_CTR
#define ADC_SAMPLE_CTR		(ADC_BASE+0x20)
#define DAC_SAMPLE_CTR		ADC_SAMPLE_CTR
#define ADC_SAMPLE_CLK_CTR	(ADC_BASE+0x24)
#define FMC_DSR			(ADC_BASE+0x28)
#define ACQ435_SW_EMB_WORD1	(ADC_BASE+0x28)
#define ACQ435_SW_EMB_WORD2	(ADC_BASE+0x2c)
#define EVT_SC_LATCH		(ADC_BASE+0x30)


#define ADC_CLKDIV		(ADC_BASE+0x40)
#define DAC_CLKDIV		ADC_CLKDIV
#define ADC_GAIN		(ADC_BASE+0x44)
#define DAC_424_CGEN		(ADC_BASE+0x44)
/* obsolete R3
#define ADC_FORMAT 		(ADC_BASE+0x48)
*/


#define ACQ435_MODE		(ADC_BASE+0x44)
#define AO420_RANGE		(ADC_BASE+0x44)
#define ACQ425_BANK             (ADC_BASE+0x44) /* MUST MATCH ACQ435_MODE in address and meaning! */
#define ACQ423_BANK		(ADC_BASE+0x44)
#define AO420_DACSPI		(ADC_BASE+0x48)

#define DAC_424_SNOOP		(ADC_BASE+0x4C)
#define ADC_CONV_TIME 		(ADC_BASE+0x4C) /*(mask 0x000000FF)*/

#define ADC_TRANSLEN		(ADC_BASE+0x50)
#define ADC_ACC_DEC		(ADC_BASE+0x54)
#define DAC_DEC			ADC_ACC_DEC

#define ADC_NACC_SAMPLES	(ADC_BASE+0x100)

#define PWM_SOURCE_CLK_CTRL	(ADC_BASE+0x58)



#define DIO482_COS_STA         (ADC_BASE+0x60)
#define DIO482_COS_EN          (ADC_BASE+0x64)

#define ATD_TRIGGERED		(ADC_BASE+0x60)
#define ATD_MASK_AND		(ADC_BASE+0x64)
#define ATD_MASK_OR		(ADC_BASE+0x68)

#define ACQ423_SPAN_A		(ADC_BASE+0x60)
#define ACQ423_SPAN_B		(ADC_BASE+0x64)
#define ACQ423_SPAN_C		(ADC_BASE+0x68)
#define ACQ423_SPAN_D		(ADC_BASE+0x6C)

#define AO_DELAY66		(ADC_BASE+0x60)

#define DTD_CTRL		(ADC_BASE+0x64)

#define ACQ424_SHOT_LENGTH	(ADC_BASE+0x70)
#define ACQ424_CLK_MIN_MAX	(ADC_BASE+0x74)

#define DAC_READ_LAT_AC		(ADC_BASE+0x70)
#define DAC_READ_LAT_MM		(ADC_BASE+0x74)

#define ACQ480_TRAIN_CTRL	(ADC_BASE+0x28)
#define ACQ480_TRAIN_HI_VAL	(ADC_BASE+0x2C)
#define ACQ480_TRAIN_LO_VAL	(ADC_BASE+0x30)

#define ACQ480_ADC_MULTIRATE	(ADC_BASE+0x58)


#define AO428_OFFSET_1		(ADC_BASE+0x80)
#define AO428_OFFSET_2		(ADC_BASE+0x84)
#define AO428_OFFSET_3		(ADC_BASE+0x88)
#define AO428_OFFSET_4		(ADC_BASE+0x8c)
#define AO428_OFFSET_5		(ADC_BASE+0x90)
#define AO428_OFFSET_6		(ADC_BASE+0x94)
#define AO428_OFFSET_7		(ADC_BASE+0x98)
#define AO428_OFFSET_8		(ADC_BASE+0x9c)

/* AO420 CH MAC GAIN and offset control */

#define DAC_MATH_BASE		(ADC_BASE+0x80)
#define DAC_GAIN_OFF(n)		(DAC_MATH_BASE+(n-1)*sizeof(u32))

#define DAC_MUX			(ADC_BASE+0x90)			/* ACQ436 upper half only */

#define ACQ480_FIRCO_LOAD	(ADC_BASE+0x80)
#define ACQ480_FIRCO_CSR	(ADC_BASE+0x84)

#define DAC_MATH_GAIN_SHL	16
#define DAC_MATH_OFFS_SHL	0

#define SPADN(ix)		(ADC_BASE+0x80+(ix)*sizeof(u32))
#define SPADMAX			8

#define XO_SPADN(ix)		(ADC_BASE+0xC0+(ix)*sizeof(u32))

#define ADC_FIFO_SAMPLE_MASK	0xff

#define FIFO_HISTO_SZ	      	(1<<8)
#define STATUS_TO_HISTO(stat)	((stat)&ADC_FIFO_SAMPLE_MASK)

/* MOD_ID Bitfields */
#define MOD_ID_TYPE_SHL		24
#define MOD_ID_IS_SLAVE		(1<<23)
#define MOD_ID_IS_CLKOUT	(1<<22)		// SITE 0 ONLY
#define MOD_ID_VERSION_SHL	16
#define MOD_ID_REV_SHL		0
#define MOD_ID_REV_MASK		0x0000ffff



/* ADC_CTRL Bitfields */
#define ADC_CTRL_DIRECT_NOT_ACC (1<<23)
#define ADC_CTRL_423_CLK_FROM_SYNC (1<<22)
#define ADC_CTRL_424_EMUL_196	(1<<21)		/* output raw data in ACQ196 order for back-compatibility */
#define ADC_CTRL_SYNC_TRG_N  	(1<<20)
#define ADC_CTRL_480_TWO_LANE_MODE (1<<19)
#define ADC_CTRL_482_CMAP	(1<<18)
#define ADC_CTRL_42x_RES_SHL	16
#define ADC_CTRL_42x_RES_MASK	0x3
#define ADC_CTRL_RGM_GATE_HI    (1 << 15)       /* 0x00008000 */
#define ADC_CTRL_RGM_GATE       (7 << 12)       /* 0x00007000 */
#define ADC_CTRL_RGM_MODE_SHL   (10)
#define ADC_CTRL_RGM_MODE_MASK  0x3
#define ADC_CTRL_435_GATE_SYNC	(1 << 10)	/* special resync mode */

#define ADC_CTRL_ES_EN		(1 << 9)	/* enables ES ACQ480FMC */
#define	ADC_CTRL_480_DCLK_SYNC  (1 << 8)
#define ADC_CTRL32B_data	(1 << 7)	/* ACQ420FMC */
#define ADC_CTRL_420_18B	(1 << 6)	/* ACQ420FMC */
#define ADC_CTRL_435_EMBED_STR	(1 << 6)	/* ACQ435 bitslice data */
#define ADC_CTRL_RAMP_EN 	(1 << 5)	/* Deprecated, sadly. Use SPAD */
#define ADC_CTRL_ADC_EN		(1 << 4)

#define DAC_CTRL_AWG_ABORT	(1 << 12)
#define DAC_CTRL_RTM_MODE	(1 <<10)
#define DAC_CTRL_LL		(1 << 8)	/* AO420FMC, AO424ELF  */
#define DAC_CTRL_TWOCMP		(1 << 9)	/* AO424ELF  */

#define ADC_CTRL_ADC_RST	(1 << 3)
#define ADC_CTRL_FIFO_EN	(1 << 2)
#define ADC_CTRL_FIFO_RST	(1 << 1)
#define ADC_CTRL_MODULE_EN	(1 << 0)	/* enable at enumeration, leave up */

#define ADC480_CTRL_TRAIN_OK		(1 << 31)
#define ADC480_CTRL_SYNC_TRAIN		(1<<7)
#define ADC480_CTRL_DESKEW_TRAIN	(1<<6)

#define ADC480_CTRL_FRAME_HI_SHL 20
#define ADC480_CTRL_FRAME_LO_SHL 16
#define ADC480_CTRL_FRAME_MASK	 0x0f

#define DAC_CTRL_DAC_EN		ADC_CTRL_ADC_EN
#define DAC_CTRL_DAC_RST	ADC_CTRL_ADC_RST
#define DAC_CTRL_FIFO_EN	ADC_CTRL_FIFO_EN
#define DAC_CTRL_FIFO_RST	ADC_CTRL_FIFO_RST
#define DAC_CTRL_MODULE_EN	ADC_CTRL_MODULE_EN

#define DAC_CTRL_ENABLE_ALL 	(DAC_CTRL_DAC_EN|DAC_CTRL_FIFO_EN|DAC_CTRL_MODULE_EN)

#define ADC_CTRL_RGM_GATE_SHL	12

#define ADC_CTRL_RST_ALL 	(ADC_CTRL_ADC_RST | ADC_CTRL_FIFO_RST)
#define ADC_CTRL_ENABLE_CAPTURE (ADC_CTRL_ADC_EN | ADC_CTRL_FIFO_EN)
#define ADC_CTRL_ENABLE_FIFO	(ADC_CTRL_MODULE_EN | ADC_CTRL_FIFO_EN)


#define TIM_CTRL_EVENT1_SHL	28
#define TIM_CTRL_EVENT0_SHL	24
#define TIM_CTRL_TRIG_SHL	20
#define TIM_CTRL_SYNC_SHL	16
#define TIM_CTRL_CLK_SHL	12
#define TIM_CTRL_MODE_SHL	0

#define CTRL_SIG_MASK	0xf
#define CTRL_SIG_RISING	0x8
#define CTRL_SIG_SEL	0x7

#define TIM_CTRL_MODE_EV1_EN	(1 << (4+TIM_CTRL_MODE_SHL))
#define TIM_CTRL_MODE_EV0_EN	(1 << (3+TIM_CTRL_MODE_SHL))
#define TIM_CTRL_MODE_HW_TRG_EN	(1 << (2+TIM_CTRL_MODE_SHL))
#define TIM_CTRL_MODE_SYNC	(1 << (1+TIM_CTRL_MODE_SHL))
#define TIM_CTRL_MODE_HW_CLK	(1 << (0+TIM_CTRL_MODE_SHL))

#define ADC480_FIFO_STA_DONE_MASK		0xff
#define ADC480_FIFO_STA_SYNC_DONE_SHL		16
#define ADC480_FIFO_STA_DESKEW_DONE_SHL		8

#define ADC_FIFO_STA_CLK	(1<<7)
#define ADC_FIFO_STA_TRG	(1<<6)
#define ADC_FIFO_STA_ACC	(1<<5)
#define ADC_FIFO_STA_ACTIVE	(1<<4)
#define ADC_FIFO_STA_FULL	(1<<3)
#define ADC_FIFO_STA_EMPTY	(1<<2)
#define ADC_FIFO_STA_OVER	(1<<1)
#define ADC_FIFO_STA_UNDER	(1<<0)

#define ADC_FIFO_STA_ERR \
	(ADC_FIFO_STA_UNDER|ADC_FIFO_STA_OVER|ADC_FIFO_STA_FULL)

#define ADC_FIFO_FLAGS 		(ADC_FIFO_STA_ERR|ADC_FIFO_STA_EMPTY)



#define DIO_INT_CSR_COS		(1<<11)
#define ADC_INT_CSR_EVENT1	(1<<10)
#define ADC_INT_CSR_EVENT0	(1<<9)
#define ADC_INT_CSR_HITIDE	(1<<8)

#define DIO_INT_CSR_COS_EN	(1<<3)
#define ADC_INT_CSR_EVENT1_EN	(1<<2)
#define ADC_INT_CSR_EVENT0_EN	(1<<1)
#define ADC_INT_CSR_HITIDE_EN	(1<<0)


#define ACQ465_LCS		(ADC_BASE+0x2c)
#define ACQ465_BANK_MODE	(ADC_BASE+0x44)
#define ACQ465_DEBUG		(ADC_BASE+0x58)

#define ACQ465_LCS_MASK		0x000000ff

#define ACQ465_BANK_FILTER_MASK 0x00000300

/* enable both event sources, clear both status, ready to run .. */
#define ADC_INT_CSR_COS_EN_ALL \
	(ADC_INT_CSR_EVENT1|ADC_INT_CSR_EVENT0|\
	ADC_INT_CSR_EVENT1_EN|ADC_INT_CSR_EVENT0_EN)

#define EVX_TO_INDEX(stat) \
	(((stat)&(ADC_INT_CSR_EVENT1|ADC_INT_CSR_EVENT0))>>9)

#define ADC_CLK_CTR_SRC_SHL	28
#define ADC_CLK_CTR_SRC_MASK	0xf		/* 1:16 sources */
#define ADC_CLK_CTR_MASK	0x0fffffff	/* 28 bit count */

#define ADC_SAMPLE_CTR_MASK	0x0fffffff

#define ADC_CLK_DIV_MASK	0x0000ffff

#define ADC_CONV_TIME_500	0x96
#define ADC_CONV_TIME_1000	0x36

#define ADC_HT_INT		91
#define HITIDE			2048


#define ACQ435_MODE_HIRES_512	(1<<4)
#define ACQ435_MODE_B3DIS	(1<<3)
#define ACQ435_MODE_B2DIS	(1<<2)
#define ACQ435_MODE_B1DIS	(1<<1)
#define ACQ435_MODE_B0DIS	(1<<0)

#define ACQ435_MODE_BXDIS	0xf
#define ACQ430_BANKSEL		\
	(ACQ435_MODE_B3DIS|ACQ435_MODE_B2DIS|ACQ435_MODE_B1DIS)

#define ACQ437_BANKSEL 		(ACQ435_MODE_B3DIS|ACQ435_MODE_B2DIS)
#define ACQ436_BANKSEL 		(ACQ435_MODE_B3DIS)

#define CHANNELS_PER_BANK(adev)        (IS_ACQ425(adev)? 4: 8)



#define AO420_DACSPI_CW		(1U<<31)
#define AO420_DACSPI_WC		(1U<<30)

#define FMC_DSR_LEMO_ROLE_TRG	(1<<12)
#define FMC_DSR_CLK_BUS_DX	0x00000e00
#define FMC_DSR_CLK_BUS		0x00000180
#define FMC_DSR_CLK_DIR		0x00000040
#define FMC_DSR_TRG_BUS_DX	0x00000038
#define FMC_DSR_TRG_BUS		0x00000006
#define FMC_DSR_TRG_DIR		0x00000001

#define AO_DELAY66_MASK		0x00000fff

#define ADC_ACC_DEC_LEN		0x000000ff		/* 0:x1, 1..31: x2..32 */
#define ADC_ACC_DEC_SHIFT_MASK	0x00000f00
#define ADC_ACC_DEC_START_MASK	0x00ff0000


#define ADC_ACC_DEC_PRESCALE_MASK	0x07000000		/* 0: /1  1:/2 .. 7:/128 */
/** prescale for full data coverage at SW poll rate < 200Hz
 /1    : 1*256*100 = 25kHz		// acq435 going slow
 /16   : 16*256*100 = 400kHz		// acq435, acq465, acq423 : all covered
 /32   : 32*256*122 = 1MHz              // acq424, acq425-18
 /128  : 128*256*61 = 2MHz              // acq425,
* ACQ48x does not support NACC at this time.
*/

#define ADC_MAX_NACC		256U


#define ADC_ACC_DEC_SHIFT_MAX   15U

/* AO420FMC */

#define DAC_FIFO_SAMPLES_MASK	0x0000ffff



#define DTD_CTRL_ZN		0x0000000f
#define DTD_CTRL_CLR		0x00000010


#define TDC_CR			MOD_CON
#define TDC_FIFO_COUNT		ADC_FIFO_SAMPLES
#define TDC_FIFO_STATUS		ADC_FIFO_STA

#define TDC_CH1_EVT_COUNT	(ADC_BASE+0x18)
#define TDC_CH2_EVT_COUNT	(ADC_BASE+0x1c)
#define TDC_CH3_EVT_COUNT	(ADC_BASE+0x20)
#define TDC_CH4_EVT_COUNT	(ADC_BASE+0x24)

#define TDC_CHX_EVT_COUNT(ch)	(TDC_CH1_EVT_COUNT+((ch)-1)*4)

#define TDC_TRAIN_HI_VAL	ACQ480_TRAIN_HI_VAL
#define TDC_TRAIN_LO_VAL	ACQ480_TRAIN_LO_VAL

#define TDC_LOADED_CALIB	(ADC_BASE+0x034)
#define TDC_CH_MASK		(ADC_BASE+0x044)

#define TDC_CR_PAD_EN		(1<<7)
#define TDC_CR_TRAIN		(1<<6)
#define TDC_CR_ENABLE		(1<<4)

#define TDC_CH_MASK_CH4		(1<<3)
#define TDC_CH_MASK_CH3		(1<<2)
#define TDC_CH_MASK_CH2		(1<<1)
#define TDC_CH_MASK_CH1		(1<<0)

enum DIO432_MODE { DIO432_DISABLE, DIO432_IMMEDIATE, DIO432_CLOCKED };


#define AO_CHAN	4

#define AO424_MAXCHAN		32

#define GET_MOD_ID(adev) 	 ((adev)->mod_id>>MOD_ID_TYPE_SHL)
#define GET_MOD_ID_VERSION(adev) (((adev)->mod_id>>MOD_ID_VERSION_SHL)&0xff)
#define GET_MOD_IDV(adev) 	 (((adev)->mod_id>>MOD_ID_VERSION_SHL)&0x1f)

#define GET_FPGA_REV(adev)	((adev)->mod_id&0x0000ffff)


/** WARNING: assumes CH01 bipolar, ALL bipolar */
#define SPAN_IS_BIPOLAR(adev)	((xo_dev)->ao424_device_settings.u.ch.ao424_spans[0] >= 2)

#define IS_ACQ420(adev) \
	(GET_MOD_ID(adev) == MOD_ID_ACQ420FMC || GET_MOD_ID(adev) == MOD_ID_ACQ420FMC_2000)
#define IS_ACQ424(adev)	(GET_MOD_ID(adev) == MOD_ID_ACQ424ELF)
#define IS_ACQ425(adev)	\
	(GET_MOD_ID(adev) == MOD_ID_ACQ425ELF || GET_MOD_ID(adev) == MOD_ID_ACQ425ELF_2000)

#define IS_ACQ427(adev) \
	(GET_MOD_ID(adev) == MOD_ID_ACQ427ELF ||\
	 GET_MOD_ID(adev) == MOD_ID_ACQ427ELF_2000)
#define IS_ACQ423(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ423ELF)

#define IS_ACQ465(adev)	(GET_MOD_ID(adev) == MOD_ID_ACQ465ELF)

#define IS_ACQ494(adev)	(GET_MOD_ID(adev) == MOD_ID_ACQ494FMC)

#define IS_ACQ42X(adev) _is_acq42x(adev)

#define HAS_VARIABLE_DATA32(adev) _has_variable_data32(adev)

#define IS_ACQ435(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ435ELF)
#define IS_ACQ430(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ430FMC)
#define IS_ACQ436(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ436ELF)
#define IS_ACQ437(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ437ELF)

#define IS_BOLO8(adev) \
	(GET_MOD_ID(adev)==MOD_ID_BOLO8 || GET_MOD_ID(adev)== MOD_ID_BOLO8B)

#define IS_TIMBUS(adev) (GET_MOD_ID(adev) == MOD_ID_TIMBUS)

#define IS_ACQ43X(adev)	\
	(IS_ACQ435(adev) || IS_ACQ430(adev) || IS_ACQ437(adev) || IS_ACQ436(adev) || IS_TIMBUS(adev))

#define IS_ACQ480(adev)	(GET_MOD_ID(adev) == MOD_ID_ACQ480FMC)

#define IS_ADC(adev)	(IS_ACQ43X(adev)||IS_ACQ42X(adev)||IS_ACQ480(adev)||IS_ACQ465(adev)||IS_ACQ494(adev)||IS_BOLO8(adev))

#define IS_AO420(adev)  \
	(GET_MOD_ID(adev)==MOD_ID_AO420FMC || GET_MOD_ID(adev)==MOD_ID_AO420FMC_CS2)
#define IS_AO42S(adev) (IS_AO420(adev)||IS_AO428(adev))
#define IS_AO428(adev)  (GET_MOD_ID(adev) == MOD_ID_DAC_CELF)
#define IS_AO424(adev)  (GET_MOD_ID(adev) == MOD_ID_AO424ELF)
#define IS_AO42X(adev) 	(IS_AO42S(adev) || IS_AO424(adev))

#define IS_AO420_HALF436(adev)	((GET_MOD_IDV(adev)&1) != 0)


#define IS_ACQ2006SC(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ2006SC)
#define IS_ACQ2006B(adev) \
	(IS_ACQ2006SC(adev) && GET_MOD_ID_VERSION(adev) != 0)
#define IS_ACQ2106SC(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ2106SC)
#define IS_ACQ2206SC(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ2206SC)
#define IS_ACQ2X06SC(adev) (IS_ACQ2006SC(adev) || IS_ACQ2106SC(adev) || IS_ACQ2206SC(adev))

#define IS_ACQ2106_AXI64(adev)  \
	(IS_ACQ2X06SC(adev) && (GET_MOD_ID_VERSION(adev)&0x2) != 0)
#define IS_ACQ2106_STACK(adev) \
	(IS_ACQ2X06SC(adev) && (GET_MOD_ID_VERSION(adev)&0x3) == 0x3)


#define IS_ACQ2106_STAGGER(adev) \
	(IS_ACQ2106_STACK(adev) && (GET_MOD_ID_VERSION(adev)&0x4) != 0)
#define IS_ACQ2106_WR(adev) 	((GET_MOD_ID_VERSION(adev)&0x8) != 0)
#define IS_AXI64_AGG32(adev)	((GET_MOD_ID_VERSION(adev)&0x10) != 0)
#define IS_ACQ2106_TIGA(adev) 	((GET_MOD_ID_VERSION(adev)&0x20) != 0)

#define IS_ACQ2X06SC(adev) (IS_ACQ2006SC(adev) || IS_ACQ2106SC(adev) || IS_ACQ2206SC(adev))
#define IS_ACQ1001SC(adev) (GET_MOD_ID(adev) == MOD_ID_ACQ1001SC)

#define IS_KMCU_SC(dev)		(GET_MOD_ID(adev) == MOD_ID_KMCU)
#define IS_KMCU30_SC(dev)	(GET_MOD_ID(adev) == MOD_ID_KMCU30)

#define IS_KMCx_SC(dev)		(IS_KMCU_SC(dev)||IS_KMCU30_SC(dev))

#define IS_Z7IO_SC(dev)		(GET_MOD_ID(adev) == MOD_ID_Z7IO)

#define IS_MTCA_AMC_SC(dev)	(IS_KMCx_SC(dev)||IS_Z7IO_SC(dev))

#define IS_ACQ1001_AXI64(adev) \
	(IS_ACQ1001SC(adev) && (GET_MOD_ID_VERSION(adev)&0x2) != 0)

#define IS_KMCx_AXI64(adev) \
	(IS_KMCx_SC(adev) && (GET_MOD_ID_VERSION(adev)&0x2) != 0)

#define IS_Z7IO_AXI64(adev) \
	(IS_Z7IO_SC(dev) && (GET_MOD_ID_VERSION(adev)&0x2) != 0)


#define IS_AXI64(adev) \
	(IS_ACQ2106_AXI64(adev) || IS_ACQ1001_AXI64(adev) || IS_KMCx_AXI64(adev) || IS_Z7IO_AXI64(adev))

#define IS_AXI64_DUALCHAN_CAPABLE(adev)	\
	(IS_AXI64(adev) && (GET_MOD_ID_VERSION(adev)&0x3) == 0x3)

#define IS_AXI64_DUALCHAN(adev) \
	(IS_AXI64(adev) && adev->dma_chan[0] && adev->dma_chan[1])

#define IS_SC(adev) \
	(IS_ACQ2X06SC(adev)||IS_ACQ1001SC(adev)||IS_KMCx_SC(dev)||IS_Z7IO_SC(adev))

#define IS_ACQ1014(adev) \
	(IS_ACQ1001SC(adev) && (GET_MOD_ID_VERSION(adev)&0x4) != 0)




#define IS_DIO432FMC(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO432FMC)
#define IS_DIO432PMOD(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO432PMOD)
#define IS_DIO482FMC(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO482FMC)
#define IS_DIO482ELF_PG(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO482FMC && GET_MOD_IDV(adev) == MOD_IDV_PG)
#define IS_DIO482TD_PG(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO482TD_PG)
#define IS_DIO482TD(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO482TD)
#define IS_DIO422ELF(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO422ELF && GET_MOD_IDV(adev) == MOD_IDV_DIO422_DIO)
#define IS_DIO422AQB(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO422ELF && GET_MOD_IDV(adev) == MOD_IDV_DIO422_AQB)
#define IS_DIO482PPW(adev)	(GET_MOD_ID(adev) == MOD_ID_DIO482TD && GET_MOD_IDV(adev) == MOD_IDV_PPW)

#define IS_DIO482_PG(adev)	(IS_DIO482ELF_PG(adev)||IS_DIO482TD_PG(adev))

#define IS_DIO432X(adev)	(IS_DIO432FMC(adev)||IS_DIO432PMOD(adev)||IS_DIO482FMC(adev)||IS_DIO482TD(adev)||IS_DIO422ELF(adev))

#define IS_XO(adev)		(IS_DIO432X(adev) || IS_AO42X(adev))

#define IS_PMODADC1(adev)	(GET_MOD_ID(adev) == MOD_ID_PMODADC1)

#define IS_ACQ400T(adev) \
	(GET_MOD_ID(adev) == MOD_ID_ACQ400T_FMC || GET_MOD_ID(adev) == MOD_ID_ACQ400T_ELF)

#define HAS_HDMI_SYNC(adev)	(IS_ACQ1001SC(adev)||IS_ACQ2006B(adev)||IS_ACQ2106SC(adev)||IS_ACQ2206SC(adev))
#define IS_DUMMY(adev) 	((adev)->mod_id>>MOD_ID_TYPE_SHL == MOD_ID_DUMMY)

#define IS_DIO_BISCUIT_GENERIC(adev)  (GET_MOD_ID(adev) == MOD_ID_DIO_BISCUIT)
#define IS_DIO_BISCUIT(adev)	(IS_DIO_BISCUIT_GENERIC(adev) && GET_MOD_IDV(adev) == MOD_IDV_DIO)
#define IS_V2F(adev)		(IS_DIO_BISCUIT_GENERIC(adev) && GET_MOD_IDV(adev) == MOD_IDV_V2F)
#define IS_QEN(adev)		((IS_DIO_BISCUIT_GENERIC(adev) && GET_MOD_IDV(adev)==MOD_IDV_QEN)   || IS_DIO422AQB(adev))
/* @@todo there's already IS_ACQ1014 tied to sc .. */
#define IS_ACQ1014_M(adev)	(IS_DIO_BISCUIT_GENERIC(adev) && GET_MOD_IDV(adev) == MOD_IDV_ACQ1014)

#define IS_PIG_CELF(adev)	(GET_MOD_ID(adev) == MOD_ID_PIG_CELF)
#define IS_RAD_CELF(adev)	(GET_MOD_ID(adev) == MOD_ID_RAD_CELF || GET_MOD_ID(adev) == MOD_ID_DDS_WERA)



#define HAS_AI(adev) 	(IS_ADC(adev) || IS_BOLO8(adev) || IS_PIG_CELF(adev) || IS_QEN(adev) )

#define HAS_ATD(adev)	(IS_ACQ430(adev) && (GET_MOD_ID_VERSION(adev)&0x1) != 0)
#define HAS_DTD(adev)	(IS_ACQ430(adev) && (GET_MOD_ID_VERSION(adev)&0x2) != 0)
#define HAS_XTD(adev)	(IS_ACQ430(adev) && (GET_MOD_ID_VERSION(adev)&0x3) != 0)

#define HAS_RGM(adev) 	(IS_ACQ43X(adev) || IS_ACQ42X(adev) || IS_ACQ480(adev) || IS_ACQ465(adev) || IS_AO424(adev) || IS_AO420(adev))

#define HAS_FPGA_FIR(adev) (IS_ACQ480(adev) && GET_MOD_IDV(adev) != 0)

#define HAS_FIXED_CLKDIV(adev)	(IS_ACQ480(adev)||IS_ACQ494(adev))

#define FPGA_REV(adev)	((adev)->mod_id&0x00ff)

#define SYSCLK_M100	100000000
#define SYSCLK_M66       66000000

#define ACQ2006_COUNTERS	0x100

#define ACQ2006_CLK_COUNT(n)	(ACQ2006_COUNTERS+((n)+ 0)*sizeof(u32))
#define ACQ2006_CLK_COUNT_MASK	0x0fffffff
#define ACQ2006_TRG_COUNT(n) 	(ACQ2006_COUNTERS+((n)+ 8)*sizeof(u32))
#define ACQ2006_EVT_COUNT(n) 	(ACQ2006_COUNTERS+((n)+16)*sizeof(u32))
#define ACQ2006_SYN_COUNT(n) 	(ACQ2006_COUNTERS+((n)+24)*sizeof(u32))

#define ACQ2006_TRG_COUNT_MASK	0x0000ffff
#define ACQ2006_SYN_COUNT_MASK  0x0000ffff
#define ACQ2006_EVT_COUNT_MASK	0x0000ffff

/* SC "site 0" registers */
/* #define MOD_CON			(0x0004) */
#define AGGREGATOR		(0x0008)
#define AGGSTA			(0x000c)
#define DATA_ENGINE_0		(0x0010)
#define DATA_ENGINE_1		(0x0014)
#define DATA_ENGINE_2		(0x0018)
#define AXI_DMA_ENGINE_DATA	(0x0018)	/* ALIAS */
#define DATA_ENGINE_3		(0x001c)
#define DATA_ENGINE(e)		(0x0010+((e)*4))
#define GPG_CTRL		(0x0020)  /* per telecon with John 201402061145 */
#define GPG_CONTROL		GPG_CTRL
#define HDMI_SYNC_DAT		(0x0024)
#define HDMI_SYNC_OUT_SRC	(0x0028)   	/* HSO_xx	*/
#define EVT_BUS_SRC		(0x002c)	/* EBS_xx	*/
#define SIG_SRC_ROUTE		(0x0030)	/* SSR_xx	*/
#define FPCTL			(0x0034)
#define SYS_CLK			(0x0038)
#define AGG_DEBUG		(0x003c)
#define DISTRIBUTOR		(0x0040)
#define DISTRIBUTOR_STA		(0x0044)
#define GPG_DEBUG		(0x0048)
#define USEC_CCR		(0x004c)
#define SPI_PERIPHERAL_CS	(0x0050)
#define DIST_DBG		(0x0054)

#define AXI_DMA_DEBUG_0		(0x0060)
#define AXI_DMA_DEBUG_1		(0x0064)
#define AXI_DMA_DEBUG_2		(0x0068)
#define AXI_DMA_DEBUG_3		(0x006C)

#define WR_CTRL			(0x0200)
#define WR_CLK_GEN		(0x0204)
#define WR_TAI_CUR_L		(0x0208)
#define WR_TAI_CUR_H		(0x020C)
#define WR_TAI_TRG0		(0x0210)
#define WR_TAI_STAMP		(0x0214)
#define WR_CUR_VERNR		(0x0218)
#define WR_TAI_TRG1		(0x021C)



#define WR_CTRL_TT1_STA		(1<<11)
#define WR_CTRL_TT1_INTEN	(1<<10)
#define WR_CTRL_PPS_STA		(1<<9)
#define WR_CTRL_PPS_INTEN	(1<<8)
#define WR_CTRL_TT0_STA		(1<<7)
#define WR_CTRL_TT0_INTEN	(1<<6)
#define WR_CTRL_TS_STA		(1<<5)
#define WR_CTRL_TS_INTEN	(1<<4)
#define WR_CTRL_TRG_SRC_SHL	0

#define WR_CLK_GEN_PV		(0x1f000000)
#define WR_CLK_GEN_PV3		(0x07000000)

#define WR_TAI_TRG_EN		(1<<31)


#define WR_TAI_CUR_H_LINKUP	(1<<31)
#define WR_TAI_CUR_H_TIMEVALID  (1<<30)

#define WR_TS_S1		(0x0240)
#define WR_TS_S2		(0x0244)
#define WR_TS_S3		(0x0248)
#define WR_TS_S4		(0x024C)
#define WR_TS_S5		(0x0250)
#define WR_TS_S6		(0x0254)
#define WR_TS_S(site)		(WR_TS_S1+((site)-1)*4)

#define WR_TT_S1		(0x0260)
#define WR_TT_S2		(0x0264)
#define WR_TT_S3		(0x0268)
#define WR_TT_S4		(0x026C)
#define WR_TT_S5		(0x0270)
#define WR_TT_S6		(0x0274)
#define WR_TT_S(site)		(WR_TT_S1+((site)-1)*4)

#define WR_TIGA_CSR		(0x0280)

#define WR_TIGA_CSR_TS_MASK	0x3F			/* 6 sites */
#define WR_TIGA_REGCOUNT	6

#define WR_TIGA_CSR_TS_ST_SHADOW (1<<15)
#define WR_TIGA_CSR_TT_ST_SHL	24
#define WR_TIGA_CSR_TT_EN_SHL	16
#define WR_TIGA_CSR_TS_ST_SHL	 8
#define WR_TIGA_CSR_TS_EN_SHL	 0


#define DE_AXI_DMA_FAIL		(1<<16)
#define DE_SELECT_AGG		(1<<14)			/* PRI only */
#define DE_MAXDESCRIPTORS	(0x0ff0)			/* limit the number of descriptors in run */
#define DE_FORCE_SINGLE_DMA	(1<<1)			/* when we have 2 channels but only want to use one, eg MGTDRAM offload */
#define DE_ENABLE		(1<<0)


#define AGG_SNAP_DI_SHL		30
#define AGG_SNAP_DI_MASK	0x3
#define AGG_SNAP_DI_OFF		0
#define AGG_SNAP_DI_4		2
#define AGG_SNAP_DI_32		3

#define AGG_DECIM_MASK		0xf
#define AGG_DECIM_SHL		24

#define AGG_SPAD_EN		(1<<17)
#define AGG_SPAD_FRAME		(1<<16)
#define AGG_SPAD_LEN_SHL	4

#define AGG_SPAD_LEN		16
#define AGG_SPAD_LEN_MASK	(AGG_SPAD_LEN-1)

#define AGG_SPAD_ALL_MASK \
	(AGG_SPAD_LEN_MASK<<AGG_SPAD_LEN_SHL|\
	  AGG_SPAD_FRAME|AGG_SPAD_EN| AGG_SNAP_DI_MASK<<AGG_SNAP_DI_SHL)

#define SET_AGG_SPAD_LEN(len)	((((len)-1)&AGG_SPAD_LEN_MASK)<<AGG_SPAD_LEN_SHL)
#define SET_AGG_SNAP_DIx(dix)	(((dix)&AGG_SNAP_DI_MASK)<<AGG_SNAP_DI_SHL)

#define AGG_FIFO_RESET		(1<<1)
#define AGG_ENABLE		(1<<0)

/* s=1..6 */
#define AGG_MOD_EN(s, shift)	(1 << ((s)-1+(shift)))
#define AGGREGATOR_MSHIFT	18
#define DATA_ENGINE_MSHIFT	 8
#define AGGREGATOR_ENABLE(s)	AGG_MOD_EN(s, AGGREGATOR_MSHIFT)
#define DATA_ENGINE_ENABLE(s)	AGG_MOD_EN(s,  DATA_ENGINE_MSHIFT)

#define AGG_SITES_MASK		0x3f
#define DATA_MOVER_EN		0x1

#define AGG_SIZE_MASK		0xff
#define AGG_SIZE_SHL		8
#define AGG_SIZE_UNIT_OLD	512
#define AGG_SIZE_UNIT_NEW	128

#define AGG_SIZE_UNIT(adev)	\
	(IS_ACQ2006SC(adev)&&FPGA_REV(adev)<=8? \
		AGG_SIZE_UNIT_OLD: AGG_SIZE_UNIT_NEW)


#define DIST_COMMS_MASK		(0x3<<28)
#define DIST_COMMS_A		(0x1<<28)
#define DIST_COMMS_B		(0x2<<28)
#define DIST_MSHIFT		AGGREGATOR_MSHIFT
#define DIST_MOD_EN(s)		AGG_MOD_EN(s, DIST_MSHIFT)

#define DIST_SPAD_EN		AGG_SPAD_EN
#define DIST_SIZE_MASK		0xff
#define DIST_SIZE_SHL		8
#define DIST_SPAD_LEN_SHL	AGG_SPAD_LEN_SHL
#define DIST_SPAD_LEN_MASK	AGG_SPAD_LEN_MASK
#define DIST_INT_STA		(1<<3)		/* WC */
#define DIST_INT_EN		(1<<2)
#define DIST_FIFO_RESET		(1<<1)
#define DIST_ENABLEN		(1<<0)

#define DIST_TRASH_LEN_MASK	0xf

#define SET_DIST_SIZE(n)	((((n)/128)&DIST_SIZE_MASK)<<DIST_SIZE_SHL)
#define GET_DIST_SIZE(dist)	((((dist)>>DIST_SIZE_SHL)&DIST_SIZE_MASK)*128)



#define AGGSTA_FIFO_COUNT	0x000000ff
#define AGGSTA_FIFO_ANYSKIP	0x00000800
#define AGGSTA_FIFO_EMPTY	0x00000100
#define AGGSTA_FIFO_STAT	0x00000f00
#define AGGSTA_BACKPRESSURE	0x00008000
#define AGGSTA_ENGINE_STAT	0x000f0000

#define DISSTA_FIFO_ANYSKIP	AGGSTA_FIFO_ANYSKIP

#define AXI_DMA_ENGINE_DATA_MAX_BLOCKS	65536
#define AXI_DMA_BLOCK	0x800
#define AXI_DMA_ENGINE_DATA_BS		0x800

#define MCR_MOD_EN			(1<<0)
#define MCR_PSU_SYNC 			(1<<1)
#define MCR_FAN_EN			(1<<2)
#define ACQ1001_MCR_CELF_PSU_EN		(1<<3)
#define MCR_SOFT_TRIG			(1<<4)
#define ACQ1001_MCR_PWM_BIT		8
#define ACQ1001_MCR_PWM_MAX		0x40
#define ACQ1001_MCR_PWM_MASK		((ACQ1001_MCR_PWM_MAX-1)<<ACQ1001_MCR_PWM_BIT)
#define ACQ1001_MCR_PWM_MIN		0x04
#define MCR_ZCLK_SELECT_SHL		16
#define MCR_ZCLK_MASK			0x0f
#define MCR_COUNTER_LATCH		(1<<20)

#define GPG_CTRL_TOPADDR_SHL		20
#define GPG_CTRL_TRG_SHL		16
#define GPG_CTRL_SYNC_SHL		12
#define GPG_CTRL_CLK_SHL		8
#define GPG_CTRL_MODE_SHL		1

#define GPG_CTRL_TOPADDR		0x1ff00000
#define GPG_CTRL_TRG_SEL		0x000e0000
#define GPG_CTRL_TRG_RISING		0x00010000
#define GPG_CTRL_SYNC_SEL		0x0000e000
#define GPG_CTRL_SYNC_RISING		0x00001000
#define GPG_CTRL_CLK_SEL		0x00000e00
#define GPG_CTRL_CLK_RISING		0x00000100
#define GPG_CTRL_EXT_TRG		0x00000040
#define GPG_CTRL_EXT_SYNC		0x00000020
#define GPG_CTRL_EXTCLK			0x00000010
#define GPG_CTRL_MODE			0x00000006
#define GPG_CTRL_ENABLE			0x00000001

#define GPG_DBG_STATE	0xe0000000
#define GPG_DBG_ADDR	0x1ff00000
#define GPG_DBG_CTR	0x000fffff


#define GPG_MEM_BASE			0xf000
#define GPG_MEM_SIZE			0x1000
#define GPG_MEM_ACTUAL			0x0800

/* HDMI_SYNC_DAT bits */
#define HDMI_SYNC_IN_SYNCb		15
#define HDMI_SYNC_IN_TRGb		14
#define HDMI_SYNC_IN_GPIOb		13
#define HDMI_SYNC_IN_CLKb		12
#define HDMI_SYNC_OUT_CABLE_DETNb	 8
#define HDMI_SYNC_OUT_SYNCb		 3
#define HDMI_SYNC_OUT_TRGb		 2
#define HDMI_SYNC_OUT_GPIOb		 1
#define HDMI_SYNC_OUT_CLKb		 0

#define HDMI_SYNC_IN_SHL		12
#define HDMI_SYNC_OUT_SHL		0
#define HDMI_SYNC_MASK			0x0f

/* HDMI_SYNC_OUT_SRC */
#define HSO_SYNC_SHL			24
#define HSO_TRG_SHL			16
#define HSO_GPIO_SHL			 8
#define HSO_CLK_SHL			 0

#define HSO_SS_SEL_SHL			3
#define HSO_XX_SEL_MASK			(0x3)
#define HSO_DO_SEL			(0x0)
#define HSO_SIG_BUS_SEL			(0x2)
#define HSO_GPG_BUS_SEL			(0x3)
#define HSO_DX_MASK			0x7


/* USEC_CTR_CTRL */

#define USEC_CCR_SRC_SHL	4
#define USEC_CCR_SRC_MASK	0x7
#define USEC_CCR_CLK_SRC_DX	0x6
#define USEC_CCR_EN		0x1


/* EVENT BUS SOURCE */
#define EBS_MASK	0xf
#define EBS_TRG		0x0
#define EBS_GPG		0x1
#define EBS_HDMI	0x2
#define EBS_CELF	0x3

#define EBS_7_SHL	28
#define EBS_6_SHL	24
#define EBS_5_SHL	20
#define EBS_4_SHL	16
#define EBS_3_SHL	12
#define EBS_2_SHL	 8
#define EBS_1_SHL	 4
#define EBS_0_SHL	 0

/* SIG SRC ROUTE */
#define SSR_MASK	0xf
#define SSR_EXT		0x0
#define SSR_HDMI	0x1
#define SSR_CELF	0x2

#define SSR_SYNC_1_SHL	20
#define SSR_SYNC_0_SHL	16
#define SSR_TRG_1_SHL	12
#define SSR_TRG_0_SHL	 8
#define SSR_CLK_1_SHL	 4
#define SSR_CLK_0_SHL	 0

#define EXT_DX	0			/* External clock */
#define MB_DX	1 			/* Motherboard clock */
#define SITE2DX(site) 	((site)+1)

#define FPCTL_FP_1014_TRG (1<<13)
#define FPCTL_FP_1014_CLK (1<<12)

#define FPCTL_GPIO_SHL	8
#define FPCTL_SYNC_SHL 	4
#define FPCTL_TRG_SHL	0

#define FPCTL_MASK	0xf
#define FPCTL_IS_INPUT	0x0

/* AO424 */

#define DAC_424_CGEN_ODD_CHANS		(1<<4)
#define DAC_424_CGEN_DISABLE_D		(1<<3)
#define DAC_424_CGEN_DISABLE_C		(1<<2)
#define DAC_424_CGEN_DISABLE_B		(1<<1)
#define DAC_424_CGEN_DISABLE_A		(1<<0)

#define DAC_424_CGEN_DISABLE_X		(0xf)

/* ACQ423 */
#define ACQ423_BANK_D37_MODE		(1<<5)
#define ACQ423_BANK_ODD_CHAN		(1<<4)

#define ACQ423_SPAN_MASK		(0xf)
#define ACQ423_SPAN_FW			4	/* Field Width    */
#define ACQ423_SPAN_FPR			8	/* Fields Per Reg */
#define ACQ423_SPAN_MASK_ODDS		(0xf0f0f0f0f0)

/* DIO432 */

#define DIO432_MOD_ID		0x00
#define DIO432_CTRL		MCR
#define DIO432_TIM_CTRL		0x08
#define DIO432_DI_HITIDE	0x0C
#define DIO432_DI_FIFO_COUNT	0x10
#define DIO432_DI_FIFO_STATUS	0x14
#define DIO432_DIO_ICR		0x18	/* same as regular ICR */

#define DIO_CLKDIV		ADC_CLKDIV
#define DIO432_DIO_CPLD_CTRL	0x44
#define DIO422_OE_CONFIG	0x44
#define DIO432_DIO_SAMPLE_COUNT 0x48
#define DIO432_DI_SNOOP		0x4c
#define DIO432_DEBUG		0x50
#define DIO432_CMD_DEBUG	0x54

#define DIO432_DO_LOTIDE	0x8c
#define DIO432_DO_FIFO_COUNT	0x90
#define DIO432_DO_FIFO_STATUS	0x94

#define DIO482_PG_FPTRG_COUNT	0x20
#define DIO482_PG_WDT		0x28

#define DIO482_PG_GPGCR		0x80
#define DIO482_PG_GPGDR		0x84
#define DIO482_PG_IMM_MASK	0x88

#define DIO482_PG_GPGMEM	0x4000
#define DIO482_PG_GPGMEM32	0x8000   // 32 bit state extension.
#define DIO432_FIFO		0x1000

#define DIO482_PG_IMM_DO	0x1000


#define DIO482_PG_DO5 0x1f	/* 5 bits if DIO432_CTRL_PG_CLK_IS_DO is set */
#define DIO482_PG_DO4 0x0f	/* 4 bits */
#define DIO482_PG_PG4 0x80
#define DIO482_PG_PG_DOx	(DIO482_PG_DO4|DIO482_PG_PG4|DIO482_PG_DO5)

#define DIO482_PG_WDT_MASK	0x000000ff

#define DIO482_PPW_PPW_DOx	(0x3f)

#define DIO432_CTRL_SHIFT_DIV_SHL (9)
#define DIO432_CTRL_AWG_ABORT	(1<<12)

#define DIO432_CTRL_BYPASS_TRG   (1<<15)
#define DIO432_CTRL_INVERT	(1<<14)
#define DIO432_CTRL_PG_CLK_IS_DO (1<<11)
#define DIO432_CTRL_CHAIN_PG 	(1 << 10)
#define DIO432_CTRL_LL		(1 << 8)
#define DIO432_CTRL_EXT_CLK_SYNC (1<< 7)
#define DIO432_CTRL_RAMP_EN 	(1 << 5)	/* Deprecated, sadly. Use SPAD */
#define DIO432_CTRL_DIO_EN	(1 << 4)
#define DIO432_CTRL_DIO_RST	(1 << 3)
#define DIO432_CTRL_FIFO_EN	(1 << 2)
#define DIO432_CTRL_FIFO_RST	(1 << 1)
#define DIO432_CTRL_MODULE_EN	(1 << 0)	/* enable at enumeration, leave up */

#define DIO432_CTRL_RST	(DIO432_CTRL_DIO_RST|DIO432_CTRL_FIFO_RST)

#define DIO432_CTRL_SHIFT_DIV_FMC	(0<<DIO432_CTRL_SHIFT_DIV_SHL)
#define DIO432_CTRL_SHIFT_DIV_PMOD	(1<<DIO432_CTRL_SHIFT_DIV_SHL)

#define PMODADC1_CTRL_EXT_CLK_FROM_SYNC (1<<7)
#define PMODADC1_CTRL_DIV		(3<<DIO432_CTRL_SHIFT_DIV_SHL)


#define DIO432_FIFSTA_FULL	(1<<3)
#define DIO432_FIFSTA_EMPTY	(1<<2)
#define DIO432_FIFSTA_OVER	(1<<1)
#define DIO432_FIFSTA_UNDER	(1<<0)

#define DIO432_FIFSTA_CLR	0xf

#define DIO432_CPLD_CTRL_COMMAND_COMPLETE	(1<<9)
#define DIO432_CPLD_CTRL_COMMAND_WRITE		(1<<8)
#define DIO432_CPLD_CTRL_COMMAND_DATA		0xff

#define DIO432_CPLD_CTRL_OUTPUT(byte)		(1<<(byte))


#define DIO422_OE_CONFIG_CONFIGX	(7<<4)
#define DIO422_OE_CONFIG_Tx_ENn		(1<<1)
#define DIO422_OE_CONFIG_Tx_EN		(1<<0)

/* DIO BISCUIT */
#define DIOUSB_CTRL		0x04
#define DIOUSB_STAT		0x14

/* bus select */
#define DIOUSB_CTRL_BUS_MASK	(0x7<<6)		/* 000: CLK, TRG, EVT, SYNC, 100 : DSP */

/* V2F */

#define V2F_MOD_ID		0x00
#define V2F_CTRL		0x04
#define V2F_STAT		0x14
#define V2F_CHAN_SEL		0x60
#define V2F_FREQ_OFF		0x64		/* 1,2,3,4 */

#define V2F_FREQ_SLO		0x74		/* 1,2,3,4 */

#define V2F_CTRL_DATA_PACKED	(1<<7)
#define V2F_CTRL_RANGE_HI	(1<<6)
#define V2F_CTRL_EN		(1<<4)
#define V2F_CTRL_RST		(1<<3)
#define V2F_CTRL_MODULE_EN	(1<<0)

#define V2F_CHAN_SEL_ENC(v, c)  (c << (8*((v)-1)))
#define V2F_CHAN_SEL_DEC(cs, v) (((cs)>>(8*((v)-1)))&0x0ff)

#define V2F_FREQ_OFF_MAX	((1<<21) - 1)
#ifdef PGMCOMOUT
/* @@todo .. which is it? */
#define V2F_FREQ_OFF_MAX 	((1<<22)-1)
#endif
#define V2F_FREQ_SLO_MAX	((1<<27) - 1)

#define ACQ480_FIRCO_CSR_BIST	(1<<31)
#define ACQ480_FIRCO_CSR_RESET	(1<<24)
#define ACQ480_FIRCO_CSR_CTR	(0x00ff0000)
#define ACQ480_FIRCO_CSR_CTR_SHL 16

#define ACQ480_ADC_MR_MRSEL1	0x00007000
#define ACQ480_ADC_MR_MRSEL0	0x00000700
#define ACQ480_ADC_MR_MR10_DEC	0x00000030
#define ACQ480_ADC_MR_EN	0x00000001

#define AO424_DAC_CTRL_SPAN	(1<<5)
#define AO424_DAC_FIFO_STA_SWC	(1<<8)

#define AO424_DAC_CTRL_SNOOPSEL_SHL	16
#define AO424_DAC_CTRL_SNOOPSEL_MSK	0x0f


#define ACQ400T_DOA	0x10
#define ACQ400T_DOB	0x14
#define ACQ400T_DIA	0x20
#define ACQ400T_DIB	0x24

#define ACQ400T_SCR_PWR_GOOD_BIT	5
#define ACQ400T_SCR_JTAG_RESET_BIT	4
#define ACQ400T_SCR_TEST_DATA_DONE_BIT	2
#define ACQ400T_SCR_SEND_START_BIT	1

/* PIGCELF PC */
#define PIG_CTL			0x04

#define PC_DDS_DAC_CLKDIV	0x40
#define PC_ADC_CLKDIV		0x44
#define PC_DDS_PHASE_INC	0x48

#define PIG_CTL_IMU_RST		(1<<3)
#define PIG_CTL_PSU_EN		0x00008000
#define PIG_CTL_MASTER		0x00000100

/* RADCELF */
#define RAD_CTL			0x04
#define RAD_DDS_A		0x10
#define RAD_DDS_B		0x14
#define RAD_DDS_AB		0x18	/* UPDATE ONLY */
#define RAD_DDS_C		0x20
#define RAD_CLK_PPS_LATCH	0x58

#define DDS_GPS_SYNC_CHIRP	(1<<15)
#define DDS_GPS_ENGAGE_HOLD	(1<<14)
#define DDS_GPS_ARM_PPS		(1<<13)
#define RAD_CTL_CLKD_RESET	(1<<4)
#define RAD_CTL_DDS_RESET	(1<<3)

#define RAD_DDS_UPD_CLK_FPGA	(1<<9)
#define RAD_DDS_UPD_CLK		(1<<8)
#define RAD_DDS_CLK_OEn		(1<<4)
#define RAD_DDS_OSK		(1<<1)
#define RAD_DDS_BPSK		(1<<0)

/* ACQ1014: DIO BISCUIT in MASTER */
#define DIO1014_CR		(0x04)
#define DIO1014_SR		(0x14)	/* @@todo pending */
#define DIO1014_SR_RP_CONN	(1<<0)	/* @@todo pending */

#define DIO1014_CR_CLK		(1<<8)
#define DIO1014_CR_CLK_LO	DIO1014_CR_CLK
#define DIO1014_CR_TRG		(1<<4)
#define DIO1014_CR_TRG_SOFT	DIO1014_CR_TRG
#define DIO1014_MOD_EN		(1<<0)

/* QEN : Quadrature ENcoder */
#define	QEN_CTRL		0x04

#define QEN_CTRL_EN		(1<<4)
#define QEN_CTRL_RESET		(1<<3)
#define QEN_CTRL_FIFO_EN	ADC_CTRL_FIFO_EN
#define QEN_CTRL_FIFO_RST	ADC_CTRL_FIFO_RST
#define QEN_CTRL_MODULE_EN	ADC_CTRL_MODULE_EN

#define QEN_FIFO_SAMPLES	(ADC_BASE+0x10)
#define QEN_FIFO_STA		(ADC_BASE+0x14)
#define QEN_INT_CSR		(ADC_BASE+0x18)
#define QEN_CLK_CTR		(ADC_BASE+0x1C)
#define QEN_SAMPLE_CTR		(ADC_BASE+0x20)
#define QEN_SAMPLE_CLK_CTR	(ADC_BASE+0x24)

#define QEN_CLKDIV		(ADC_BASE+0x40)
#define QEN_TRANSLEN		(ADC_BASE+0x50)
#define QEN_INDEX_HOME		(ADC_BASE+0x54)


#define QEN_DIO_CTRL		(ADC_BASE+0x5c)
#define QEN_ENC_COUNT		(ADC_BASE+0x60)
#define QEN_ZCOUNT		(ADC_BASE+0x64)
#define QEN_DI_MON		(ADC_BASE+0x68)
#define QEN_ECOUNT		(ADC_BASE+0x6c)
#define QEN_POS_ABS_TRG		(ADC_BASE+0x70)
#define QEN_POS_PRD_TRG		(ADC_BASE+0x74)
#define QEN_POS_PRD_HYST	(ADC_BASE+0x78)
#define QEN_POS_PRD_CNT		(ADC_BASE+0x7c)

#define QEN_DIO_CTRL_PRD_TRG_EN (1<<16)
#define QEN_DIO_CTRL_ABS_TRG_EN (1<<15)
#define QEN_DIO_CTRL_SNAP32	(1<<14)
#define QEN_DIO_CTRL_MSBDIRECT	(1<<13)		// Count MSB is DI2, DI4 respectively
#define QEN_DIO_CTRL_ZCOUNT	(1<<12)		// Include Z input as a separate counter
#define QEN_DIO_CTRL_CTR_RESET	(1<<11)
#define QEN_DIO_CTRL_ZSEL	(3<<10)
#define QEN_DIO_CTRL_PB_EN	(1<<9)
#define QEN_DIO_CTRL_PA_EN	(1<<8)

#define QEN_DIO_CTRL_DIR_OUT	0x00f0
#define QEN_DIO_CTRL_DO_IMM	0x000f

#define HALF_SITE		100		/* MFD, half sites at 100+ overlay 0+ */

/* PWM */
#define PWM_SOURCE_CLK_CTRL_DIV_SHL	16
#define PWM_SOURCE_CLK_CTRL_EN		(1<<4)
#define PWM_SOURCE_CLK_CTRL_SHL		0

/* PPW */
#define PPW_CHANNEL(n)		(0xa0+(0x10*(n-1)))
#define PPW_TRG(n)		(PPW_CHANNEL(n)+0x0)
//#define PPW_PWM(n)		(PPW_CHANNEL(n)+0x4)
#define PPW_REP(n)		(PPW_CHANNEL(n)+0x8)
#define PPW_STA(n)		(PPW_CHANNEL(n)+0xc)

#define _PPW_PWM_CH(n)        (0x100+(n-1)*0x10)
#define PPW_PWM_GP(n)        (_PPW_PWM_CH(n)+0x0)
#define PPW_PWM_IC(n)        (_PPW_PWM_CH(n)+0x4)
#define PPW_PWM_OC(n)        (_PPW_PWM_CH(n)+0x8)

#define PPW_MIN	1
#define PPW_MAX 6

#define PPW_TRG_BUS		0x03U
#define PPW_TRG_BIT   		0x70U
#define PPW_TRG_RISING		0x80U

#define PPW_PWM_ICOUNT		0x7fffffffU
#define PPW_PWM_IS		0x80000000U
#define PPW_PWM_OCOUNT		0x7fffffffU
#define PPW_PWM_PRD		0xffffffffU

#define PPW_REP_FIELD		0x0000ffffU
#define PPW_REP_SINGLE		0
#define PPW_REP_FREE		0x0000ffff


#include "acq400_structs.h"

#endif /* ACQ420FMC_H_ */
