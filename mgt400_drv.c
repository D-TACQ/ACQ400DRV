/* ------------------------------------------------------------------------- */
/* mgt400_drv.c  D-TACQ mgt400_comms driver
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 *
 * ------------------------------------------------------------------------- */
/*   Copyright (C) 2015 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
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
#include "mgt400.h"
#include "dmaengine.h"

#define REVID "0.147"

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME 	"mgt400"


static char* revid = REVID;
module_param(revid, charp, 0644);

int ndevices;
module_param(ndevices, int, 0444);
MODULE_PARM_DESC(ndevices, "number of devices found in probe");
#undef MAXDEVICES
#define MAXDEVICES 4

char* MODEL = "";
module_param(MODEL, charp, 0444);


int counter_updates;
module_param(counter_updates, int, 0444);
MODULE_PARM_DESC(counter_updates, "monitor count rate");

int maxdevices = MAXDEVICES;

int mgt_reset_on_close = 0;
module_param(mgt_reset_on_close, int , 0644);
MODULE_PARM_DESC(mgt_reset_on_close, "1: close resets FIFO");

int mgt400_cr_init = 0;
module_param(mgt400_cr_init, int , 0444);
MODULE_PARM_DESC(mgt_reset_on_close, "0: no CR init. 1: enable 2: enable+full sites");

/* index from 0. There's only one physical MGT400, but may have 2 channels */
struct mgt400_dev* mgt400_devices[MAXDEVICES+2];

/* Define debugging for use during our driver bringup */
#undef PDEBUG
#define PDEBUG(fmt, args...) printk(KERN_INFO fmt, ## args)

#define OF_IRQ_USEME		1	/* index of IRQ# in dtb */
#define OF_IRQ_COUNT		3	/* number of items */
#define OF_IRQ_MAGIC		32	/* add to the OF number to get actual */



#undef DEVP
#define DEVP(mdev)		(&(mdev)->pdev->dev)



static struct mgt400_dev* mgt400_allocate_dev(struct platform_device *pdev)
/* Allocate and init a private structure to manage this device */
{
	struct mgt400_dev* mdev = kzalloc(sizeof(struct mgt400_dev), GFP_KERNEL);
        if (mdev == NULL) {
                return NULL;
        }
        mdev->pdev = pdev;
        return mdev;
}



/* assume we can count packets by scanning the descriptor id at 100Hz.
 * 16 ID's, handles packet rate to 1600 pps - easy doable.
 */

#define PD_ID(pkt) ((pkt)&DESCR_ID)

static unsigned _channel_buffer_counter(struct mgt400_dev* mdev, u32 reg, struct DMA_CHANNEL* dma)
{
	u32 current_descr = mgt400rd32(mdev, reg);
	unsigned packet_count = 0;

	for (; dma->last_packet_id != PD_ID(current_descr); ++packet_count){
		dma->last_packet_id = PD_ID(dma->last_packet_id+1);
	}

	if (packet_count){
		if (dma->buffer_count == 0){
			dma->buffer_count = 1;
		}else{
			if (dma->previous_count){
				dma->buffer_count += packet_count;
			}else{
				/* gives better results SR > buffer_rate */
				dma->buffer_count += 1;
			}
			dma->previous_count = packet_count;
		}
		return current_descr;
	}else{
		dma->previous_count = 0;
		return 0;
	}
}

static void _update_histogram(unsigned long *histo, unsigned count, unsigned mask)
{
	if (count > mask) count = mask;
	histo[count]++;
}

static void _update_histogram_desc(
	struct DMA_CHANNEL* chan, u32 data_sr, u32 desc_sr, u32 desc)
{
	_update_histogram(chan->data_histo, GET_DMA_DATA_FIFO_COUNT(data_sr), DATA_HMASK);
	_update_histogram(chan->desc_histo, GET_DMA_DATA_FIFO_COUNT(desc_sr), DATA_HMASK);

	if (chan->fd_ix < DESC_HISTOLEN){
		chan->first_descriptors[chan->fd_ix++] = desc;
	}
}
static void _update_histograms(
	struct mgt400_dev* mdev, unsigned push_desc, unsigned pull_desc)
{
	u32 data_sr = mgt400rd32(mdev, DMA_FIFO_SR);
	u32 desc_sr = mgt400rd32(mdev, DESC_FIFO_SR);

	if (push_desc){
		_update_histogram_desc(&mdev->push, data_sr>>DMA_DATA_PUSH_SHL,
			desc_sr>>DMA_DATA_PUSH_SHL, push_desc);
	}

	if (pull_desc){
		_update_histogram_desc(&mdev->pull, data_sr>>DMA_DATA_PULL_SHL,
			desc_sr>>DMA_DATA_PULL_SHL, pull_desc);
	}
}
static void _mgt400_buffer_counter(struct mgt400_dev* mdev)
{
	unsigned push = _channel_buffer_counter(
			mdev, DMA_PUSH_DESC_SR, &mdev->push);
	unsigned pull = _channel_buffer_counter(
			mdev, DMA_PULL_DESC_SR, &mdev->pull);
	if (push || pull){
		_update_histograms(mdev, push, pull);
	}
}

static void _mgt400_dma_status_check1(struct mgt400_dev* mdev, int id, int stat)
{
	if (stat != mdev->dma_enable_status[id].status){
		mdev->dma_enable_status[id].status = stat;
		wake_up_interruptible(&mdev->dma_enable_status[id].status_change);
	}
}

static void _mgt400_dma_status_check(struct mgt400_dev* mdev)
{
	unsigned dma_ctrl = mgt400rd32(mdev, DMA_CTRL);
	_mgt400_dma_status_check1(mdev, ID_PUSH, (dma_ctrl>>DMA_DATA_PUSH_SHL)&1);
	_mgt400_dma_status_check1(mdev, ID_PULL, (dma_ctrl>>DMA_DATA_PULL_SHL)&1);
}


static void _mgt_status_init(struct mgt400_dev* mdev, int id, int stat)
{
        init_waitqueue_head(&mdev->dma_enable_status[id].status_change);
        mdev->dma_enable_status[id].status = stat;
}

static ktime_t kt_period;

enum hrtimer_restart mgt400_buffer_counter(struct hrtimer* hrt)
{
	struct mgt400_dev *mdev =
		container_of(hrt, struct mgt400_dev, buffer_counter_timer);

	_mgt400_buffer_counter(mdev);
	_mgt400_dma_status_check(mdev);
	counter_updates++;
	hrtimer_forward_now(hrt, kt_period);
	return HRTIMER_RESTART;
}
void mgt400_start_buffer_counter(struct mgt400_dev* mdev)
{
	kt_period = ktime_set(0, 10*NSEC_PER_MSEC);
	hrtimer_init(&mdev->buffer_counter_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	mdev->buffer_counter_timer.function = mgt400_buffer_counter;
	hrtimer_start(&mdev->buffer_counter_timer, kt_period, HRTIMER_MODE_REL);
}

void mgt400_stop_buffer_counter(struct mgt400_dev* mdev)
{
	hrtimer_cancel(&mdev->buffer_counter_timer);
}

void mgt400_clear_counters(struct mgt400_dev* mdev)
{
	dev_info(DEVP(mdev), "mgt400_clear_counters :%p  %d %lu",
			&mdev->push, sizeof(struct DMA_CHANNEL), mdev->push.buffer_count);
	mgt400_stop_buffer_counter(mdev);
	memset(&mdev->push, 0, sizeof(struct DMA_CHANNEL));
	dev_info(DEVP(mdev), "mgt400_clear_counters :%p  %d %lu",
			&mdev->push, sizeof(struct DMA_CHANNEL), mdev->push.buffer_count);
	memset(&mdev->pull, 0, sizeof(struct DMA_CHANNEL));
	mgt400_start_buffer_counter(mdev);
}


static void mgt_status_init(struct mgt400_dev* mdev)
{
	unsigned dma_ctrl = mgt400rd32(mdev, DMA_CTRL);
	_mgt_status_init(mdev, ID_PUSH, (dma_ctrl>>DMA_DATA_PUSH_SHL)&1);
	_mgt_status_init(mdev, ID_PULL, (dma_ctrl>>DMA_DATA_PULL_SHL)&1);
	mgt400_start_buffer_counter(mdev);
}


static int mgt400_device_tree_init(struct mgt400_dev* mdev)
{
	struct device_node *of_node = mdev->pdev->dev.of_node;

	dev_info(DEVP(mdev), "mgt400_device_tree_init() 01 %p", of_node);
        if (of_node) {
        	u32 irqs[OF_IRQ_COUNT];

        	if (of_property_read_u32(of_node, "site",
        			&mdev->of_prams.site) < 0){
        		dev_warn(DEVP(mdev), "error: site NOT specified in DT\n");
        		return -1;
        	}
        	if (of_property_read_u32(of_node, "sn",
        			&mdev->of_prams.sn) < 0){
        		dev_warn(DEVP(mdev), "error: sn NOT specified in DT\n");
        		        		return -1;
        	}else{
        		snprintf(mdev->devname, 16, "mgt400.%c",
        				mdev->of_prams.sn+'A');
        	}
        	if (of_property_read_u32(of_node, "physid",
        	        			&mdev->of_prams.phys) < 0){
        	}
                if (of_property_read_u32_array(
                		of_node, "interrupts", irqs, OF_IRQ_COUNT)){
                	dev_warn(DEVP(mdev), "failed to find IRQ values");
                }else{
                	mdev->of_prams.irq = irqs[OF_IRQ_USEME] + OF_IRQ_MAGIC;
                }

                dev_info(DEVP(mdev), "mgt400 \"%s\" site:%d sn:%d phys:%s",
                		mdev->devname, mdev->of_prams.site,
				mdev->of_prams.sn,
				mdev->of_prams.phys? "PCIe": "SFP");
                return 0;
        }else{
        	return -1;
        }
}


#define MAX_DESCR	128	/* conservative, should be 512 */

void mgt400_fifo_reset(struct mgt400_dev* mdev, unsigned shl)
{
	u32 cr = mgt400rd32(mdev, DMA_CTRL);
	cr &= ~((DMA_CTRL_EN|DMA_CTRL_RST)<<shl);
	mgt400wr32(mdev, DMA_CTRL, cr|(DMA_CTRL_RST<<shl));
	mgt400wr32(mdev, DMA_CTRL, cr);
	dev_dbg(DEVP(mdev), "%s 99", __FUNCTION__);
}
void mgt400_fifo_enable(struct mgt400_dev* mdev, unsigned shl)
{
	u32 cr = mgt400rd32(mdev, DMA_CTRL);
	u32 cr_en = cr|(DMA_CTRL_EN<<shl);
	if (cr != cr_en){
		mgt400wr32(mdev, DMA_CTRL, cr_en);
	}
	dev_dbg(DEVP(mdev), "%s 99", __FUNCTION__);
}

int mgt400_fifo_count(struct mgt400_dev* mdev, unsigned shl)
{
	u32 sr = mgt400rd32(mdev, DESC_FIFO_SR);
	sr >>= (shl);

	return (sr&DMA_DATA_FIFO_COUNT) >>  DMA_DATA_FIFO_COUNT_SHL;
}

int mgt400_headroom(struct mgt400_dev* mdev, unsigned shl)
{
	u32 count = mgt400_fifo_count(mdev, shl);

	if (count >= MAX_DESCR){
		return 0;		/* no headroom */
	}else{
		return MAX_DESCR - count;
	}
}

int _mgt400_dma_descr_open(struct inode *inode, struct file *file)
{
	struct mgt400_dev* mdev = PD(file)->dev;
	unsigned shl = PD_FIFO_SHL(file);
	mgt400_fifo_reset(mdev, shl);
	return 0;
}
ssize_t mgt400_dma_descr_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct mgt400_dev* mdev = PD(file)->dev;
	unsigned fifo_offset = PD_FIFO_OFFSET(file);
	unsigned shl = PD_FIFO_SHL(file);

	u32* lbuf = PD(file)->buffer;
	int cw, iw;
	int rc;
	int pollcat = 0;
	wait_queue_head_t wq_dummy;

	init_waitqueue_head(&wq_dummy);

	dev_dbg(DEVP(mdev), "%s 01 count %d pos %u",
			__FUNCTION__, count, *(unsigned *)f_pos);
	if (count&3){
		count = count&~3;	/* truncate to LW */
	}
	cw = count / sizeof(u32);
	if (cw > PDBUF_WORDS){
		cw = PDBUF_WORDS;
	}

	rc = copy_from_user(lbuf, buf, cw*sizeof(u32));
	if (rc){
		return -1;
	}

	while(mgt400_headroom(mdev, shl) == 0){
		if ((pollcat++&0xff) == 0){
			dev_dbg(DEVP(mdev), "%s 30 polling headroom", __FUNCTION__);
		}
		rc = wait_event_interruptible_timeout(
			wq_dummy, mgt400_headroom(mdev, shl) != 0, 10);
		if (rc == 0){
			continue;	/* timeout and retry */
		}else if (rc < 0){
			dev_warn(DEVP(mdev), "%s Quit on signal", __FUNCTION__);
			return -EINTR;
		}else if (rc > 0){
			break;
		}
	}


	dev_dbg(DEVP(mdev), "%s 55 headroom %d",
			__FUNCTION__, mgt400_headroom(mdev, shl));

	for (iw = 0; iw < cw && mgt400_headroom(mdev, shl); ++iw){
		dev_dbg(DEVP(mdev), "%s 66 [%d] %04x = 0x%08x",
					__FUNCTION__, iw, fifo_offset, lbuf[iw]);

		mgt400wr32(mdev, fifo_offset, lbuf[iw]);
	}
	mgt400_fifo_enable(mdev, shl);

	count = iw * sizeof(u32);
	*f_pos += count;
	dev_dbg(DEVP(mdev), "%s 99 return %d", __FUNCTION__, count);
	return count;
}

int mgt400_release(struct inode *inode, struct file *file)
{
	dev_dbg(DEVP(PD(file)->dev), "%s 01", __FUNCTION__);
	kfree(PD(file));
	return 0;
}
int mgt400_dma_descr_release(struct inode *inode, struct file *file)
{
	struct mgt400_dev* mdev = PD(file)->dev;
	unsigned shl = PD_FIFO_SHL(file);
	wait_queue_head_t wq_dummy;

	init_waitqueue_head(&wq_dummy);

	dev_dbg(DEVP(mdev), "%s 01 count %d", __FUNCTION__, mgt400_fifo_count(mdev, shl));

	while(mgt400_fifo_count(mdev, shl)){
		int rc = wait_event_interruptible_timeout(
				wq_dummy,
				mgt400_fifo_count(mdev, shl) == 0, 10);
		if (rc == 0){
			continue;	/* timeout and retry */
		}else if (rc < 0){
			dev_warn(DEVP(mdev), "Quit on signal");
			goto quit;
		}else if (rc > 0){
			break;
		}
	}
quit:
	if (mgt_reset_on_close){
		mgt400_fifo_reset(mdev, shl);
	}
	dev_dbg(DEVP(mdev), "%s 99", __FUNCTION__);
	return mgt400_release(inode, file);
}
int mgt400_dma_descr_open(struct inode *inode, struct file *file)
{
	static struct file_operations mgt400_dma_descr_fops = {
		.open = _mgt400_dma_descr_open,
		.write = mgt400_dma_descr_write,
		.release = mgt400_dma_descr_release,
	};
	struct mgt400_dev *mdev = PD(file)->dev;
	dev_dbg(DEVP(mdev), "%s 01", __FUNCTION__);

	file->f_op = &mgt400_dma_descr_fops;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}

#define MGT_STATUS_OLD  (PD(file)->buffer[0])
#define MGT_STATUS_ID	(PD(file)->minor == MINOR_PULL_STATUS? ID_PULL: ID_PUSH)

ssize_t mgt400_status_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct mgt400_dev *mdev = PD(file)->dev;
	unsigned id = MGT_STATUS_ID;
	char lbuf[4];
	int rc;

	dev_dbg(DEVP(mdev), "%s 01 id:%d", __FUNCTION__, id);

	if (count < 2){
		return -EINVAL;
	}
	if (wait_event_interruptible(
		mdev->dma_enable_status[id].status_change,
		mdev->dma_enable_status[id].status != MGT_STATUS_OLD)){
		return -EINTR;
	}
	MGT_STATUS_OLD = mdev->dma_enable_status[id].status;
	snprintf(lbuf, 4, "%d\n", PD(file)->buffer[0]&1);
	rc = copy_to_user(buf, lbuf, 2);

	dev_dbg(DEVP(mdev), "%s 99 id:%d rc:%d", __FUNCTION__, id, rc);

	if (rc != 0){
		return -rc;
	}else{
		return 2;
	}
}
int mgt400_status_open(struct inode *inode, struct file *file)
{
	static struct file_operations _fops = {
		.read = mgt400_status_read,
		.release = mgt400_release,
	};
	struct mgt400_dev *mdev = PD(file)->dev;
	unsigned id = MGT_STATUS_ID;
	dev_dbg(DEVP(mdev), "%s 01", __FUNCTION__);

	MGT_STATUS_OLD = mdev->dma_enable_status[id].status;
	file->f_op = &_fops;

	return 0;
}

int mgt400_open(struct inode *inode, struct file *file)
{
	SETPD(file, kzalloc(PDSZ, GFP_KERNEL));
	PD(file)->dev = container_of(inode->i_cdev, struct mgt400_dev, cdev);
	PD(file)->minor = MINOR(inode->i_rdev);
	switch(MINOR(inode->i_rdev)){
	case MINOR_PULL_STATUS:
	case MINOR_PUSH_STATUS:
		return mgt400_status_open(inode, file);
	case MINOR_PUSH_DESC_FIFO:
	case MINOR_PULL_DESC_FIFO:
		return mgt400_dma_descr_open(inode, file);
	}
	return 0;
}

int get_histo_from_minor(struct mgt400_dev *mdev, int minor,
		unsigned long **the_histo, int* maxentries)
{
	switch(minor){
	case  MINOR_PUSH_DATA_HISTO:
		*the_histo = mdev->push.data_histo;
		*maxentries = DATA_HISTOLEN;
		return 0;
	case  MINOR_PUSH_DESC_HISTO:
		*the_histo = mdev->push.desc_histo;
		*maxentries = DESC_HISTOLEN;
		return 0;
	case  MINOR_PULL_DATA_HISTO:
		*the_histo = mdev->pull.data_histo;
		*maxentries = DATA_HISTOLEN;
		return 0;
	case  MINOR_PULL_DESC_HISTO:
		*the_histo = mdev->pull.desc_histo;
		*maxentries = DESC_HISTOLEN;
		return 0;
	case MINOR_PUSH_DESC_LIST:
		*the_histo = mdev->push.first_descriptors;
		*maxentries = DESC_HISTOLEN;
		return 0;
	case MINOR_PULL_DESC_LIST:
		*the_histo = mdev->pull.first_descriptors;
		*maxentries = DESC_HISTOLEN;
		return 0;
	default:
		return -ENODEV;
	}
}

int mgt400_clear_histo(struct mgt400_dev *mdev, int minor)
{
	unsigned long *the_histo;
	int maxentries;
	int rc = get_histo_from_minor(mdev, minor, &the_histo, &maxentries);
	if (rc){
		return rc;
	}else{
		memset(the_histo, 0, maxentries*sizeof(unsigned));
		return 0;
	}
}
ssize_t mgt400_read_histo(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	unsigned long *the_histo;
	int maxentries;
	unsigned cursor = *f_pos;	/* f_pos counts in entries */
	int rc;

	rc = get_histo_from_minor(PD(file)->dev,
			PD(file)->minor, &the_histo, &maxentries);
	if (rc != 0){
		return rc;
	}

	if (cursor >= maxentries){
		return 0;
	}else{
		int headroom = (maxentries - cursor) * sizeof(unsigned);
		if (count > headroom){
			count = headroom;
		}
	}
	count -= count%sizeof(unsigned);
	rc = copy_to_user(buf, the_histo+cursor, count);
	if (rc){
		return -1;
	}

	*f_pos += count/sizeof(unsigned);
	return count;
}


struct file_operations mgt400_fops = {
        .owner = THIS_MODULE,
        .open = mgt400_open,
        .read = mgt400_read_histo,
        .release = mgt400_release,
};

static void enableZDMA(struct mgt400_dev* mdev)
{
	if (mgt400_cr_init == 0){
		return;
	}else{
		unsigned zdma_cr = mgt400rd32(mdev, ZDMA_CR);
		unsigned enables = ZDMA_CR_ENABLE;
		if (mgt400_cr_init > 1){
			enables |= AGG_SITES_MASK << AGGREGATOR_MSHIFT;
		}
		mgt400wr32(mdev, ZDMA_CR, zdma_cr|enables);
		dev_info(DEVP(mdev),
				"mod_id 0x%02x enabling comms aggregator 0x%08x",
				mdev->mod_id, mgt400rd32(mdev, ZDMA_CR));
	}
}
static int mgt400_probe(struct platform_device *pdev)
{
        int rc = 0;
        dev_t devno;
        struct mgt400_dev* mdev = mgt400_allocate_dev(pdev);
        dev_info(&pdev->dev, "mgt400_probe()");

        if (ndevices >= maxdevices){
        	dev_err(&pdev->dev, "ERROR: MAXDEVICES:%d", MAXDEVICES);
        	rc = -ENODEV;
        	goto remove;
        }
        if (!mdev){
        	dev_err(&pdev->dev, "unable to allocate device structure\n");
        	rc = -ENODEV;
        	goto remove;
        }

        mdev->pdev->dev.id = ndevices;
        mgt400_devices[ndevices++] = mdev;

        mdev->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (mdev->mem == NULL){
        	dev_err(DEVP(mdev), "No resources found");
        	rc = -ENODEV;
        	goto remove;
        }else if (mgt400_device_tree_init(mdev)){
        	rc = -ENODEV;
        	goto remove;
        }
        if (!request_mem_region(mdev->mem->start,
                mdev->mem->end-mdev->mem->start+1, mdev->devname)) {
                dev_err(DEVP(mdev), "can't reserve i/o memory at 0x%08X\n",
                        mdev->mem->start);
                rc = -ENODEV;
                goto fail;
        }
        mdev->va = ioremap(mdev->mem->start, mdev->mem->end-mdev->mem->start+1);

        rc = alloc_chrdev_region(&devno, 0, MGT_MINOR_COUNT, mdev->devname);
        if (rc < 0) {
        	dev_err(DEVP(mdev), "unable to register chrdev\n");
                goto fail;
        }

        mdev->mod_id = mgt400rd32(mdev, MOD_ID);
        switch(mdev->mod_id){
        case MOD_ID_MGT_DRAM:
        	maxdevices = 1;
        	break;
        case MOD_ID_HUDP:
        	dev_info(&pdev->dev, "HUDP detected");
                break;
        }

        cdev_init(&mdev->cdev, &mgt400_fops);
        mdev->cdev.owner = THIS_MODULE;
        rc = cdev_add(&mdev->cdev, devno, MGT_MINOR_COUNT);
        if (rc < 0){
        	goto fail;
        }

        mgt_status_init(mdev);
        mgt400_createSysfs(&mdev->pdev->dev);
        mgt400_createDebugfs(mdev);

        switch(mdev->mod_id){
        default:
        	enableZDMA(mdev);
        	break;
        case MOD_ID_HUDP:
                break;
        }
        return rc;

fail:
remove:
	kfree(mdev);
	return rc;
}

static int _mgt400_remove(struct mgt400_dev* mdev){
	mgt400_stop_buffer_counter(mdev);
	kfree(mdev);
	return 0;
}

static int mgt400_remove(struct platform_device *pdev)
/* undo all the probe things in reverse */
{
	if (pdev->id == -1){
		return -1;
	}else{
		return _mgt400_remove(mgt400_devices[pdev->id]);
	}
}

#ifdef CONFIG_OF
static struct of_device_id mgt400_of_match[] /* __devinitdata */ = {
        { .compatible = "D-TACQ,mgt400"  },
        { /* end of table */}
};
MODULE_DEVICE_TABLE(of, mgt400_of_match);
#else
#define mgt400_of_match NULL
#endif /* CONFIG_OF */



static struct platform_driver mgt400_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = mgt400_of_match,
        },
        .probe = mgt400_probe,
        .remove = mgt400_remove,
};

static struct proc_dir_entry *mgt400_proc_root;

void mgt400_module_init_proc(void)
{
	mgt400_proc_root = proc_mkdir("driver/acq400/mgt400", 0);
}
void mgt400_module_remove_proc(void)
{
	remove_proc_entry("driver/acq400/mgt400", NULL);
}

static void __exit mgt400_exit(void)
{
	platform_driver_unregister(&mgt400_driver);
	mgt400_module_remove_proc();
}

static int __init mgt400_init(void)
{
        int status;

	printk("D-TACQ MGT400 Comms Module Driver %s\n", REVID);
	//kt_period = ktime_set(0, 10000000);
	kt_period = ktime_set(0, 1000000);
	mgt400_module_init_proc();
        status = platform_driver_register(&mgt400_driver);

        return status;
}

module_init(mgt400_init);
module_exit(mgt400_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
