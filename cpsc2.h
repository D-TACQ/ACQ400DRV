/*
 * cpsc2.h
 *
 *  Created on: 15 Mar 2019
 *      Author: pgm
 */

#ifndef CPSC2_H_
#define CPSC2_H_

#define MOD_ID_CPSC2		0x85
#define MOD_ID_CPSC2_COMMS	0x96
#define MOD_ID_CPSC2_DAC	0x43


#define IS_CPSC2_SC(adev)		(GET_MOD_ID(adev) == MOD_ID_CPSC2)
#define IS_CSPC2_COMMS(adev)	(GET_MOD_ID(adev) == MOD_ID_CPSC2_COMMS)
#define IS_CPSC2_DAC(adev)		(GET_MOD_ID(adev) == MOD_ID_CPSC2_DAC)

#define CPSC2_SC_RFD			(ADC_BASE+0x70)

#define CPSC2_SC_RFD_MCM2_STOPPED		(1<<11)
#define CPSC2_SC_RFD_MCM2_PLL_LOCKED	(1<<10)
#define CPSC2_SC_RFD_MCM1_STOPPED		(1<<11)
#define CPSC2_SC_RFD_MCM1_PLL_LOCKED	(1<<10)
#define CPSC2_SC_RFD_DIV			0x000000ff

/* additional DAC REGS */

#define CPSC2_DAC_SFP1_PKT_CNT		(ADC_BASE+0x60)
#define CPSC2_DAC_SFP1_SKIP_PKT_CNT	(ADC_BASE+0x64)
#define CPSC2_DAC_SFP2_PKT_CNT		(ADC_BASE+0x68)
#define CPSC2_DAC_SFP2_SKIP_PKT_CNT	(ADC_BASE+0x6C)
#define CPSC2_DAC_PKT_TX			(ADC_BASE+0x70)
#define CPSC2_DAC_PKT_SEQ			(ADC_BASE+0x74)
#define CPSC2_DAC_PROC_ICST			(ADC_BASE+0x78)
#define CPSC2_DAC_MATH_ICST			(ADC_BASE+0x7C)
#define CPSC2_DAC_SLOW_SET_CH(n)	(ADC_BASE+0x80+4*((n)-1))
#define CPSC2_DAC_RTM_LENGTH		(ADC_BASE+0xa0)

/* DAC bitfields */
#define CPSC2_DAC_RTM_MODE		(0x1<<31)
#define CPSC2_DACB_WF_CLEAR		(0x1<<23)
#define CPSC2_DACB_WF_CLEAR		(0x1<<23)
#define CPSC2_DACB_PKT_CLEAR	(0x1<<22)
#define CPSC2_DACA_WF_CLEAR		(0x1<<21)
#define CPSC2_DACA_PKT_CLEAR	(0x1<<20)
#define CPSC2_DAC_CTRL_SFP_MODE	(0x3<<18)
#define CPSC2_DACB_WFnSFP		(0x1<<17)
#define CPSC2_DACA_WFnSFP		(0x1<<16)


#define CPSC2_DAC_PKT_TX_ERR		(1<<16)
#define CPSC2_DAC_PKT_TX_EN		(1<<0)

#define CPSC2_DAC_MATH_A_IE		(0xff<<0)
#define CPSC2_DAC_MATH_A_ST		(0xff<<8)
#define CPSC2_DAC_MATH_B_IE		(0xff<<16)
#define CPSC2_DAC_MATH_B_ST		(0xff<<24)

/* COM REGS */

#define CPSC2_COM_CONTROL		(ADC_BASE+0x04)
#define CPSC2_COM_HEARTBEAT		(ADC_BASE+0x08)
#define CPSC2_COM_SFP_AURORA_CR(n)	(ADC_BASE+0x10+((n)-1)*0x10)
#define CPSC2_COM_SFP_AURORA_STA(n)	(ADC_BASE+0x14+((n)-1)*0x10)
#define CPSC2_COM_SFP_AURORA_STA1(n)	(ADC_BASE+0x18+((n)-1)*0x10)
#define CPSC2_COM_SFP_AURORA_STA2(n)	(ADC_BASE+0x1C+((n)-1)*0x10)

#define CPSC2_COM_LAST_PKT(n, f)	(ADC_BASE + (n)*0x100+ 4*(f))

/* COM Bitfields */

#define CPSC2_COM_CONTROL_SFP2_TX_EN	(1<<9)
#define CPSC2_COM_CONTROL_SFP2_RX_EN	(1<<8)
#define CPSC2_COM_CONTROL_SFP1_TX_EN	(1<<5)
#define CPSC2_COM_CONTROL_SFP1_RX_EN	(1<<4)

#define CPSC2_COM_AURORA_CR_EN		(1<<31)
#define CPSC2_COM_AURORA_CR_RST		(1<<15)
#define CPSC2_COM_AURORA_CR_CLRF	(1<<7)
#define CPSC2_COM_AURORA_CR_PWRDN	(1<<4)
#define CPSC2_COM_AURORA_CR_LPBK	(0x7)

#define CPSC2_COM_AURORA_STA_CNT	(0xffff0000)
#define CPSC2_COM_AURORA_STA_FLG	(0x0000ffff)
#define CPSC2_COM_AURORA_STA_LANEUP	(1<<0)

#define CPSC2_COM_AURORA_STA1_LANE_UP   (0xff<<24)
#define CPSC2_COM_AURORA_STA1_ERR_SOFT  (0xff<<16)
#define CPSC2_COM_AURORA_STA1_ERR_HARD  (0xff<< 8)
#define CPSC2_COM_AURORA_STA1_ERR_FRAM  (0xff<< 0)

#define CPSC2_COM_AURORA_STA2_RPL	(0xf << 0)


struct acq400_dev;

extern void cpsc2_dac_createDebugfs(struct acq400_dev* adev, char* pcursor);
extern void cpsc2_com_createDebugfs(struct acq400_dev* adev, char* pcursor);
#endif /* CPSC2_H_ */
