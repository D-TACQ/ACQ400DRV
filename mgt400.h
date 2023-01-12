/*
 * mgt400.h
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 */

#ifndef MGT400_H_
#define MGT400_H_

#define DESC_HISTOLEN	128
#define DATA_HISTOLEN	128
#define DESC_HMASK  	(DESC_HISTOLEN-1)
#define DATA_HMASK	(DATA_HISTOLEN-1)

struct mgt400_dev {
	struct of_prams {
		int site;
		int irq;
		int sn;
		int phys;	/* 0: SFP, 1: PCIe */
	} of_prams;
	char devname[16];
	u32 mod_id;

	struct platform_device *pdev;
	struct resource *mem;
	void *va;
	struct cdev cdev;
	char* debug_names;
	struct dentry* debug_dir;
	int RW32_debug;

	struct DMA_CHANNEL {
		unsigned long buffer_count;
		unsigned long desc_histo[DESC_HISTOLEN];
		unsigned long data_histo[DATA_HISTOLEN];
		unsigned long first_descriptors[DESC_HISTOLEN];
		unsigned last_packet_id;
		unsigned previous_count;
		int fd_ix;
	} push, pull;
	struct RegCache clk_reg_cache;
	struct hrtimer buffer_counter_timer;
	struct StatusClient {
		unsigned status;
		wait_queue_head_t status_change;
	} dma_enable_status[2];			/* PULL, PUSH */
};

#define dev_virtaddr	va
#define PDBUF_WORDS	512

struct mgt400_path_descriptor {
	struct mgt400_dev* dev;
	int minor;
	unsigned buffer[PDBUF_WORDS];
};

#undef PD
#undef PDSZ
#define PD(filp)	((struct mgt400_path_descriptor*)filp->private_data)
#define SETPD(filp, value)	(filp->private_data = (value))
#define PDSZ		(sizeof (struct mgt400_path_descriptor))
#define DEVP(adev)		(&(adev)->pdev->dev)

extern struct mgt400_dev* mgt400_devices[];

void mgt400wr32(struct mgt400_dev *adev, int offset, u32 value);
u32 mgt400rd32(struct mgt400_dev *adev, int offset);
void mgt400_createDebugfs(struct mgt400_dev* adev);
void mgt400_createSysfs(struct device *dev);
void mgt400_createDebugfs(struct mgt400_dev* adev);
void mgt400_removeDebugfs(struct mgt400_dev* adev);
void mgt400_clear_counters(struct mgt400_dev* mdev);

int mgt400_clear_histo(struct mgt400_dev *mdev, int minor);

/* ZYNQ : RW    HOST: RO (at start only) */
#undef MOD_ID
#define MOD_ID		(0x0000)
#define ZDMA_CR		(0x0004)
#define HEART		(0x0008)
#define AURORA_CR	(0x000C)
#define AURORA_SR	(0x0010)
#define ZIDENT		(0x0014)
#define COMMS_TXB_FCR	(0x0020)
#define COMMS_TXB_FSR	(0x0024)
#define COMMS_RXB_FCR	(0x0040)
#define COMMS_RXB_FSR   (0x0044)
#define ASTATS1		(0x0050)
#define ASTATS2		(0x0054)
#define ALAT_AVG	(0x0058)
#define ALAT_MIN_MAX	(0x005c)
#define MGT_DRAM_STA	(0x0080)
#define MGT_DRAM_RX_CNT	(0x0084)
#define MGT_DRAM_TX_CNT (0x0088)


#define ZDMA_CR_KILL_COMMS      (1<<3)
#define ZDMA_CR_AUTO_PUSH_DMA	(1<<1)
#define ZDMA_CR_ENABLE		(1<<0)

#define AURORA_CR_ENA		(1<<31)
#define AURORA_CR_CLR		(1<<7)
#define AURORA_CR_PWR_DWN	(1<<4)
#define AURORA_CR_LOOPBACK	(0x7)

#define AURORA_SR_HARD_ERR	(1<<6)
#define AURORA_SR_SOFT_ERR	(1<<5)
#define AURORA_SR_FRAME_ERR	(1<<4)
#define AURORA_SR_CHANNEL_UP	(1<<1)
#define AURORA_SR_LANE_UP	(1<<0)

#define AURORA_SR_ERR_XX \
	(AURORA_SR_HARD_ERR|AURORA_SR_SOFT_ERR|AURORA_SR_FRAME_ERR)

#define AURORA_SR_UP_MASK 	(AURORA_SR_CHANNEL_UP|AURORA_SR_LANE_UP)

#define AURORA_SR_ERR(xx) (((xx)&AURORA_SR_ERR_XX)>>4)
#define AURORA_SR_UP(xx)  (((xx)&AURORA_SR_UP_MASK)>>0)

/** PCIE REGS : ZYNQ:RO HOST: RW */
#define PCIE_CTRL 	(0x1004)
#define PCIE_INTR	(0x1008)
#define PCI_CSR		(0x100C)
#define PCIE_DEV_CSR	(0x1010)
#define PCIE_LINK_CSR	(0x1014)
#define PCIE_CONF	(0x1018)
#define PCIE_BUF_CTRL	(0x101C)

/** DMA REGS : ZYNQ:RO HOST:RW */
#define DMA_TEST		(0x2000)
#define DMA_CTRL		(0x2004)
#define DMA_FIFO_SR		(0x2008)
#define DESC_FIFO_SR		(0x200C)
#define DMA_PUSH_DESC_SR	(0x2010)
#define DMA_PULL_DESC_SR	(0x2014)
#define DMA_PUSH_COUNT_LW	(0x2018)
#define DMA_PULL_COUNT_LW	(0x201C) /* speculative */
#define DMA_PUSH_DESC_LEN	(0x2020)
#define DMA_PULL_DESC_LEN	(0x2024)
#define DMA_PUSH_DESC_FIFO	(0x2040)
#define DMA_PULL_DESC_FIFO	(0x2080)


#define ID_PUSH				0
#define ID_PULL				1
#define DMA_DATA_PULL_SHL		16
#define DMA_DATA_PUSH_SHL		0

#define DMA_CTRL_EN	0x0001
#define DMA_CTRL_RST	0x0010

#define DMA_DATA_FIFO_COUNT		0xfff0
#define DMA_DATA_FIFO_COUNT_SHL		4
#define DMA_DATA_FIFO_FLAGS		0x000f

#define GET_DMA_DATA_FIFO_COUNT(sta) \
	(((sta)&DMA_DATA_FIFO_COUNT)>>DMA_DATA_FIFO_COUNT_SHL)

#define DESCR_ADDR	0xffffffc00
#define DESCR_INTEN	0x000000100
#define DESCR_LEN	0x0000000f0
#define DESCR_ID	0x00000000f

#define MINOR_PUSH_DATA_HISTO	0
#define MINOR_PUSH_DESC_HISTO 	1
#define MINOR_PULL_DATA_HISTO	2
#define MINOR_PULL_DESC_HISTO	3
#define MINOR_PUSH_DESC_LIST	4
#define MINOR_PULL_DESC_LIST	5
#define MINOR_PUSH_DESC_FIFO	6
#define MINOR_PULL_DESC_FIFO	7
#define MINOR_PUSH_STATUS	8
#define MINOR_PULL_STATUS	9
#define MGT_MINOR_COUNT	       10

#define PD_FIFO_SHL(file)	\
    ( PD(file)->minor==MINOR_PUSH_DESC_FIFO? \
	DMA_DATA_PUSH_SHL: DMA_DATA_PULL_SHL)

#define PD_FIFO_OFFSET(file) \
(PD(file)->minor==MINOR_PUSH_DESC_FIFO? DMA_PUSH_DESC_FIFO: DMA_PULL_DESC_FIFO)

#define MOD_ID_MGT_DRAM		0x95

#define IS_MGT_DRAM(mdev)	(GET_MOD_ID(mdev) == MOD_ID_MGT_DRAM)
#define IS_MGT_HUDP(mdev)	(GET_MOD_ID(mdev) == MOD_ID_HUDP)

#define HUDP_CON		0x0004
#define HUDP_IP_ADDR		0x0008
#define HUDP_GW_ADDR		0x000c
#define HUDP_NETMASK		0x0010
#define HUDP_MAC		0x0014		/* 00:21:ww:xx:yy:zz  ww should be 54 */
#define HUDP_SRC_PORT		0x0018
#define HUDP_TX_PKT_SZ		0x001c
#define HUDP_RX_PORT		0x0020
#define HUDP_RX_SRC_ADDR	0x0024

#define HUDP_TX_PKT_COUNT	0x0030
#define HUDP_RX_PKT_COUNT	0x0034
#define HUDP_DISCO_COUNT	0x0038
#define HUDP_RX_PKT_LEN		0x003c     	/* R/O detected packet lenght */
#define HUDP_STATUS		0x0040
#define HUDP_CALC_PKT_SZ	0x0044
#define ARP_RESP_MAC_UPPER	0x0048
#define ARP_RESP_MAC_LOWER	0x004c
#define UDP_SLICE		0x0050

#define HUDP_DEST_ADDR		0x0108
#define HUDP_DEST_PORT		0x0118

#define HUDP_DISCO_EN		0x80000000
#define HUDP_DISCO_INDEX	0x7ff00000
#define HUDP_DISCO_COUNT_COUNT	0x000fffff



void mgt400wr32(struct mgt400_dev *mdev, int offset, u32 value);
u32 mgt400rd32(struct mgt400_dev *mdev, int offset);

#endif /* MGT400_H_ */
