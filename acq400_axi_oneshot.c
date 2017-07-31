/* ------------------------------------------------------------------------- */
/* acq400_axi_oneshot.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 29 Jul 2017  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2017 Peter Milne, D-TACQ Solutions Ltd         *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO 
 * TODO
 * ------------------------------------------------------------------------- */

/* AXI DMA Example
*
* This small example is intended to simply llustrate how to use the DMA engine
* of Linux to take advantage of DMA in the PL. The hardware design is intended
* to be an AXI DMA without scatter gather and with the transmit channel looped
* back to the receive channel.
* https://forums.xilinx.com/t5/Embedded-Linux/AXI-DMA-with-Zynq-Running-Linux/m-p/523105#M10658
*/

#include "acq400.h"
#include "dmaengine.h"
#include "hbm.h"

int AXIDMA_ONCE_TO_MSEC = 1000;
module_param(AXIDMA_ONCE_TO_MSEC, int, 0644);
MODULE_PARM_DESC(AXIDMA_ONCE_TO_MSEC, "timeout for transferrining 4MB at 800MB/s");


int AXIDMA_ONCE_BUSY;
module_param(AXIDMA_ONCE_BUSY, int, 0044);
MODULE_PARM_DESC(AXIDMA_ONCE_BUSY, "TRUE when ONCE in progress");

int AXIDMA_ONCE_RESET_ON_EXIT;
module_param(AXIDMA_ONCE_RESET_ON_EXIT, int , 0644);

#define WAIT 	1
#define NO_WAIT 0

static void axidma_sync_callback(void *completion)
{
	/* Step 9, indicate the DMA transaction completed to allow the other
	 * thread of control to finish processing
	 */

	complete(completion);

}

/* Prepare a DMA buffer to be used in a DMA transaction, submit it to the DMA engine
 * to queued and return a cookie that can be used to track that status of the
 * transaction
 */
static dma_cookie_t axidma_prep_buffer(struct dma_chan *chan, dma_addr_t buf, size_t len,
					enum dma_transfer_direction dir, struct completion *cmp)
{
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	struct dma_async_tx_descriptor *chan_desc;
	dma_cookie_t cookie;

	/* Step 5, create a buffer (channel)  descriptor for the buffer since only a
	 * single buffer is being used for this transfer
	 */

	chan_desc = dmaengine_prep_slave_single(chan, buf, len, dir, flags);

	/* Make sure the operation was completed successfully
	 */
	if (!chan_desc) {
		printk(KERN_ERR "dmaengine_prep_slave_single error\n");
		cookie = -EBUSY;
	} else {
		chan_desc->callback = axidma_sync_callback;
		chan_desc->callback_param = cmp;

		/* Step 6, submit the transaction to the DMA engine so that it's queued
		 * up to be processed later and get a cookie to track it's status
		 */

		cookie = dmaengine_submit(chan_desc);

	}
	return cookie;
}

/* Start a DMA transfer that was previously submitted to the DMA engine and then
 * wait for it complete, timeout or have an error
 */
static void axidma_start_transfer(struct dma_chan *chan, struct completion *cmp,
					dma_cookie_t cookie, int wait)
{
	unsigned long timeout = msecs_to_jiffies(AXIDMA_ONCE_TO_MSEC);
	enum dma_status status;

	/* Step 7, initialize the completion before using it and then start the
	 * DMA transaction which was previously queued up in the DMA engine
	 */

	init_completion(cmp);
	dma_async_issue_pending(chan);

	if (wait) {
		dev_dbg(&chan->dev->device, "Waiting for DMA to complete...");

		/* Step 8, wait for the transaction to complete, timeout, or get
		 * get an error
		 */

		timeout = wait_for_completion_timeout(cmp, timeout);
		status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);

		/* Determine if the transaction completed without a timeout and
		 * without any errors
		 */
		if (timeout == 0)  {
			dev_dbg(&chan->dev->device, "DMA timed out");
		}else if (status != DMA_COMPLETE) {
			dev_dbg(&chan->dev->device,  "DMA returned completion callback status of: %s",
			       status == DMA_ERROR ? "error" : "in progress");
		}else{
			dev_dbg(&chan->dev->device, "DMA GOOD");
		}
	}
}

extern int xilinx_dma_reset_dmachan(struct dma_chan *chan);

int _axi64_data_once(struct acq400_dev *adev)
{
	struct dma_chan *rx_chan = adev->dma_chan[0];
	char *dest_dma_buffer = (char*)adev->hb[0]->va;
	int dma_length = adev->hb[0]->len;
	dma_addr_t rx_dma_handle = dma_map_single(
			rx_chan->device->dev, dest_dma_buffer, dma_length, DMA_FROM_DEVICE);
	struct completion rx_cmp;

	dma_cookie_t rx_cookie = axidma_prep_buffer(
			rx_chan, rx_dma_handle, dma_length, DMA_DEV_TO_MEM, &rx_cmp);
	if (dma_submit_error(rx_cookie)) {
		printk(KERN_ERR "xdma_prep_buffer error\n");
		return -1;
	}
	axidma_start_transfer(rx_chan, &rx_cmp, rx_cookie, WAIT);
	dma_unmap_single(rx_chan->device->dev, rx_dma_handle, dma_length, DMA_FROM_DEVICE);

	dmaengine_terminate_all(adev->dma_chan[0]);
	if (AXIDMA_ONCE_RESET_ON_EXIT){
		xilinx_dma_reset_dmachan(adev->dma_chan[0]);
	}
	return 0;
}

/* @@todo .. nasty use of global - what if more than one channel ..
 * bad Linux, but effective here as it's a singleton */
int axi64_data_once(struct acq400_dev *adev)
{
	int rc;
	AXIDMA_ONCE_BUSY = 1;
	rc = _axi64_data_once(adev);
	AXIDMA_ONCE_BUSY = 0;
	return 0;
}

