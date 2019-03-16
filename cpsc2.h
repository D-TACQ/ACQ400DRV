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
#define IS_CSPC2_COMMS(adev)		(GET_MOD_ID(adev) == MOD_ID_CPSC2_COMMS)
#define IS_CPSC2_DAC(adev)		(GET_MOD_ID(adev) == MOD_ID_CPSC2_DAC)


#define CPSC2_DAC_SFP1_PKT_CNT		(ADC_BASE+0x60)
#define CPSC2_DAC_SFP1_SKIP_PKT_CNT	(ADC_BASE+0x64)
#define CPSC2_DAC_SFP2_PKT_CNT		(ADC_BASE+0x68)
#define CPSC2_DAC_SFP2_SKIP_PKT_CNT	(ADC_BASE+0x6C)
#define CPSC2_DAC_PKT_TX		(ADC_BASE+0x70)
#define CPSC2_DAC_PKT_SEQ		(ADC_BASE+0x74)
#define CPSC2_DAC_PROC_STA		(ADC_BASE+0x78)
#define CPSC2_DAC_MATH_STA		(ADC_BASE+0x7C)
#define CPSC2_DAC_SLOW_SET_CH(n)	(ADC_BASE+0x80+4*((n)-1))

struct acq400_dev;

extern void cpsc2_dac_createDebugfs(struct acq400_dev* adev, char* pcursor);
#endif /* CPSC2_H_ */
