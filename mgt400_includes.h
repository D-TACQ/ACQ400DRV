/*
 * mgt400.h
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 */

#ifndef MGT400_INCLUDES_H_
#define MGT400_INCLUDES_H_

#define DESC_HISTOLEN 128
#define DATA_HISTOLEN 128
#define DESC_HMASK (DESC_HISTOLEN - 1)
#define DATA_HMASK (DATA_HISTOLEN - 1)

struct mgt400_dev {
	struct of_prams {
		int site;
		int irq;
		int sn;
		int phys; /* 0: SFP, 1: PCIe */
	} of_prams;
	char devname[16];
	u32 mod_id;

	struct platform_device *pdev;
	struct resource *mem;
	void *va;
	struct cdev cdev;
	char *debug_names;
	struct dentry *debug_dir;
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
	} dma_enable_status[2]; /* PULL, PUSH */
};

#define dev_virtaddr va
#define PDBUF_WORDS 512

struct mgt400_path_descriptor {
	struct mgt400_dev *dev;
	int minor;
	unsigned buffer[PDBUF_WORDS];
};

#undef PD
#undef PDSZ
#define PD(filp) ((struct mgt400_path_descriptor *)filp->private_data)
#define SETPD(filp, value) (filp->private_data = (value))
#define PDSZ (sizeof(struct mgt400_path_descriptor))
#define DEVP(adev) (&(adev)->pdev->dev)

extern struct mgt400_dev *mgt400_devices[];

void mgt400wr32(struct mgt400_dev *adev, int offset, u32 value);
u32 mgt400rd32(struct mgt400_dev *adev, int offset);
void mgt400_createDebugfs(struct mgt400_dev *adev);
void mgt400_createSysfs(struct mgt400_dev *mdev);
void mgt400_createDebugfs(struct mgt400_dev *adev);
void mgt400_removeDebugfs(struct mgt400_dev *adev);
void mgt400_clear_counters(struct mgt400_dev *mdev);

int mgt400_clear_histo(struct mgt400_dev *mdev, int minor);

#define MINOR_PUSH_DATA_HISTO 0
#define MINOR_PUSH_DESC_HISTO 1
#define MINOR_PULL_DATA_HISTO 2
#define MINOR_PULL_DESC_HISTO 3
#define MINOR_PUSH_DESC_LIST 4
#define MINOR_PULL_DESC_LIST 5
#define MINOR_PUSH_DESC_FIFO 6
#define MINOR_PULL_DESC_FIFO 7
#define MINOR_PUSH_STATUS 8
#define MINOR_PULL_STATUS 9
#define MGT_MINOR_COUNT 10

#define PD_FIFO_SHL(file)                                              \
	(PD(file)->minor == MINOR_PUSH_DESC_FIFO ? DMA_DATA_PUSH_SHL : \
						   DMA_DATA_PULL_SHL)

#define PD_FIFO_OFFSET(file)                                            \
	(PD(file)->minor == MINOR_PUSH_DESC_FIFO ? DMA_PUSH_DESC_FIFO : \
						   DMA_PULL_DESC_FIFO)

void mgt400wr32(struct mgt400_dev *mdev, int offset, u32 value);
u32 mgt400rd32(struct mgt400_dev *mdev, int offset);

#endif /* MGT400_INCLUDES_H_ */
