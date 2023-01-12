/*
 * xilinx_axidma.h
 *
 *  Created on: 10 Nov 2015
 *      Author: pgm
 */

#ifndef XILINX_AXIDMA_H_
#define XILINX_AXIDMA_H_

/* Hw specific definitions */
#define XILINX_DMA_MAX_CHANS_PER_DEVICE	0x2 /* Max no of channels */
#define XILINX_DMA_MAX_TRANS_LEN	0x7FFFFF /* Max transfer length */

/* Hardware descriptor */
struct xilinx_dma_desc_hw {
	u32 next_desc;	/* 0x00 */
	u32 pad1;	/* 0x04 */
	u32 buf_addr;	/* 0x08 */
	u32 pad2;	/* 0x0C */
	u32 pad3;	/* 0x10 */
	u32 pad4;	/* 0x14 */
	u32 control;	/* 0x18 */
	u32 status;	/* 0x1C */
	u32 app_0;	/* 0x20 */
	u32 app_1;	/* 0x24 */
	u32 app_2;	/* 0x28 */
	u32 app_3;	/* 0x2C */
	u32 app_4;	/* 0x30 */
} __aligned(64);

/* Software descriptor */
struct xilinx_dma_desc_sw {
	struct xilinx_dma_desc_hw hw;
	struct list_head node;
	struct list_head tx_list;
	struct dma_async_tx_descriptor async_tx;
} __aligned(64);

#define MAXTRACE	(1<<8)

/* Per DMA specific operations should be embedded in the channel structure */
struct xilinx_dma_chan {
	void __iomem *regs;		/* Control status registers */
	dma_cookie_t completed_cookie;	/* The maximum cookie completed */
	dma_cookie_t cookie;		/* The current cookie */
	spinlock_t lock;		/* Descriptor operation lock */
	bool sg_waiting;		/* Scatter gather transfer waiting */
	struct list_head active_list;	/* Active descriptors */
	struct list_head pending_list;	/* Descriptors waiting */
	struct dma_chan common;		/* DMA common channel */
	struct dma_pool *desc_pool;	/* Descriptors pool */
	struct device *dev;		/* The dma device */
	int irq;			/* Channel IRQ */
	int id;				/* Channel ID */
	enum dma_transfer_direction direction;
					/* Transfer direction */
	int max_len;			/* Maximum data len per transfer */
	bool has_sg;			/* Support scatter transfers */
	bool has_dre;			/* Support unaligned transfers */
	int err;			/* Channel has errors */
	struct tasklet_struct tasklet;	/* Cleanup work after irq */
	u32 feature;			/* IP feature */
	u32 private;			/* Match info for channel request */
	void (*start_transfer)(struct xilinx_dma_chan *chan);
	struct xilinx_dma_config config;
					/* Device configuration info */
	struct dentry* debug_dir;
	char* debug_names;
	char devname[32];
	struct DescriptorTrace {
		u32 buffer[MAXTRACE];
		int cursor;
	} dTrace;

	void* client_private;
};

/* DMA Device Structure */
struct xilinx_dma_device {
	void __iomem *regs;
	struct device *dev;
	struct dma_device common;
	struct xilinx_dma_chan *chan[XILINX_DMA_MAX_CHANS_PER_DEVICE];
	u32 feature;
};

#define to_xilinx_chan(chan) \
	container_of(chan, struct xilinx_dma_chan, common)

/* IO accessors */
static inline void dma_write(struct xilinx_dma_chan *chan, u32 reg, u32 val)
{
	dev_dbg(chan->dev, "dma_write() %08x = %08x", reg, val);
	writel(val, chan->regs + reg);
}

static inline u32 dma_read(struct xilinx_dma_chan *chan, u32 reg)
{
	u32 rc = readl(chan->regs + reg);
	dev_dbg(chan->dev, "dma_read() %08x : %08x", reg, rc);
	return rc;
}

/* Register Offsets */
#define XILINX_DMA_CONTROL_OFFSET	0x00 /* Control Reg */
#define XILINX_DMA_STATUS_OFFSET	0x04 /* Status Reg */
#define XILINX_DMA_CDESC_OFFSET		0x08 /* Current descriptor Reg */
#define XILINX_DMA_TDESC_OFFSET		0x10 /* Tail descriptor Reg */
#define XILINX_DMA_SRCADDR_OFFSET	0x18 /* Source Address Reg */
#define XILINX_DMA_DSTADDR_OFFSET	0x20 /* Dest Address Reg */
#define XILINX_DMA_BTT_OFFSET		0x28 /* Bytes to transfer Reg */

/* General register bits definitions */
#define XILINX_DMA_CR_RESET_MASK	0x00000004 /* Reset DMA engine */
#define XILINX_DMA_CR_RUNSTOP_MASK	0x00000001 /* Start/stop DMA engine */

#define XILINX_DMA_SR_HALTED_MASK	0x00000001 /* DMA channel halted */
#define XILINX_DMA_SR_IDLE_MASK		0x00000002 /* DMA channel idle */

#define XILINX_DMA_XR_IRQ_IOC_MASK	0x00001000 /* Completion interrupt */
#define XILINX_DMA_XR_IRQ_DELAY_MASK	0x00002000 /* Delay interrupt */
#define XILINX_DMA_XR_IRQ_ERROR_MASK	0x00004000 /* Error interrupt */
#define XILINX_DMA_XR_IRQ_ALL_MASK	0x00007000 /* All interrupts */

#define XILINX_DMA_XR_DELAY_MASK	0xFF000000 /* Delay timeout counter */
#define XILINX_DMA_XR_COALESCE_MASK	0x00FF0000 /* Coalesce counter */

#define XILINX_DMA_DELAY_SHIFT		24 /* Delay timeout counter shift */
#define XILINX_DMA_COALESCE_SHIFT	16 /* Coalesce counter shift */

#define XILINX_DMA_DELAY_MAX		0xFF /* Maximum delay counter value */
#define XILINX_DMA_COALESCE_MAX		0xFF /* Max coalescing counter value */

#define XILINX_DMA_RX_CHANNEL_OFFSET	0x30 /* S2MM Channel Offset */

/* BD definitions for AXI Dma */
#define XILINX_DMA_BD_STS_ALL_MASK	0xF0000000
#define XILINX_DMA_BD_SOP		0x08000000 /* Start of packet bit */
#define XILINX_DMA_BD_EOP		0x04000000 /* End of packet bit */

/* Feature encodings */
#define XILINX_DMA_FTR_HAS_SG		0x00000100 /* Has SG */
#define XILINX_DMA_FTR_HAS_SG_SHIFT	8 /* Has SG shift */
/* Optional feature for dma */
#define XILINX_DMA_FTR_STSCNTRL_STRM	0x00010000


/* Delay loop counter to prevent hardware failure */
#define XILINX_DMA_RESET_LOOP		1000000
#define XILINX_DMA_HALT_LOOP		1000000

extern int xilinx_dma_reset(struct xilinx_dma_chan *chan);

#endif /* XILINX_AXIDMA_H_ */
