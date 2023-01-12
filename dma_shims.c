/*
 * dma_shims.c
 *
 *  Created on: 16 Feb 2021
 *      Author: pgm
 */

#include "acq400.h"
#include "dmaengine.h"


#define DMA_NS_MAX     40
int dma_ns_lines[DMA_NS_MAX];
int dma_ns[DMA_NS_MAX];
int dma_ns_num = DMA_NS_MAX;
module_param_array(dma_ns, int, &dma_ns_num, 0444);
module_param_array(dma_ns_lines, int, &dma_ns_num, 0444);

#if 0
#define DMA_NS_INIT \
	do { 					\
		ins=0; t0 = otick(); 		\
	} while(0)

#define DMA_NS \
	do { 						\
		dma_ns_lines[ins] = __LINE__; 		\
		dma_ns[ins] = delta_nsec(t0, otick()); 	\
		ins++; 					\
		BUG_ON(ins >= DMA_NS_MAX);		\
	} while(0)


void timer_test(void)
{
	DMA_NS_INIT;

	while(ins+1 < DMA_NS_MAX){
		DMA_NS;
	}
}
#define DMA_NS_TEST	timer_test()
#else
#define DMA_NS_INIT
#define DMA_NS
#define DMA_NS_TEST
#endif


void acq400_dma_callback(void *param)
{
	struct acq400_dev* adev = (struct acq400_dev*)param;
	adev->dma_callback_done++;
	wake_up_interruptible(&adev->DMA_READY);
}


dma_cookie_t
dma_async_memcpy_callback(
	struct dma_chan *chan,
	dma_addr_t dma_dst, dma_addr_t dma_src,
	size_t len, unsigned long flags,
	dma_async_tx_callback callback,
	void *callback_param)
{
	struct dma_device *dev = chan->device;
	struct dma_async_tx_descriptor *tx;
	flags |=  DMA_CTRL_ACK;

	DMA_NS;
	dev_dbg(dev->dev, "dev->prep_dma_memcpy %d 0x%08x 0x%08x %d %08lx",
			chan->chan_id, dma_dst, dma_src, len, flags);
	tx = dev->device_prep_dma_memcpy(chan, dma_dst, dma_src, len, flags);

	DMA_NS;
	if (!tx) {
		dev_err(dev->dev, "prep failed");
		return -ENOMEM;
	} else{
		dma_cookie_t cookie;
		tx->callback = callback;
		tx->callback_param = callback_param;
		cookie = tx->tx_submit(tx);
		dev_dbg(dev->dev, "submit(%d) done %x", chan->chan_id, cookie);
		DMA_NS;

		preempt_disable();
		__this_cpu_add(chan->local->bytes_transferred, len);
		__this_cpu_inc(chan->local->memcpy_count);
		preempt_enable();
		return cookie;
	}
}

dma_cookie_t
dma_async_memcpy(
	struct dma_chan *chan, dma_addr_t src, 	dma_addr_t dest, size_t len)
{
	struct dma_device *dev = chan->device;
	struct dma_async_tx_descriptor *tx;
	unsigned long flags = DMA_DST_NO_INCR | DMA_CTRL_ACK;

	DMA_NS;
	tx = dev->device_prep_dma_memcpy(chan, dest, src, len, flags);

	DMA_NS;
	if (!tx) {
		return -ENOMEM;
	} else{
		dma_cookie_t cookie;
		tx->callback = NULL;
		cookie = tx->tx_submit(tx);

		DMA_NS;

		preempt_disable();
		__this_cpu_add(chan->local->bytes_transferred, len);
		__this_cpu_inc(chan->local->memcpy_count);
		preempt_enable();
		return cookie;
	}
}

int dma_memcpy(
	struct acq400_dev* adev, dma_addr_t dest, dma_addr_t src, size_t len)
{
	dma_cookie_t cookie;
	DMA_NS_INIT;
	DMA_NS;
	if (adev->dma_chan[0] == 0){
		dev_err(DEVP(adev), "%p id:%d dma_find_channel set zero",
				adev, adev->pdev->dev.id);
		return -1;
	}
	dev_dbg(DEVP(adev), "dma_memcpy() chan:%d src:%08x dest:%08x len:%d\n",
			adev->dma_chan[0]->chan_id, src, dest, len);
	cookie = dma_async_memcpy(adev->dma_chan[0], src, dest, len);
	dev_dbg(DEVP(adev), "dma_memcpy() wait cookie:%d\n", cookie);
	dma_sync_wait(adev->dma_chan[0], cookie);
	dev_dbg(DEVP(adev), "dma_memcpy() wait cookie:%d done\n", cookie);
	DMA_NS;
	return len;
}

