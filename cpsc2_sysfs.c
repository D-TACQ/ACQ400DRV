/*
 * cpsc2_sysfs.c
 *
 *  Created on: 16 Mar 2019
 *      Author: pgm
 */

#include <linux/ctype.h>
#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */
#include "lk-shim.h"
#include <linux/device.h>
#include <linux/user.h>

#include "cpsc2.h"
#include "acq400.h"
#include "acq400_sysfs.h"

#include "sysfs_attrs.h"

MAKE_BITS(rfd_mcm2_stopped, 	CPSC2_SC_RFD, MAKE_BITS_FROM_MASK, CPSC2_SC_RFD_MCM2_STOPPED);
MAKE_BITS(rfd_mcm2_pll_locked, 	CPSC2_SC_RFD, MAKE_BITS_FROM_MASK, CPSC2_SC_RFD_MCM2_PLL_LOCKED);
MAKE_BITS(rfd_mcm1_stopped, 	CPSC2_SC_RFD, MAKE_BITS_FROM_MASK, CPSC2_SC_RFD_MCM1_STOPPED);
MAKE_BITS(rfd_mcm1_pll_locked, 	CPSC2_SC_RFD, MAKE_BITS_FROM_MASK, CPSC2_SC_RFD_MCM1_PLL_LOCKED);
MAKE_BITS(rfd_div, 		CPSC2_SC_RFD, MAKE_BITS_FROM_MASK, CPSC2_SC_RFD_DIV);

const struct attribute *cpsc2_sc_attrs[] = {
		&dev_attr_rfd_mcm2_stopped.attr,
		&dev_attr_rfd_mcm2_pll_locked.attr,
		&dev_attr_rfd_mcm1_stopped.attr,
		&dev_attr_rfd_mcm1_pll_locked.attr,
		&dev_attr_rfd_div.attr,
		NULL
};

SCOUNT_KNOB(SFP1_PKT_CNT, 	CPSC2_DAC_SFP1_PKT_CNT);
SCOUNT_KNOB(SFP1_SKIP_PKT_CNT, 	CPSC2_DAC_SFP1_SKIP_PKT_CNT);
SCOUNT_KNOB(SFP2_PKT_CNT, 	CPSC2_DAC_SFP2_PKT_CNT);
SCOUNT_KNOB(SFP2_SKIP_PKT_CNT, 	CPSC2_DAC_SFP2_SKIP_PKT_CNT);

MAKE_BITS(sfp_mode,       DAC_CTRL, MAKE_BITS_FROM_MASK, CPSC2_DAC_CTRL_SFP_MODE);
MAKE_BITS(dac_b_wf_n_sfp, DAC_CTRL, MAKE_BITS_FROM_MASK, CPSC2_DACB_WFnSFP);
MAKE_BITS(dac_a_wf_n_sfp, DAC_CTRL, MAKE_BITS_FROM_MASK, CPSC2_DACA_WFnSFP);
MAKE_BITS(pkt_tx_err,     CPSC2_DAC_PKT_TX, MAKE_BITS_FROM_MASK, CPSC2_DAC_PKT_TX_ERR);
MAKE_BITS(pkt_tx_en,      CPSC2_DAC_PKT_TX, MAKE_BITS_FROM_MASK, CPSC2_DAC_PKT_TX_EN);
MAKE_BITS(pkt_seq,        CPSC2_DAC_PKT_SEQ, MAKE_BITS_FROM_MASK, ~0);

MAKE_BITS(proc_a_ie,  CPSC2_DAC_PROC_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_A_IE);
MAKE_BITS(proc_a_sta, CPSC2_DAC_PROC_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_A_ST);
MAKE_BITS(proc_b_ie,  CPSC2_DAC_PROC_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_B_IE);
MAKE_BITS(proc_b_sta, CPSC2_DAC_PROC_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_B_ST);

MAKE_BITS(math_a_ie,  CPSC2_DAC_MATH_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_A_IE);
MAKE_BITS(math_a_sta, CPSC2_DAC_MATH_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_A_ST);
MAKE_BITS(math_b_ie,  CPSC2_DAC_MATH_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_B_IE);
MAKE_BITS(math_b_sta, CPSC2_DAC_MATH_ICST, MAKE_BITS_FROM_MASK, CPSC2_DAC_MATH_B_ST);

const struct attribute *cpsc2_dac_attrs[] = {
	&dev_attr_scount_SFP1_PKT_CNT.attr,
	&dev_attr_scount_SFP1_SKIP_PKT_CNT.attr,
	&dev_attr_scount_SFP2_PKT_CNT.attr,
	&dev_attr_scount_SFP2_SKIP_PKT_CNT.attr,

	&dev_attr_sfp_mode.attr,
	&dev_attr_dac_b_wf_n_sfp.attr,
	&dev_attr_dac_a_wf_n_sfp.attr,

	&dev_attr_pkt_tx_err.attr,
	&dev_attr_pkt_tx_en.attr,
	&dev_attr_pkt_seq.attr,

	&dev_attr_proc_a_sta.attr,
	&dev_attr_proc_b_sta.attr,
	&dev_attr_proc_a_ie.attr,
	&dev_attr_proc_b_ie.attr,

	&dev_attr_math_a_sta.attr,
	&dev_attr_math_b_sta.attr,
	&dev_attr_math_a_ie.attr,
	&dev_attr_math_b_ie.attr,
	NULL
};

MAKE_BITS(sfp2_tx_en, CPSC2_COM_CONTROL, MAKE_BITS_FROM_MASK, CPSC2_COM_CONTROL_SFP2_TX_EN);
MAKE_BITS(sfp2_rx_en, CPSC2_COM_CONTROL, MAKE_BITS_FROM_MASK, CPSC2_COM_CONTROL_SFP2_RX_EN);
MAKE_BITS(sfp1_tx_en, CPSC2_COM_CONTROL, MAKE_BITS_FROM_MASK, CPSC2_COM_CONTROL_SFP1_TX_EN);
MAKE_BITS(sfp1_rx_en, CPSC2_COM_CONTROL, MAKE_BITS_FROM_MASK, CPSC2_COM_CONTROL_SFP1_RX_EN);

SCOUNT_KNOB(heartbeat, CPSC2_COM_HEARTBEAT);

MAKE_BITS(aurora1_en, 	CPSC2_COM_SFP_AURORA_CR(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_EN);
MAKE_BITS(aurora1_rst, 	CPSC2_COM_SFP_AURORA_CR(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_RST);
MAKE_BITS(aurora1_clrf, CPSC2_COM_SFP_AURORA_CR(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_CLRF);
MAKE_BITS(aurora1_pwrdn,CPSC2_COM_SFP_AURORA_CR(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_PWRDN);
MAKE_BITS(aurora1_lpbk, CPSC2_COM_SFP_AURORA_CR(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_LPBK);

MAKE_BITS(aurora2_en, 	CPSC2_COM_SFP_AURORA_CR(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_EN);
MAKE_BITS(aurora2_rst, 	CPSC2_COM_SFP_AURORA_CR(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_RST);
MAKE_BITS(aurora2_clrf, CPSC2_COM_SFP_AURORA_CR(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_CLRF);
MAKE_BITS(aurora2_pwrdn,CPSC2_COM_SFP_AURORA_CR(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_PWRDN);
MAKE_BITS(aurora2_lpbk, CPSC2_COM_SFP_AURORA_CR(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_CR_LPBK);

SCOUNT_KNOB_FIELD(aurora1_rx_pkts, CPSC2_COM_SFP_AURORA_STA(1), CPSC2_COM_AURORA_STA_CNT);
MAKE_BITS(aurora1_flags,   CPSC2_COM_SFP_AURORA_STA(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_STA_FLG);
MAKE_BITS(aurora1_lane_up, CPSC2_COM_SFP_AURORA_STA(1), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_STA_LANEUP);

SCOUNT_KNOB_FIELD(aurora2_rx_pkts, CPSC2_COM_SFP_AURORA_STA(2), CPSC2_COM_AURORA_STA_CNT);
MAKE_BITS(aurora2_flags,   CPSC2_COM_SFP_AURORA_STA(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_STA_FLG);
MAKE_BITS(aurora2_lane_up, CPSC2_COM_SFP_AURORA_STA(2), MAKE_BITS_FROM_MASK, CPSC2_COM_AURORA_STA_LANEUP);


SCOUNT_KNOB_FIELD(aurora1_lane_up,   CPSC2_COM_SFP_AURORA_STA1(1), CPSC2_COM_AURORA_STA1_LANE_UP);
SCOUNT_KNOB_FIELD(aurora1_soft_err,  CPSC2_COM_SFP_AURORA_STA1(1), CPSC2_COM_AURORA_STA1_ERR_SOFT);
SCOUNT_KNOB_FIELD(aurora1_hard_err,  CPSC2_COM_SFP_AURORA_STA1(1), CPSC2_COM_AURORA_STA1_ERR_HARD);
SCOUNT_KNOB_FIELD(aurora1_frame_err, CPSC2_COM_SFP_AURORA_STA1(1), CPSC2_COM_AURORA_STA1_ERR_FRAM);
SCOUNT_KNOB_FIELD(aurora1_rx_len,    CPSC2_COM_SFP_AURORA_STA2(1), CPSC2_COM_AURORA_STA2_RPL);

SCOUNT_KNOB_FIELD(aurora2_lane_up,   CPSC2_COM_SFP_AURORA_STA1(2), CPSC2_COM_AURORA_STA1_LANE_UP);
SCOUNT_KNOB_FIELD(aurora2_soft_err,  CPSC2_COM_SFP_AURORA_STA1(2), CPSC2_COM_AURORA_STA1_ERR_SOFT);
SCOUNT_KNOB_FIELD(aurora2_hard_err,  CPSC2_COM_SFP_AURORA_STA1(2), CPSC2_COM_AURORA_STA1_ERR_HARD);
SCOUNT_KNOB_FIELD(aurora2_frame_err, CPSC2_COM_SFP_AURORA_STA1(2), CPSC2_COM_AURORA_STA1_ERR_FRAM);
SCOUNT_KNOB_FIELD(aurora2_rx_len,    CPSC2_COM_SFP_AURORA_STA2(2), CPSC2_COM_AURORA_STA2_RPL);


const struct attribute *cpsc2_com_attrs[] = {
	&dev_attr_sfp2_tx_en.attr,
	&dev_attr_sfp2_rx_en.attr,
	&dev_attr_sfp1_tx_en.attr,
	&dev_attr_sfp1_rx_en.attr,
	&dev_attr_scount_heartbeat.attr,

	&dev_attr_aurora1_en.attr,
	&dev_attr_aurora1_rst.attr,
	&dev_attr_aurora1_clrf.attr,
	&dev_attr_aurora1_pwrdn.attr,
	&dev_attr_aurora1_lpbk.attr,

	&dev_attr_aurora2_en.attr,
	&dev_attr_aurora2_rst.attr,
	&dev_attr_aurora2_clrf.attr,
	&dev_attr_aurora2_pwrdn.attr,
	&dev_attr_aurora2_lpbk.attr,

	&dev_attr_scount_aurora1_rx_pkts.attr,
	&dev_attr_aurora1_flags.attr,
	&dev_attr_aurora1_lane_up.attr,

	&dev_attr_scount_aurora2_rx_pkts.attr,
	&dev_attr_aurora2_flags.attr,
	&dev_attr_aurora2_lane_up.attr,

	&dev_attr_scount_aurora1_lane_up.attr,
	&dev_attr_scount_aurora1_soft_err.attr,
	&dev_attr_scount_aurora1_hard_err.attr,
	&dev_attr_scount_aurora1_frame_err.attr,
	&dev_attr_scount_aurora1_rx_len.attr,

	&dev_attr_scount_aurora2_lane_up.attr,
	&dev_attr_scount_aurora2_soft_err.attr,
	&dev_attr_scount_aurora2_hard_err.attr,
	&dev_attr_scount_aurora2_frame_err.attr,
	&dev_attr_scount_aurora2_rx_len.attr,
	NULL
};

