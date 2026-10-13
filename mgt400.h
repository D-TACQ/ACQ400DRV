/*
 * mgt400.h
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 */

#ifndef MGT400_H_
#define MGT400_H_

#include "mgt400_includes.h"

/* ZYNQ : RW    HOST: RO (at start only) */
#undef MOD_ID
#define MOD_ID (0x0000)
#define ZDMA_CR (0x0004)
#define HEART (0x0008)
#define AURORA_CR (0x000C)
#define AURORA_SR (0x0010)
#define ZIDENT (0x0014)
#define MGT508_LEN (0x001C)
#define COMMS_TXB_FCR (0x0020)
#define COMMS_TXB_FSR (0x0024)
#define COMMS_RXB_FCR (0x0040)
#define COMMS_RXB_FSR (0x0044)
#define ASTATS1 (0x0050)
#define ASTATS2 (0x0054)
#define ALAT_AVG (0x0058)
#define ALAT_MIN_MAX (0x005c)
#define MGT_DRAM_STA (0x0080)
#define MGT_DRAM_RX_CNT (0x0084)
#define MGT_DRAM_TX_CNT (0x0088)

#define ZDMA_CR_COOKED_DATA_ON (1 << 16)
#define ZDMA_CR_RAW_DATA_ON (1 << 15)

#define ZDMA_CR_KILL_COMMS (1 << 3)
#define ZDMA_CR_AUTO_PUSH_DMA (1 << 1)
#define ZDMA_CR_ENABLE (1 << 0)

#define AURORA_CR_ENA (1 << 31)
#define AURORA_GT_RESET (1 << 15)
#define AURORA_CR_CLR (1 << 7)
#define AURORA_CR_PWR_DWN (1 << 4)
#define AURORA_CR_LOOPBACK (0x7)

#define AURORA_SR_HARD_ERR (1 << 6)
#define AURORA_SR_SOFT_ERR (1 << 5)
#define AURORA_SR_FRAME_ERR (1 << 4)
#define AURORA_SR_CHANNEL_UP (1 << 1)
#define AURORA_SR_LANE_UP (1 << 0)

#define AURORA_SR_ERR_XX \
	(AURORA_SR_HARD_ERR | AURORA_SR_SOFT_ERR | AURORA_SR_FRAME_ERR)

#define AURORA_SR_UP_MASK (AURORA_SR_CHANNEL_UP | AURORA_SR_LANE_UP)

#define AURORA_SR_ERR(xx) (((xx) & AURORA_SR_ERR_XX) >> 4)
#define AURORA_SR_UP(xx) (((xx) & AURORA_SR_UP_MASK) >> 0)

/** PCIE REGS : ZYNQ:RO HOST: RW */
#define PCIE_CTRL (0x1004)
#define PCIE_INTR (0x1008)
#define PCI_CSR (0x100C)
#define PCIE_DEV_CSR (0x1010)
#define PCIE_LINK_CSR (0x1014)
#define PCIE_CONF (0x1018)
#define PCIE_BUF_CTRL (0x101C)

/** DMA REGS : ZYNQ:RO HOST:RW */
#define DMA_TEST (0x2000)
#define DMA_CTRL (0x2004)
#define DMA_FIFO_SR (0x2008)
#define DESC_FIFO_SR (0x200C)
#define DMA_PUSH_DESC_SR (0x2010)
#define DMA_PULL_DESC_SR (0x2014)
#define DMA_PUSH_COUNT_LW (0x2018)
#define DMA_PULL_COUNT_LW (0x201C) /* speculative */
#define DMA_PUSH_DESC_LEN (0x2020)
#define DMA_PULL_DESC_LEN (0x2024)
#define DMA_PUSH_DESC_FIFO (0x2040)
#define DMA_PULL_DESC_FIFO (0x2080)

#define ID_PUSH 0
#define ID_PULL 1
#define DMA_DATA_PULL_SHL 16
#define DMA_DATA_PUSH_SHL 0

#define DMA_CTRL_EN 0x0001
#define DMA_CTRL_RST 0x0010

#define DMA_DATA_FIFO_COUNT 0xfff0
#define DMA_DATA_FIFO_COUNT_SHL 4
#define DMA_DATA_FIFO_FLAGS 0x000f

#define GET_DMA_DATA_FIFO_COUNT(sta) \
	(((sta) & DMA_DATA_FIFO_COUNT) >> DMA_DATA_FIFO_COUNT_SHL)

#define DESCR_ADDR 0xffffffc00
#define DESCR_INTEN 0x000000100
#define DESCR_LEN 0x0000000f0
#define DESCR_ID 0x00000000f

#define MOD_ID_MGT508 0x91 /* shared with KMCU */
#define MOD_ID_MGT_DRAM 0x95

#define IS_MGT_DRAM(mdev) (GET_MOD_ID(mdev) == MOD_ID_MGT_DRAM)
#define IS_MGT_HUDP(mdev) (GET_MOD_ID(mdev) == MOD_ID_HUDP)

extern int host_is_acq2206;
#define IS_MGT508(mdev) (host_is_acq2206 && GET_MOD_ID(mdev) == MOD_ID_MGT508)

#define HUDP_CON 0x0004
#define HUDP_IP_ADDR 0x0008
#define HUDP_GW_ADDR 0x000c
#define HUDP_NETMASK 0x0010
#define HUDP_MAC 0x0014 /* 00:21:ww:xx:yy:zz  ww should be 54 */
#define HUDP_SRC_PORT 0x0018
#define HUDP_TX_PKT_SZ 0x001c
#define HUDP_RX_PORT 0x0020
#define HUDP_RX_SRC_ADDR 0x0024

#define HUDP_TX_PKT_COUNT 0x0030
#define HUDP_RX_PKT_COUNT 0x0034
#define HUDP_DISCO_COUNT 0x0038
#define HUDP_RX_PKT_LEN 0x003c /* R/O detected packet lenght */
#define HUDP_STATUS 0x0040
#define HUDP_CALC_PKT_SZ 0x0044
#define ARP_RESP_MAC_UPPER 0x0048
#define ARP_RESP_MAC_LOWER 0x004c
#define UDP_SLICE 0x0050
#define UDP_HEARTBEAT 0x0080

#define HUDP_DEST_ADDR 0x0108
#define HUDP_DEST_PORT 0x0118

#define HUDP_DISCO_EN 0x80000000
#define HUDP_DISCO_INDEX 0x7ff00000
#define HUDP_DISCO_COUNT_COUNT 0x000fffff

#endif /* MGT400_H_ */
