/*
 * Xilinx AXI DMA Engine support
 *
 * Copyright (C) 2012 - 2013 Xilinx, Inc. All rights reserved.
 *
 * Based on the Freescale DMA driver.
 *
 * Description:
 *  . Axi DMA engine, it does transfers between memory and device. It can be
 *    configured to have one channel or two channels. If configured as two
 *    channels, one is to transmit to a device and another is to receive from
 *    a device.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "include/linux/dmaengine.h"
#include "include/linux/amba/xilinx_dma.h"
#include <linux/dmapool.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/moduleparam.h>

int maxdev = 1;
module_param(maxdev, int, 0444);
MODULE_PARM_DESC(maxdev, "maximum number of devices");

int ndevices = 0;
module_param(ndevices, int, 0444);
MODULE_PARM_DESC(ndevices, "actual number of instantiated devices");

int dbg_dump_desc = 0;
module_param(dbg_dump_desc, int, 0644);
MODULE_PARM_DESC(dbg_dump_desc, "print descriptor chain before and after");

int th_max = 99;
module_param(th_max, int, 0644);
MODULE_PARM_DESC(th_max, "max interrupt threshold");

int axi64_icount = 0;
module_param(axi64_icount, int, 0644);
MODULE_PARM_DESC(icount, "combined interrupt count, reset at start");

#if defined(CONFIG_XILINX_DMATEST) || defined(CONFIG_XILINX_DMATEST_MODULE)
# define TEST_DMA_WITH_LOOPBACK
#endif

#include "xilinx_axidma.h"

void xilinx_set_irq_threshold(struct xilinx_dma_chan *chan, int sg_len)
{
	unsigned cr = dma_read(chan, XILINX_DMA_CONTROL_OFFSET);
	sg_len = min(sg_len, th_max);
	cr &= ~XILINX_DMA_XR_COALESCE_MASK;
	cr |= sg_len << XILINX_DMA_COALESCE_SHIFT;
	dev_dbg(chan->dev, "xilinx_set_irq_threshold %d 0x%08x",
			sg_len, cr);
	dma_write(chan, XILINX_DMA_CONTROL_OFFSET, cr);
}

static int xilinx_dma_alloc_chan_resources(struct dma_chan *dchan)
{
	struct xilinx_dma_chan *chan = to_xilinx_chan(dchan);

	/* Has this channel already been allocated? */
	if (chan->desc_pool)
		return 1;

	/*
	 * We need the descriptor to be aligned to 64bytes
	 * for meeting Xilinx DMA specification requirement.
	 */
	chan->desc_pool =
		dma_pool_create("xilinx_dma_desc_pool", chan->dev,
				sizeof(struct xilinx_dma_desc_sw),
				__alignof__(struct xilinx_dma_desc_sw), 0);
	if (!chan->desc_pool) {
		dev_err(chan->dev,
			"unable to allocate channel %d descriptor pool\n",
			chan->id);
		return -ENOMEM;
	}

	chan->completed_cookie = 1;
	chan->cookie = 1;

	/* There is at least one descriptor free to be allocated */
	return 1;
}

static void xilinx_dma_free_desc_list(struct xilinx_dma_chan *chan,
				      struct list_head *list)
{
	struct xilinx_dma_desc_sw *desc, *_desc;

	list_for_each_entry_safe(desc, _desc, list, node) {
		list_del(&desc->node);
		dma_pool_free(chan->desc_pool, desc, desc->async_tx.phys);
	}
}

static void xilinx_dma_free_desc_list_reverse(struct xilinx_dma_chan *chan,
					      struct list_head *list)
{
	struct xilinx_dma_desc_sw *desc, *_desc;

	list_for_each_entry_safe_reverse(desc, _desc, list, node) {
		list_del(&desc->node);
		dma_pool_free(chan->desc_pool, desc, desc->async_tx.phys);
	}
}

static void xilinx_dma_free_chan_resources(struct dma_chan *dchan)
{
	struct xilinx_dma_chan *chan = to_xilinx_chan(dchan);
	unsigned long flags;

	dev_dbg(chan->dev, "Free all channel resources.\n");
	spin_lock_irqsave(&chan->lock, flags);
	xilinx_dma_free_desc_list(chan, &chan->active_list);
	xilinx_dma_free_desc_list(chan, &chan->pending_list);
	spin_unlock_irqrestore(&chan->lock, flags);

	dma_pool_destroy(chan->desc_pool);
	chan->desc_pool = NULL;
}

/* static */
enum dma_status xilinx_dma_desc_status(struct xilinx_dma_chan *chan,
					      struct xilinx_dma_desc_sw *desc)
{
	enum dma_status rc;
	/** @@todo .. PGM figure this out */

	dev_dbg(chan->dev, "xilinx_dma_desc_status() current: %08x complete: %08x last_used: %08x",
			desc->async_tx.cookie, chan->completed_cookie, chan->cookie);


	rc = dma_async_is_complete(desc->async_tx.cookie,
				     chan->completed_cookie,
				     chan->cookie);
	if (rc == DMA_COMPLETE){
		return rc;
	}else{
		dev_dbg(chan->dev, "xilinx_dma_desc_status() rc=%d papering over", rc);
		chan->completed_cookie = chan->cookie;
		return DMA_COMPLETE;
	}
}

static void dump_desc(
		struct xilinx_dma_chan *chan, struct xilinx_dma_desc_sw *desc,
		const char* id)
{
	dev_info(chan->dev, "%s ->%08x b:%08x c:%08x s:%08x",
		 id, desc->hw.next_desc, desc->hw.buf_addr,
		 desc->hw.control, desc->hw.status 		);
}
static void xilinx_chan_desc_cleanup(struct xilinx_dma_chan *chan)
{
	struct xilinx_dma_desc_sw *desc, *_desc;
	unsigned long flags;
	dma_async_tx_callback callback = 0;
	void *callback_param;
	dev_dbg(chan->dev, "xilinx_chan_desc_cleanup()");
	spin_lock_irqsave(&chan->lock, flags);

	xilinx_set_irq_threshold(chan, 1);

	list_for_each_entry_safe(desc, _desc, &chan->active_list, node) {
		if (dbg_dump_desc){
			dump_desc(chan, desc, "cleanup");
		}
		if (xilinx_dma_desc_status(chan, desc) == DMA_IN_PROGRESS){
			dev_warn(chan->dev, "xilinx_chan_desc_cleanup() DMA_IN_PROGRESS");
			break;
		}

		/* Remove from the list of running transactions */
		list_del(&desc->node);

		if (desc->async_tx.callback){
			if (callback) {
				spin_unlock_irqrestore(&chan->lock, flags);
				dev_warn(chan->dev, "xilinx_chan_desc_cleanup() INLOOP callback:%p pram:%p", callback, callback_param);
				callback(callback_param);
				spin_lock_irqsave(&chan->lock, flags);
			}
			callback = desc->async_tx.callback;
			callback_param = desc->async_tx.callback_param;
		}

		/* Run any dependencies, then free the descriptor */
		dma_run_dependencies(&desc->async_tx);
		dma_pool_free(chan->desc_pool, desc, desc->async_tx.phys);
	}

	spin_unlock_irqrestore(&chan->lock, flags);
	if (callback) {
		dev_dbg(chan->dev, "xilinx_chan_desc_cleanup() NORMAL callback:%p pram:%p", callback, callback_param);
		callback(callback_param);
	}
	dev_dbg(chan->dev, "xilinx_chan_desc_cleanup() 99");
}

static enum dma_status xilinx_tx_status(struct dma_chan *dchan,
					dma_cookie_t cookie,
					struct dma_tx_state *txstate)
{
	struct xilinx_dma_chan *chan = to_xilinx_chan(dchan);
	dma_cookie_t last_used;
	dma_cookie_t last_complete;

	xilinx_chan_desc_cleanup(chan);

	last_used = dchan->cookie;
	last_complete = chan->completed_cookie;

	dma_set_tx_state(txstate, last_complete, last_used, 0);

	return dma_async_is_complete(cookie, last_complete, last_used);
}

static int dma_is_running(struct xilinx_dma_chan *chan)
{
	return !(dma_read(chan, XILINX_DMA_STATUS_OFFSET) &
		 XILINX_DMA_SR_HALTED_MASK) &&
	       (dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
		XILINX_DMA_CR_RUNSTOP_MASK);
}

static int dma_is_idle(struct xilinx_dma_chan *chan)
{
	return dma_read(chan, XILINX_DMA_STATUS_OFFSET) &
	       XILINX_DMA_SR_IDLE_MASK;
}

/* Stop the hardware, the ongoing transfer will be finished */
static void dma_halt(struct xilinx_dma_chan *chan)
{
	int loop = XILINX_DMA_HALT_LOOP;

	dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
		  dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
		  ~XILINX_DMA_CR_RUNSTOP_MASK);

	/* Wait for the hardware to halt */
	while (loop) {
		if (!(dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
		      XILINX_DMA_CR_RUNSTOP_MASK))
			break;

		loop -= 1;
	}

	if (!loop) {
		pr_debug("Cannot stop channel %x: %x\n",
			 (unsigned int)chan,
			 (unsigned int)dma_read(chan,
						XILINX_DMA_CONTROL_OFFSET));
		chan->err = 1;
	}
}

/* Start the hardware. Transfers are not started yet */
static void dma_start(struct xilinx_dma_chan *chan)
{
	int loop = XILINX_DMA_HALT_LOOP;

	dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
		  dma_read(chan, XILINX_DMA_CONTROL_OFFSET) |
		  XILINX_DMA_CR_RUNSTOP_MASK);

	/* Wait for the hardware to start */
	while (loop) {
		if (dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
		    XILINX_DMA_CR_RUNSTOP_MASK)
			break;

		loop -= 1;
	}

	if (!loop) {
		pr_debug("Cannot start channel %x: %x\n",
			 (unsigned int)chan,
			 (unsigned int)dma_read(chan,
						XILINX_DMA_CONTROL_OFFSET));

		chan->err = 1;
	}
}

static void xilinx_dma_start_transfer(struct xilinx_dma_chan *chan)
{
	unsigned long flags;
	struct xilinx_dma_desc_sw *desch, *desct;
	struct xilinx_dma_desc_hw *hw;

	if (chan->err)
		return;

	spin_lock_irqsave(&chan->lock, flags);

	if (list_empty(&chan->pending_list))
		goto out_unlock;

	/* If hardware is busy, cannot submit */
	if (dma_is_running(chan) && !dma_is_idle(chan)) {
		dev_dbg(chan->dev, "DMA controller still busy\n");
		goto out_unlock;
	}

	/*
	 * If hardware is idle, then all descriptors on active list are
	 * done, start new transfers
	 */
	dma_halt(chan);

	if (chan->err)
		goto out_unlock;

	if (chan->has_sg) {
		desch = list_first_entry(&chan->pending_list,
					 struct xilinx_dma_desc_sw, node);

		desct = container_of(chan->pending_list.prev,
				     struct xilinx_dma_desc_sw, node);

		dma_write(chan, XILINX_DMA_CDESC_OFFSET, desch->async_tx.phys);

		dma_start(chan);

		if (chan->err)
			goto out_unlock;
		list_splice_tail_init(&chan->pending_list, &chan->active_list);

		/* Enable interrupts */
		dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
			  dma_read(chan, XILINX_DMA_CONTROL_OFFSET) |
			  XILINX_DMA_XR_IRQ_ALL_MASK);

		/* Update tail ptr register and start the transfer */
		dma_write(chan, XILINX_DMA_TDESC_OFFSET, desct->async_tx.phys);
		goto out_unlock;
	}

	/* In simple mode */
	dma_halt(chan);

	if (chan->err)
		goto out_unlock;

	pr_info("xilinx_dma_start_transfer::simple DMA mode\n");

	desch = list_first_entry(&chan->pending_list,
				 struct xilinx_dma_desc_sw, node);

	list_del(&desch->node);
	list_add_tail(&desch->node, &chan->active_list);

	dma_start(chan);

	if (chan->err)
		goto out_unlock;

	hw = &desch->hw;

	/* Enable interrupts */
	dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
		  dma_read(chan, XILINX_DMA_CONTROL_OFFSET) |
		  XILINX_DMA_XR_IRQ_ALL_MASK);

	dma_write(chan, XILINX_DMA_SRCADDR_OFFSET, hw->buf_addr);

	/* Start the transfer */
	dma_write(chan, XILINX_DMA_BTT_OFFSET,
		  hw->control & XILINX_DMA_MAX_TRANS_LEN);

out_unlock:
	spin_unlock_irqrestore(&chan->lock, flags);
}

static void xilinx_dma_issue_pending(struct dma_chan *dchan)
{
	struct xilinx_dma_chan *chan = to_xilinx_chan(dchan);

	xilinx_dma_start_transfer(chan);
}

/**
 * xilinx_dma_update_completed_cookie - Update the completed cookie.
 * @chan : xilinx DMA channel
 *
 * CONTEXT: hardirq
 */
static void xilinx_dma_update_completed_cookie(struct xilinx_dma_chan *chan)
{
	struct xilinx_dma_desc_sw *desc = NULL;
	struct xilinx_dma_desc_hw *hw = NULL;
	unsigned long flags;
	dma_cookie_t cookie = -EBUSY;
	int done = 0;

	spin_lock_irqsave(&chan->lock, flags);

	if (list_empty(&chan->active_list)) {
		dev_dbg(chan->dev, "no running descriptors\n");
		goto out_unlock;
	}

	/* Get the last completed descriptor, update the cookie to that */
	list_for_each_entry(desc, &chan->active_list, node) {
		if (chan->has_sg) {
			hw = &desc->hw;

			/* If a BD has no status bits set, hw has it */
			if (!(hw->status & XILINX_DMA_BD_STS_ALL_MASK)) {
				break;
			} else {
				done = 1;
				cookie = desc->async_tx.cookie;
			}
		} else {
			/* In non-SG mode, all active entries are done */
			done = 1;
			cookie = desc->async_tx.cookie;
		}
	}

	if (done)
		chan->completed_cookie = cookie;

out_unlock:
	spin_unlock_irqrestore(&chan->lock, flags);
}

/* Reset hardware */
static int dma_reset(struct xilinx_dma_chan *chan)
{
	int loop = XILINX_DMA_RESET_LOOP;
	u32 tmp;

	dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
		  dma_read(chan, XILINX_DMA_CONTROL_OFFSET) |
		  XILINX_DMA_CR_RESET_MASK);

	tmp = dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
	      XILINX_DMA_CR_RESET_MASK;

	/* Wait for the hardware to finish reset */
	while (loop && tmp) {
		tmp = dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
		      XILINX_DMA_CR_RESET_MASK;
		loop -= 1;
	}

	if (!loop) {
		dev_err(chan->dev, "reset timeout, cr %x, sr %x\n",
			dma_read(chan, XILINX_DMA_CONTROL_OFFSET),
			dma_read(chan, XILINX_DMA_STATUS_OFFSET));
		return -EBUSY;
	}

	return 0;
}


static irqreturn_t dma_intr_handler(int irq, void *data)
{
	struct xilinx_dma_chan *chan = data;
	int update_cookie = 0;
	int to_transfer = 0;
	u32 ctrl = dma_read(chan, XILINX_DMA_CONTROL_OFFSET);
	u32 stat = dma_read(chan, XILINX_DMA_STATUS_OFFSET);
	u32 cdesc = dma_read(chan, XILINX_DMA_CDESC_OFFSET);

	if (!(stat & XILINX_DMA_XR_IRQ_ALL_MASK))
		return IRQ_NONE;

	/* Ack the interrupts */
	dma_write(chan, XILINX_DMA_STATUS_OFFSET, XILINX_DMA_XR_IRQ_ALL_MASK);
	/* Check for only the interrupts which are enabled */
	stat &= (ctrl & XILINX_DMA_XR_IRQ_ALL_MASK);

	axi64_icount = (axi64_icount+1)&0xf;
	chan->dTrace.buffer[chan->dTrace.cursor] = cdesc|axi64_icount;
	chan->dTrace.cursor = (chan->dTrace.cursor+1)&(MAXTRACE-1);

	dev_dbg(chan->dev, "dma_intr_handler() stat:%08x curr:%08x", stat, cdesc);

	if (stat & XILINX_DMA_XR_IRQ_ERROR_MASK) {
		dev_err(chan->dev,
			"Channel %x has errors %x, cdr %x tdr %x\n",
			(unsigned int)chan,
			(unsigned int)dma_read(chan, XILINX_DMA_STATUS_OFFSET),
			(unsigned int)dma_read(chan, XILINX_DMA_CDESC_OFFSET),
			(unsigned int)dma_read(chan, XILINX_DMA_TDESC_OFFSET));
		chan->err = 1;
	}

	/*
	 * Device takes too long to do the transfer when user requires
	 * responsiveness
	 */
	if (stat & XILINX_DMA_XR_IRQ_DELAY_MASK)
		dev_dbg(chan->dev, "Inter-packet latency too long\n");

	if (stat & XILINX_DMA_XR_IRQ_IOC_MASK) {
		update_cookie = 1;
		to_transfer = 1;
	}

	if (update_cookie)
		xilinx_dma_update_completed_cookie(chan);

	if (to_transfer)
		chan->start_transfer(chan);

	dev_dbg(chan->dev, "dma_intr_handler() tasklet_schedule");
	tasklet_schedule(&chan->tasklet);

	return IRQ_HANDLED;
}

static void dma_do_tasklet(unsigned long data)
{
	struct xilinx_dma_chan *chan = (struct xilinx_dma_chan *)data;
	dev_dbg(chan->dev, "dma_do_tasklet cookie 0x%08x", chan->common.cookie);

	xilinx_chan_desc_cleanup(chan);
}

/* Append the descriptor list to the pending list */
static void append_desc_queue(struct xilinx_dma_chan *chan,
			      struct xilinx_dma_desc_sw *desc)
{
	struct xilinx_dma_desc_sw *tail =
		container_of(chan->pending_list.prev,
			     struct xilinx_dma_desc_sw, node);
	struct xilinx_dma_desc_hw *hw;

	if (list_empty(&chan->pending_list))
		goto out_splice;

	/*
	 * Add the hardware descriptor to the chain of hardware descriptors
	 * that already exists in memory.
	 */
	hw = &(tail->hw);
	hw->next_desc = (u32)desc->async_tx.phys;

	/*
	 * Add the software descriptor and all children to the list
	 * of pending transactions
	 */
out_splice:
	list_splice_tail_init(&desc->tx_list, &chan->pending_list);
}

/*
 * Assign cookie to each descriptor, and append the descriptors to the pending
 * list
 */
static dma_cookie_t xilinx_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct xilinx_dma_chan *chan = to_xilinx_chan(tx->chan);
	struct xilinx_dma_desc_sw *desc;
	struct xilinx_dma_desc_sw *child;
	unsigned long flags;
	dma_cookie_t cookie = -EBUSY;

	desc = container_of(tx, struct xilinx_dma_desc_sw, async_tx);

	if (chan->err) {
		/*
		 * If reset fails, need to hard reset the system.
		 * Channel is no longer functional
		 */
		if (!dma_reset(chan))
			chan->err = 0;
		else
			return cookie;
	}

	spin_lock_irqsave(&chan->lock, flags);

	/*
	 * Assign cookies to all of the software descriptors
	 * that make up this transaction
	 */
	cookie = chan->cookie;
	list_for_each_entry(child, &desc->tx_list, node) {
		cookie++;
		if (cookie < 0)
			cookie = DMA_MIN_COOKIE;

		child->async_tx.cookie = cookie;
	}

	chan->cookie = cookie;

	/* Put this transaction onto the tail of the pending queue */
	append_desc_queue(chan, desc);

	spin_unlock_irqrestore(&chan->lock, flags);

	return cookie;
}

static struct
xilinx_dma_desc_sw *xilinx_dma_alloc_descriptor(struct xilinx_dma_chan *chan)
{
	struct xilinx_dma_desc_sw *desc;
	dma_addr_t pdesc;

	desc = dma_pool_alloc(chan->desc_pool, GFP_ATOMIC, &pdesc);
	if (!desc) {
		dev_dbg(chan->dev, "out of memory for desc\n");
		return NULL;
	}

	memset(desc, 0, sizeof(*desc));
	INIT_LIST_HEAD(&desc->tx_list);
	dma_async_tx_descriptor_init(&desc->async_tx, &chan->common);
	desc->async_tx.tx_submit = xilinx_dma_tx_submit;
	desc->async_tx.phys = pdesc;

	return desc;
}

/**
 * xilinx_dma_prep_slave_sg - prepare descriptors for a DMA_SLAVE transaction
 * @chan: DMA channel
 * @sgl: scatterlist to transfer to/from
 * @sg_len: number of entries in @scatterlist
 * @direction: DMA direction
 * @flags: transfer ack flags
 */
static struct dma_async_tx_descriptor *xilinx_dma_prep_slave_sg(
	struct dma_chan *dchan, struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	struct xilinx_dma_chan *chan;
	struct xilinx_dma_desc_sw *first = NULL, *prev = NULL, *new = NULL;
	struct xilinx_dma_desc_hw *hw = NULL, *prev_hw = NULL;

	size_t copy;

	int i;
	struct scatterlist *sg;
	size_t sg_used;
	dma_addr_t dma_src;

#ifdef TEST_DMA_WITH_LOOPBACK
	int total_len;
#endif
	if (!dchan)
		return NULL;

	chan = to_xilinx_chan(dchan);

	if (chan->direction != direction)
		return NULL;
	dev_dbg(chan->dev, "xilinx_dma_prep_slave_sg() dir:%d len=%d", direction, sg_len);

	tasklet_init(&chan->tasklet, dma_do_tasklet, (unsigned long)chan);
#ifdef TEST_DMA_WITH_LOOPBACK
	total_len = 0;

	for_each_sg(sgl, sg, sg_len, i) {
		total_len += sg_dma_len(sg);
	}
#endif
	/* Build transactions using information in the scatter gather list */
	for_each_sg(sgl, sg, sg_len, i) {
		sg_used = 0;

		/* Loop until the entire scatterlist entry is used */
		while (sg_used < sg_dma_len(sg)) {

			/* Allocate the link descriptor from DMA pool */
			new = xilinx_dma_alloc_descriptor(chan);
			if (!new) {
				dev_err(chan->dev,
					"No free memory for link descriptor\n");
				goto fail;
			}
			/*
			 * Calculate the maximum number of bytes to transfer,
			 * making sure it is less than the hw limit
			 */
			copy = min((size_t)(sg_dma_len(sg) - sg_used),
				   (size_t)chan->max_len);
			hw = &(new->hw);

			dma_src = sg_dma_address(sg) + sg_used;

			hw->buf_addr = dma_src;

			/* Fill in the descriptor */
			hw->control = copy;

			/*
			 * If this is not the first descriptor, chain the
			 * current descriptor after the previous descriptor
			 *
			 * For the first DMA_MEM_TO_DEV transfer, set SOP
			 */
			if (!first) {
				first = new;
				if (direction == DMA_MEM_TO_DEV) {
					hw->control |= XILINX_DMA_BD_SOP;
#ifdef TEST_DMA_WITH_LOOPBACK
					hw->app_4 = total_len;
#endif
				}
			} else {
				prev_hw = &(prev->hw);
				prev_hw->next_desc = new->async_tx.phys;
			}

			new->async_tx.cookie = 0;
			async_tx_ack(&new->async_tx);

			prev = new;
			sg_used += copy;

			/* Insert the link descriptor into the LD ring */
			list_add_tail(&new->node, &first->tx_list);
		}
	}

	/* Link the last BD with the first BD */
	hw->next_desc = first->async_tx.phys;

	if (direction == DMA_MEM_TO_DEV)
		hw->control |= XILINX_DMA_BD_EOP;

	/* All scatter gather list entries has length == 0 */
	if (!first || !new)
		return NULL;

	if (dbg_dump_desc){
		struct xilinx_dma_desc_sw *desc, *_desc;
		dev_info(chan->dev, "xilinx_dma_prep_slave_sg");
		list_for_each_entry_safe(desc, _desc, &first->tx_list, node) {
			dump_desc(chan, desc, "prep_sg");
		}
	}
	new->async_tx.flags = flags;
	new->async_tx.cookie = -EBUSY;

	/* Set EOP to the last link descriptor of new list */
	hw->control |= XILINX_DMA_BD_EOP;

	xilinx_set_irq_threshold(chan, sg_len);

	return &first->async_tx;

fail:
	/*
	 * If first was not set, then we failed to allocate the very first
	 * descriptor, and we're done
	 */
	if (!first)
		return NULL;

	/*
	 * First is set, so all of the descriptors we allocated have been added
	 * to first->tx_list, INCLUDING "first" itself. Therefore we
	 * must traverse the list backwards freeing each descriptor in turn
	 */
	xilinx_dma_free_desc_list_reverse(chan, &first->tx_list);

	return NULL;
}

/* Run-time device configuration for Axi DMA */
static int xilinx_dma_device_control(struct dma_chan *dchan,
				     enum dma_ctrl_cmd cmd, unsigned long arg)
{
	struct xilinx_dma_chan *chan;
	unsigned long flags;

	if (!dchan)
		return -EINVAL;

	chan = to_xilinx_chan(dchan);

	if (cmd == DMA_TERMINATE_ALL) {
		/* Halt the DMA engine */
		dma_halt(chan);

		spin_lock_irqsave(&chan->lock, flags);

		/* Remove and free all of the descriptors in the lists */
		xilinx_dma_free_desc_list(chan, &chan->pending_list);
		xilinx_dma_free_desc_list(chan, &chan->active_list);

		spin_unlock_irqrestore(&chan->lock, flags);
		return 0;
	} else if (cmd == DMA_SLAVE_CONFIG) {
		/*
		 * Configure interrupt coalescing and delay counter
		 * Use value XILINX_DMA_NO_CHANGE to signal no change
		 */
		struct xilinx_dma_config *cfg = (struct xilinx_dma_config *)arg;
		u32 reg = dma_read(chan, XILINX_DMA_CONTROL_OFFSET);

		if (cfg->coalesc <= XILINX_DMA_COALESCE_MAX) {
			reg &= ~XILINX_DMA_XR_COALESCE_MASK;
			reg |= cfg->coalesc << XILINX_DMA_COALESCE_SHIFT;

			chan->config.coalesc = cfg->coalesc;
		}

		if (cfg->delay <= XILINX_DMA_DELAY_MAX) {
			reg &= ~XILINX_DMA_XR_DELAY_MASK;
			reg |= cfg->delay << XILINX_DMA_DELAY_SHIFT;
			chan->config.delay = cfg->delay;
		}

		dma_write(chan, XILINX_DMA_CONTROL_OFFSET, reg);

		return 0;
	} else
		return -ENXIO;
}

#include <linux/debugfs.h>
#include "acq400_debugfs_internal.h"
struct dentry* acq400_axi_dma_debug_root;

#define ADBG_REG_CREATE(reg) 					\
	sprintf(pcursor, "%s.0x%02x", #reg, reg##_OFFSET);	\
	debugfs_create_x32(pcursor, S_IRUGO, 			\
		chan->debug_dir, chan->regs+(reg##_OFFSET));	\
	pcursor += strlen(pcursor) + 1

void acq400_axi_dma_createDebugfs(struct xilinx_dma_chan *chan)
{
	char* pcursor;
	if (!acq400_axi_dma_debug_root){
		acq400_axi_dma_debug_root = debugfs_create_dir("acq400_axi_dma", 0);
		if (!acq400_axi_dma_debug_root){
			dev_warn(chan->dev, "failed create dir acq400_axi_dma");
			return;
		}
	}
	pcursor = chan->debug_names = kmalloc(4096, GFP_KERNEL);
	sprintf(pcursor, "adma%d", chan->id);


	chan->debug_dir = debugfs_create_dir(pcursor, acq400_axi_dma_debug_root);

	if (!chan->debug_dir){
		dev_warn(chan->dev, "failed create dir %s", pcursor);
		return;
	}
	pcursor += strlen(pcursor) + 1;
	ADBG_REG_CREATE(XILINX_DMA_CONTROL);
	ADBG_REG_CREATE(XILINX_DMA_STATUS);
	ADBG_REG_CREATE(XILINX_DMA_CDESC);
	ADBG_REG_CREATE(XILINX_DMA_TDESC);
}

void acq400_axi_dma_removeDebugfs(struct xilinx_dma_chan *chan)
{
	debugfs_remove_recursive(chan->debug_dir);
	kfree(chan->debug_names);
}


static void xilinx_dma_free_channels(struct xilinx_dma_device *xdev)
{
	int i;

	for (i = 0; i < XILINX_DMA_MAX_CHANS_PER_DEVICE; i++) {
		list_del(&xdev->chan[i]->common.device_node);
		acq400_axi_dma_removeDebugfs(xdev->chan[i]);
		tasklet_kill(&xdev->chan[i]->tasklet);
		irq_dispose_mapping(xdev->chan[i]->irq);
	}
}


/*
 * Probing channels
 *
 * . Get channel features from the device tree entry
 * . Initialize special channel handling routines
 */
static int xilinx_dma_chan_probe(struct xilinx_dma_device *xdev,
				 struct device_node *node, u32 feature)
{
	struct xilinx_dma_chan *chan;
	int err;
	u32 device_id, value, width = 0;

	/* alloc channel */
	chan = devm_kzalloc(xdev->dev, sizeof(struct xilinx_dma_chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;

	chan->feature = feature;
	chan->max_len = XILINX_DMA_MAX_TRANS_LEN;

	chan->has_dre = of_property_read_bool(node, "xlnx,include-dre");

	err = of_property_read_u32(node, "xlnx,datawidth", &value);
	if (err) {
		dev_err(xdev->dev, "unable to read datawidth property");
		return err;
	} else {
		width = value >> 3; /* convert bits to bytes */

		/* If data width is greater than 8 bytes, DRE is not in hw */
		if (width > 8)
			chan->has_dre = 0;

		chan->feature |= width - 1;
	}

	err = of_property_read_u32(node, "xlnx,device-id", &device_id);
	if (err) {
		dev_err(xdev->dev, "unable to read device id property");
		return err;
	}
	chan->id = device_id;

	chan->has_sg = (xdev->feature & XILINX_DMA_FTR_HAS_SG) >>
		       XILINX_DMA_FTR_HAS_SG_SHIFT;

	chan->start_transfer = xilinx_dma_start_transfer;

	if (of_device_is_compatible(node, "xlnx,axi-dma-mm2s-channel"))
		chan->direction = DMA_MEM_TO_DEV;

	if (of_device_is_compatible(node, "xlnx,axi-dma-s2mm-channel"))
		chan->direction = DMA_DEV_TO_MEM;

	chan->regs = xdev->regs;

	if (chan->direction == DMA_DEV_TO_MEM) {
		dev_info(xdev->dev, "axi-dma-s2mm-channel offset regs _%08x", XILINX_DMA_RX_CHANNEL_OFFSET);
		chan->regs = (xdev->regs + XILINX_DMA_RX_CHANNEL_OFFSET);
	}

	/*
	 * Used by dmatest channel matching in slave transfers
	 * Can change it to be a structure to have more matching information
	 */
	chan->private = (chan->direction & 0xFF) | XILINX_DMA_IP_DMA |
			(device_id << XILINX_DMA_DEVICE_ID_SHIFT);
	chan->common.private = (void *)&(chan->private);

	if (!chan->has_dre)
		xdev->common.copy_align = fls(width - 1);

	chan->dev = xdev->dev;
	xdev->chan[chan->id] = chan;

	/* Initialize the channel */
	err = dma_reset(chan);
	if (err) {
		dev_err(xdev->dev, "Reset channel failed\n");
		return err;
	}

	spin_lock_init(&chan->lock);
	INIT_LIST_HEAD(&chan->pending_list);
	INIT_LIST_HEAD(&chan->active_list);

	chan->common.device = &xdev->common;

	/* find the IRQ line, if it exists in the device tree */
	chan->irq = irq_of_parse_and_map(node, 0);
	snprintf(chan->devname, sizeof(chan->devname), "axi-dma%d", device_id);
	err = devm_request_irq(xdev->dev, chan->irq, dma_intr_handler,
			       IRQF_SHARED, chan->devname, chan);
	if (err) {
		dev_err(xdev->dev, "unable to request IRQ\n");
		return err;
	}

	tasklet_init(&chan->tasklet, dma_do_tasklet, (unsigned long)chan);

	/* Add the channel to DMA device channel list */
	list_add_tail(&chan->common.device_node, &xdev->common.channels);

	acq400_axi_dma_createDebugfs(chan);
	return 0;
}

static int xilinx_dma_probe(struct platform_device *pdev)
{
	struct xilinx_dma_device *xdev;
	struct device_node *child, *node;
	struct resource *res;
	int ret;
	u32 value;

	if (ndevices >= maxdev){
		dev_warn(&(pdev->dev), "device_id not instantiated, limit %d already reached", maxdev);
		return -1;
	}

	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if (!xdev)
		return -ENOMEM;

	xdev->dev = &(pdev->dev);
	INIT_LIST_HEAD(&xdev->common.channels);

	node = pdev->dev.of_node;

	/* iomap registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	xdev->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(xdev->regs))
		return PTR_ERR(xdev->regs);

	/* Check if SG is enabled */
	value = of_property_read_bool(node, "xlnx,include-sg");
	if (value)
		xdev->feature |= XILINX_DMA_FTR_HAS_SG;

	/* Check if status control streams are enabled */
	value = of_property_read_bool(node,
				      "xlnx,sg-include-stscntrl-strm");
	if (value)
		xdev->feature |= XILINX_DMA_FTR_STSCNTRL_STRM;

	/* Axi DMA only do slave transfers */
	dma_cap_set(DMA_SLAVE, xdev->common.cap_mask);
	dma_cap_set(DMA_PRIVATE, xdev->common.cap_mask);
	xdev->common.device_prep_slave_sg = xilinx_dma_prep_slave_sg;
	xdev->common.device_control = xilinx_dma_device_control;
	xdev->common.device_issue_pending = xilinx_dma_issue_pending;
	xdev->common.device_alloc_chan_resources =
		xilinx_dma_alloc_chan_resources;
	xdev->common.device_free_chan_resources =
		xilinx_dma_free_chan_resources;
	xdev->common.device_tx_status = xilinx_tx_status;
	xdev->common.dev = &pdev->dev;

	platform_set_drvdata(pdev, xdev);

	for_each_child_of_node(node, child) {
		ret = xilinx_dma_chan_probe(xdev, child, xdev->feature);
		if (ret) {
			dev_err(&pdev->dev, "Probing channels failed\n");
			goto free_chan_resources;
		}
	}

	ret = dma_async_device_register(&xdev->common);
	if (ret) {
		dev_err(&pdev->dev, "DMA device registration failed\n");
		goto free_chan_resources;
	}

	dev_info(&pdev->dev, "Probing xilinx axi dma engine...Successful\n");
	++ndevices;
	return 0;

free_chan_resources:
	xilinx_dma_free_channels(xdev);

	return ret;
}

static int xilinx_dma_remove(struct platform_device *pdev)
{
	struct xilinx_dma_device *xdev;

	xdev = platform_get_drvdata(pdev);
	dma_async_device_unregister(&xdev->common);

	xilinx_dma_free_channels(xdev);

	return 0;
}

static const struct of_device_id xilinx_dma_of_match[] = {
	{ .compatible = "xlnx-acq400,axi-dma", },
	{}
};
MODULE_DEVICE_TABLE(of, xilinx_dma_of_match);

static struct platform_driver xilinx_dma_driver = {
	.driver = {
		.name = "xilinx-acq400-dma",
		.owner = THIS_MODULE,
		.of_match_table = xilinx_dma_of_match,
	},
	.probe = xilinx_dma_probe,
	.remove = xilinx_dma_remove,
};

module_platform_driver(xilinx_dma_driver);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("Xilinx DMA driver");
MODULE_LICENSE("GPL v2");
