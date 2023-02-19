/*
 * acq400_mod_id.h
 *
 *  Created on: 23 Sep 2021
 *      Author: pgm
 */

#ifndef ACQ400_MOD_ID_H_
#define ACQ400_MOD_ID_H_

/* AI/ADC MODULES */
#define MOD_ID_ACQ420FMC	0x01
#define MOD_ID_ACQ435ELF	0x02
#define MOD_ID_ACQ430FMC	0x03
#define MOD_ID_ACQ424ELF	0x04
#define MOD_ID_ACQ425ELF	0x05
#define MOD_ID_ACQ437ELF	0x06
#define MOD_ID_ACQ427ELF	0x07
#define MOD_ID_ACQ480FMC	0x08		/* ACQ480, ACQ481, ACQ482 */
#define MOD_ID_ACQ423ELF	0x09
#define MOD_ID_ACQ465ELF	0x0a
#define MOD_ID_ACQ494FMC	0x0b		/* TDC : it's more like an ADC than anything else */

#define MOD_ID_ACQ420FMC_2000	0xa1
#define MOD_ID_ACQ425ELF_2000	0xa5
#define MOD_ID_ACQ427ELF_2000   0xa7


/* AO/DAC MODULES */
#define MOD_ID_AO420FMC		0x40
#define MOD_ID_AO424ELF		0x41
#define MOD_ID_AO420FMC_CS2	0x42

/* Multifunction MODULES */
#define MOD_ID_BOLO8		0x60
#define MOD_ID_DIO432FMC	0x61
#define MOD_ID_DIO432PMOD	0x62

#define MOD_ID_PMODADC1		0x63
#define MOD_ID_BOLO8B		0x64
#define MOD_ID_PMODGPS_CELF	0x65
#define MOD_ID_PMODGPS_FMC	0x66

#define MOD_ID_DIO_BISCUIT	0x67
#define MOD_ID_PIG_CELF		0x68
#define MOD_ID_RAD_CELF		0x69
#define MOD_ID_DAC_CELF		0x6a

#define MOD_ID_DIO482FMC	0x6b

#define MOD_ID_ACQ436ELF	0x6d
#define MOD_ID_TIMBUS		0x6e

#define MOD_ID_DDS_WERA		0x70
#define MOD_ID_DIO422ELF	0x71

#define MOD_ID_DIO482TD		0x7a	// Vanilla DIO version
#define MOD_ID_DIO482TD_PG	0x7b

#define MOD_ID_ACQ2006SC	0x80
#define MOD_ID_ACQ1001SC	0x81
#define MOD_ID_ACQ2106SC	0x82
#define MOD_ID_KMCU		0x83
#define MOD_ID_KMCU30		0x84
#define MOD_ID_CPSC2		0x85
#define MOD_ID_Z7IO		0x86
#define MOD_ID_ACQ2206SC	0x87

#define MOD_ID_MTCA_ADAP	0xfc
#define MOD_ID_ACQ400T_FMC	0xfd
#define MOD_ID_ACQ400T_ELF	0xfe
#define MOD_ID_DUMMY		0x00ff


#define MOD_ID_HUDP		0x97

/* ACQ480 variants */
#define MOD_ID_TYPE_ACQ480DIV4	0x1
#define MOD_ID_TYPE_ACQ480DIV10 0x2

/* known DIO432 variants */
#define MOD_IDV_PWM		0x01
#define MOD_IDV_PWM2		0x02	/* "SLOW PWM, with external CLOCK REG */
#define MOD_IDV_PG		0x0f	/* DIO482-PG */

/* known DIO422 variants */
#define MOD_IDV_DIO422_DIO	0x00
#define MOD_IDV_DIO422_AQB	0x01	/* AquadB input */

/* known Biscuit Variants, switch on MOD_ID_VERSION */
#define MOD_IDV_V2F		0x0
#define MOD_IDV_DIO		0x1
#define MOD_IDV_QEN		0x2
#define MOD_IDV_ACQ1014		0x14

/* known DIO482TD Variants, switch on MOD_ID_VERSION */
#define MOD_IDV_PPW		0x0e





#endif /* ACQ400_MOD_ID_H_ */
