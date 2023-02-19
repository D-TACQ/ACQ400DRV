/* ------------------------------------------------------------------------- */
/* acq400_drv.c  D-TACQ ACQ400 FMC  DRIVER		                     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2013 Peter Milne, D-TACQ Solutions Ltd                    *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include "acq400.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"
#include "acq400_ui.h"

#include "dmaengine.h"


#define REVID 			"3.756"
#define MODULE_NAME             "acq420"

/* Define debugging for use during our driver bringup */
#undef PDEBUG
#define PDEBUG(fmt, args...) printk(KERN_INFO fmt, ## args)


/* MODULE PARAMETERS */


int ndevices;
/*
static char* revid = REVID;

module_param(revid, charp, 0666);
*/

static char* revid = REVID;
module_param(revid, charp, 0644);


module_param(ndevices, int, 0444);
MODULE_PARM_DESC(ndevices, "number of devices found in probe");


int nbuffers = 64;
module_param(nbuffers, int, 0444);
MODULE_PARM_DESC(nbuffers, "number of capture buffers");

int bufferlen = 0x10000;
module_param(bufferlen, int, 0444);
MODULE_PARM_DESC(bufferlen, "length of capture buffer");

int frontside_bufferlen = 0x10000;
module_param(frontside_bufferlen, int, 0444);


int FIFERR = 0; 				/* ADC_FIFO_STA_ERR; */
module_param(FIFERR, int, 0644);
MODULE_PARM_DESC(FIFERR, "fifo status flags considered ERROR");

int first_axi_channel = 0;
module_param(first_axi_channel, int, 0644);
MODULE_PARM_DESC(first_axi_channel, "in 2D, this channel goes first [0] ");

int maxdma = MAXDMA;
module_param(maxdma, int, 0644);
MODULE_PARM_DESC(maxdma, "set maximum DMA len bytes");

int agg_reset_dbg = 0;
module_param(agg_reset_dbg, int, 0644);

int quit_on_buffer_exhaustion;
module_param(quit_on_buffer_exhaustion, int, 0644);
MODULE_PARM_DESC(quit_on_buffer_exhaustion, "abort capture when out of buffers");



int AO420_MAX_FILL_BLOCK = 16384;
module_param(AO420_MAX_FILL_BLOCK, int, 0644);

int BQ_LEN_SHL = 2;
module_param(BQ_LEN_SHL, int, 0644);

int hb0_no_ratelimit = 0;
module_param(hb0_no_ratelimit, int, 0644);

/* PIG TEST. SLAVE has non zero settings */
int sideport_does_not_touch_trg = 0;
module_param(sideport_does_not_touch_trg, int, 0644);

int default_dma_direction = DMA_FROM_DEVICE;
module_param(default_dma_direction, int , 0644);
MODULE_PARM_DESC(default_dma_direction, "set=1 for XO only device");

int xo_use_bigbuf = 1;
module_param(xo_use_bigbuf, int, 0644);
MODULE_PARM_DESC(xo_use_bigbuf, "set=1 if ONLY XO in box, then use bb to load long AWG");

int xo_use_contiguous_pa_if_possible = 0;
module_param(xo_use_contiguous_pa_if_possible, int, 0644);
MODULE_PARM_DESC(xo_use_contiguous_pa_if_possible, "set=1 to roll forward into next HB if contiguous");


int distributor_first_buffer = 0;
module_param(distributor_first_buffer, int, 0644);
MODULE_PARM_DESC(distributor_first_buffer, "use in mixed aggregator/distributor systems to avoid buffer overlap");

int reserve_buffers = 2;
module_param(reserve_buffers, int, 0444);
MODULE_PARM_DESC(reserve_buffers, "buffers held out of shot, used post shot data start");


int event0_feeds_ev_device = 1;
module_param(event0_feeds_ev_device, int, 0444);
MODULE_PARM_DESC(event0_feeds_ev_device, "report event0 to clients");

int event1_feeds_ev_device = 0;
module_param(event1_feeds_ev_device, int, 0444);
MODULE_PARM_DESC(event1_feeds_ev_device, "report event1 to clients");

unsigned event_status_mask;
module_param(event_status_mask, int, 0444);
MODULE_PARM_DESC(event_status_mask, "actual mask used in event_action()");

int DMA_TIMEOUT = 10001;
module_param(DMA_TIMEOUT, int, 0644);
MODULE_PARM_DESC(DMA_TIMEOUT, "default DMA TIMEOUT in jiffies");

int subrate_nmax = 1;
module_param(subrate_nmax, int, 0444);
MODULE_PARM_DESC(subrate_nmax, "number of subrate outputs to average over [max 256]");

/* GLOBALS */

/* driver supports multiple devices.
 * ideally we'd have no globals here at all, but it works, for now
 */

/* index from 0, including site 0 */
struct acq400_dev* acq400_devices[MAXDEVICES+1];
/* index by site */
struct acq400_dev* acq400_sites[MAXDEVICES+1];


int OPEN_BACKLOG = 0;
module_param(OPEN_BACKLOG, int, 0644);
MODULE_PARM_DESC(OPEN_BACKLOG, "allow backlog of open descriptors before put eg for SPY task to operate");


int good_sites[MAXDEVICES];
int good_sites_count = 0;
module_param_array(good_sites, int, &good_sites_count, 0444);

int ao420_dma_threshold = 32000;		/* DMA NFG, replace MIN_DMA_BYTES with est size of FIFO */
module_param(ao420_dma_threshold, int, 0644);
MODULE_PARM_DESC(ao420_dma_threshold, "use DMA for transfer to AO [set 999999 to disable]");

int min_dma_bytes = 32000;			/* FRONTSIDE DMA NFG, replace MIN_DMA_BYTES with est size of FIFO */
module_param(min_dma_bytes, int, 0644);
MODULE_PARM_DESC(min_dma_bytes, "threshold for using DMA not PIO for AO frontside");

int event_isr_msec = 20;
module_param(event_isr_msec, int, 0644);
MODULE_PARM_DESC(event_isr_msec, "event isr poll rate (remove when actual interrupt works)");

#define AO420_BUFFERLEN	0x100000

int ao420_buffer_length = AO420_BUFFERLEN;
module_param(ao420_buffer_length, int, 0644);
MODULE_PARM_DESC(ao420_buffer_length, "AWG buffer length");

#define AO424_BUFFERLEN	0x400000		/* HACK biggest buffer we can get */
int ao424_buffer_length = AO424_BUFFERLEN;
module_param(ao424_buffer_length, int, 0644);
MODULE_PARM_DESC(ao424_buffer_length, "AWG buffer length");



int is_acq2106B = 0;
module_param(is_acq2106B, int, 0444);
MODULE_PARM_DESC(is_acq2106B, "boolean indicator set on load");


int AXI_BUFFER_COUNT = 512;
module_param(AXI_BUFFER_COUNT, int, 0644);
MODULE_PARM_DESC(AXI_BUFFER_COUNT, "number of buffers in AXI cycle");


int AXI_BUFFER_CHECK_TICKS = 100;
module_param(AXI_BUFFER_CHECK_TICKS, int , 0644);
MODULE_PARM_DESC(AXI_BUFFER_CHECK_TICKS, "AXI buffer poll timeout");

int AXI_CALL_HELPER = 1;
module_param(AXI_CALL_HELPER, int, 0644);
MODULE_PARM_DESC(AXI_CALL_HELPER, "call helper to set up DMA chain else, someone else did it");

int AXI_DMA_HAS_CATCHUP = 1;
module_param(AXI_DMA_HAS_CATCHUP, int, 0644);
MODULE_PARM_DESC(AXI_DMA_HAS_CATCHUP, "catchup backlog on same tick");

int sync_continuous = 1;
module_param(sync_continuous, int, 0644);
MODULE_PARM_DESC(sync_continuous, "set zero to stub dma sync on continuous read (experiment)");


int AXI_ONESHOT = 0;
module_param(AXI_ONESHOT, int, 0644);
MODULE_PARM_DESC(AXI_ONESHOT, "axi DMA once through, non-zero count is number of buffers");

int AXI_DEBUG_LOOPBACK_INDEX = 0;
module_param(AXI_DEBUG_LOOPBACK_INDEX, int, 0644);
MODULE_PARM_DESC(AXI_POISON_OFFSET, "DEBUG: set non zero to skip first buffers on loopback. so that we see first time contents..");


int dtd_pulse_nsec = 1000000;
module_param(dtd_pulse_nsec, int, 0644);

int dtd_display_pulse_nsec = 200000000;
module_param(dtd_display_pulse_nsec, int, 0644);



int xo_use_distributor = 1;
module_param(xo_use_distributor, int, 0644);
MODULE_PARM_DESC(xo_use_distributor, "use distributor and PRI for XO transfer";)



int continuous_readers[MAX_PHYSICAL_SITES+1];
int max_continuous_readers = MAX_PHYSICAL_SITES+1;
module_param_array(continuous_readers, int, &max_continuous_readers, 0444);
MODULE_PARM_DESC(continuous_readers, "array index from 1 shows which sites have a reader");

/** @@TODO: chances are the below is a bogus duplicate of AXI_ONESHOT */
int axi_oneshot = 0;
module_param(axi_oneshot, int, 0644);
MODULE_PARM_DESC(axi_oneshot, "one-shot: don't poison recycled buffers");


int firstDistributorBuffer(void)
{
	return distributor_first_buffer + reserve_buffers;
}
int lastDistributorBuffer(void)
{
	return nbuffers - 1;
}

#define AO420_NBUFFERS 	2


#define ACQ420_NBUFFERS	16
#define ACQ420_BUFFERLEN frontside_bufferlen

/* SC uses DMA with peripheral control */
#define SC_PRI	0		/* use this peripheral 		*/
#define SC_EV	8		/* use this event (and +1) 	*/

#define DMA_SC_FLAGS \
	(SC_EV << DMA_CHANNEL_EV0_SHL        | \
	 DMA_SRC_NO_INCR | \
	 PRI_TO_CTRL_FLAGS(SC_PRI) << DMA_CHANNEL_STARTS_WFP_SHL | \
	 PRI_TO_CTRL_FLAGS(SC_PRI) << DMA_CHANNEL_ENDS_FLUSHP_SHL)





int ai_data_loop(void *data);
int axi64_data_loop(void* data);
int axi64_dual_data_loop(void* data);
int fifo_monitor(void* data);

int fiferr;


int acq400_reserve_dist_buffers(struct acq400_path_descriptor* pd)
{
	if (distributor_first_buffer > 0){
		int ib;
		int rc = 0;
		for (ib = distributor_first_buffer; ib < nbuffers; ++ib){
			rc = reserve(pd, ib);
			if (rc != 0){
				dev_err(&pd->dev->pdev->dev,
					"failed to reserve buffer %d", ib);
				return rc;
			}
		}
		return 0;
	}
	return 0;	/* distributor_first_buffer==0 is an option */
}

int acq400_free_buffers(struct acq400_dev *adev, int free_from)
{
	struct acq400_path_descriptor* ppd;
	struct list_head rlist;
	int freemax = nbuffers;
	int nfree = 0;
	int ib;
	int rc = 0;

	acq400_init_descriptor(&ppd);
	ppd->dev = adev;
	INIT_LIST_HEAD(&rlist);
	if (distributor_first_buffer > 0){
		freemax = distributor_first_buffer;
	}

	for (ib = free_from; ib < freemax; ++ib){
		rc = remove(ppd, ib, &rlist);
		if (rc != 0){
			dev_err(DEVP(adev), "failed to reserve buffer %d", ib);
		}else{
			dev_dbg(DEVP(adev), "removing buffer %d", ib);
			++nfree;
		}
	}
	hbm_free_buffer_only(DEVP(adev), &rlist);
	dev_info(DEVP(adev), "%s dumped %d buffers, PLEASE REBOOT before capture", __FUNCTION__, nfree);
	kfree(ppd);
	return 0;
}
#define GS_DBG(...)
//#define GS_DBG dev_info
int isGoodSite(int site)
{
	GS_DBG(0, "%s %d site %d", __FUNCTION__, __LINE__, site);
	if (site == 0){
		GS_DBG(0, "%s %d site %d", __FUNCTION__, __LINE__, site);
		return 1;
	}else if (site < 0 || site%100 > MAX_PHYSICAL_SITES ){
		GS_DBG(0, "%s %d site %d", __FUNCTION__, __LINE__, site);
		return 0;
	}else{
		int ii;
		for (ii = 0; ii < good_sites_count; ++ii){
			if (good_sites[ii] == site){
				GS_DBG(0, "%s %d site %d", __FUNCTION__, __LINE__, site);
				return 1;
			}
		}
		GS_DBG(0, "%s %d site %d", __FUNCTION__, __LINE__, site);
		return 0;
	}
}

void set_continuous_reader(struct acq400_dev *adev)
{
	adev->continuous_reader = task_pid_nr(current);
	continuous_readers[adev->of_prams.site] = adev->of_prams.site;
}

void clr_continuous_reader(struct acq400_dev *adev)
{
	adev->continuous_reader = 0;
	continuous_readers[adev->of_prams.site] = 0;
}




void acqXXX_onStartNOP(struct acq400_dev *adev) {}
void acqXXX_onStopNOP(struct acq400_dev *adev)  {}



unsigned long long t0;
int ins;

/** @@todo ... hook to ZYNQ timers how? */

extern unsigned long long otick(void);
extern unsigned delta_nsec(unsigned long long t0, unsigned long long t1);








/* File operations */
int acq400_open_main(struct inode *inode, struct file *file)
{
        int rc = 0;
        struct acq400_dev* adev = ACQ400_DEV(file);

        if (mutex_lock_interruptible(&adev->mutex)) {
                return -ERESTARTSYS;
        }

        /* We're only going to allow one write at a time, so manage that via
         * reference counts
         */
        switch (file->f_flags & O_ACCMODE) {
        case O_RDONLY:
                break;
        case O_WRONLY:
                if (adev->writers || adev->busy) {
                        rc = -EBUSY;
                        goto out;
                } else {
                        adev->writers++;
                }
                break;
        case O_RDWR:
        default:
                if (adev->writers || adev->busy) {
                        rc = -EBUSY;
                        goto out;
                } else {
                        adev->writers++;
                }
        }

        adev->stats.opens++;

out:
        mutex_unlock(&adev->mutex);
        return rc;
}


int _acq420_continuous_start_dma(struct acq400_dev *adev)
{
	int rc = 0;
	int pollcat = 0;
	if (mutex_lock_interruptible(&adev->mutex)) {
		return -EINTR;
	}

	if (adev->busy || get_dma_channels(adev)){
		if (adev->busy){
			dev_err(DEVP(adev), "BUSY");
		}else{
			dev_err(DEVP(adev), "no dma chan");
		}
		rc = -EBUSY;
		mutex_unlock(&adev->mutex);
		return rc;
	}
	adev->dma_callback_done = 0;
	adev->fifo_isr_done = 0;
	mutex_unlock(&adev->mutex);

	dev_dbg(DEVP(adev), "acq420_continuous_start() %p id:%d : dma_chan: %p",
			adev, adev->pdev->dev.id, adev->dma_chan);

	if (adev->w_task == 0){
		int retry = 0;
		for(; IS_ERR_OR_NULL(adev->w_task); ++retry){
			dev_dbg(DEVP(adev), "acq420_continuous_start() kthread_run()");
			adev->w_task = kthread_run(
					IS_AXI64_DUALCHAN(adev)? axi64_dual_data_loop:
							IS_AXI64(adev)? axi64_data_loop:
							ai_data_loop,
					adev, "%s.ai", adev->dev_name);
			if (IS_ERR_OR_NULL(adev->w_task)){
				dev_err(DEVP(adev), "ERROR: failed to start task %p", adev->w_task);
				if (++retry > 5){
					BUG();
				}
				msleep(10*retry);
			}else{
				break;
			}
		}
	}else{
		dev_warn(DEVP(adev),
				"acq420_continuous_start() task already running ?\n");
	}

	if (adev->h_task == 0){
		adev->h_task = kthread_run(fifo_monitor, adev, "%s.fm", adev->dev_name);
	}
	while(!adev->task_active){
		msleep(10);
		if ((++pollcat&0x1f) == 0){
			dev_warn(DEVP(adev), "Polling for task active");
			if (pollcat > 200){
				dev_err(DEVP(adev), "ERROR: task_active not happening return ERR do not set busy");
				return -1;
			}
		}
	}
	adev->busy = 1;
	return rc;
}


int _onStart(struct acq400_dev *adev)
{
	if (mutex_lock_interruptible(&adev->mutex)) {
		return -EINTR;
	}

	adev->stats.shot++;
	adev->stats.run = 1;
	memset(&adev->rt, 0, sizeof(struct RUN_TIME));
	acq400_clear_histo(adev);
	mutex_unlock(&adev->mutex);
	return 0;
}
int _acq420_continuous_start(struct acq400_dev *adev, int dma_start)
{
	int rc;
	dev_dbg(DEVP(adev), "_acq420_continuous_start() 01 dma_start:%d", dma_start);
	rc = _onStart(adev);
	if (rc){
		return rc;
	}
	if (dma_start){
		int rc =_acq420_continuous_start_dma(adev);
		if (rc != 0){
			return rc;
		}
	}

	adev->onStart(adev);
	dev_dbg(DEVP(adev), "_acq420_continuous_start() 99");
	return 0;
}

int acq420_continuous_start(struct inode *inode, struct file *file)
{
	return _acq420_continuous_start(ACQ400_DEV(file), 1);
}


int acq400_set_bufferlen(struct acq400_dev *adev, int _bufferlen)
{
	/* client may request sub-buffer size eg to ensure data align
	 * 96 x 4 channels, best length = 1044480
	 */
	if (_bufferlen){
		adev->bufferlen = min(bufferlen, _bufferlen);
	}else{
		adev->bufferlen = bufferlen;
	}

	return adev->bufferlen;
}

extern int acq400_set_dist_bufferlen(struct acq400_dev *adev, int _bufferlen)
{
	int dbl;
	int ib;
	if (_bufferlen){
		dbl = min(bufferlen, _bufferlen);
	}else{
		dbl = bufferlen;
	}
	for (ib = distributor_first_buffer; ib < nbuffers; ++ib){
		adev->hb[ib]->len = dbl;
	}
	return dbl;
}
extern int acq400_get_dist_bufferlen(struct acq400_dev *adev)
{
	return adev->hb[distributor_first_buffer]->len;
}
int acq2006_continuous_start(struct inode *inode, struct file *file)
/* this sequence CRITICAL for a clean start
 * (1) reset the aggregator, leaving it disabled
 * (2) reset the data engine, leaving it enabled (0xf1000000 : waiting flush)
 * (3) enable the DMA, sends the flush, data engine => 0xf3000000
 * (4) enable the aggregator
 * (5) now enable the trigger.
 */
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int rc;

	empty_lists(adev);
	dev_dbg(DEVP(adev), "acq2006_continuous_start() 01");
	fiferr = FIFERR;
	_onStart(adev);
	adev->RW32_debug = agg_reset_dbg;

	acq2106_aggregator_reset(adev);				/* (1) */
	sc_data_engine_reset_enable(DATA_ENGINE_0);		/* (2) */

	if (agg_reset_dbg) dev_info(DEVP(adev), "start dma");
	rc =_acq420_continuous_start_dma(adev);			/* (3) */
	if (rc != 0){
		dev_info(DEVP(adev), "acq2006_continuous_start() 66");
		return rc;
	}


	acq2006_aggregator_enable(adev);			/* (4) */

	dev_dbg(DEVP(adev), "acq2006_continuous_start() acq400_enable_trg %d",
			sc_dev->aggregator_set[0]->of_prams.site);

	/* (5) */

	acq400_visit_set(sc_dev->aggregator_set, acq400_enable_trg_if_master);
	adev->RW32_debug = 0;
	dev_dbg(DEVP(adev), "acq2006_continuous_start() 99");
	return 0;
}


void acq400_bq_notify(struct acq400_dev *adev, struct HBM *hbm)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct BQ_Wrapper* bqw = &sc_dev->bqw;
	struct acq400_path_descriptor *cur;
	struct acq400_path_descriptor *tmp;
	int ix = hbm->ix;
	int nelems = 0;
	int th = nbuffers - 2;

	mutex_lock(&sc_dev->bqw.bq_clients_mutex);

	/* _safe should not be needed since we're mutexed, but .. */
	list_for_each_entry_safe(cur, tmp, &bqw->bq_clients, bq_list){
		struct BQ* bq = &cur->bq;
		int nq = CIRC_CNT(bq->head, bq->tail, bq->bq_len);
		if (BQ_space(bq) < 1 || nq > th){
			bq->head = bq->tail = 0;
			++bqw->bq_overruns;
		}
		BQ_put(DEVP(adev), bq, ix);

		wake_up_interruptible(&cur->waitq);
		++nelems;
		if (nq > bqw->bq_max) bqw->bq_max = nq;
	}
	mutex_unlock(&bqw->bq_clients_mutex);
	dev_dbg(DEVP(adev), "acq400_bq_notify() nelems:%d", nelems);
}


int over_open_backlog(struct acq400_dev *adev)
{
	if (list_empty(&adev->OPENS)){
		return 0;
	}else if (OPEN_BACKLOG == 0){
		return 1;
	}else{
		return list_depth(&adev->OPENS) >= OPEN_BACKLOG;
	}
}


ssize_t acq400_continuous_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
/* NB: waits for a full buffer to ARRIVE, but only returns the 2 char ID */
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	char* lbuf = PD(file)->lbuf;
	int rc = 0;
	struct HBM *hbm;
	int nread;
	unsigned long now;


	adev->stats.reads++;

	set_continuous_reader(adev);

	if (adev->rt.please_stop){
		return -1;		/* EOF ? */
	}
	while (over_open_backlog(adev)){
		putEmpty(adev);
	}

	dev_dbg(DEVP(adev), "acq400_continuous_read():getFull()");

	switch((rc = getFull(adev, &hbm, GF_WAIT))){
	case GET_FULL_OK:
		break;
	case GET_FULL_DONE:
		dev_warn(DEVP(adev), "finished");
		return -1;		/* finished  ret 0 WBN? */
	case GET_FULL_REFILL_ERR:
		dev_warn(DEVP(adev), "refill error\n");
		return -1;
	default:
		dev_warn(DEVP(adev), "interrupted\n");
		return rc;
	}

	if (list_empty(&adev->OPENS)){
		dev_warn(DEVP(adev), "no buffer available");
		return -1;
	}


	dev_dbg(DEVP(adev), "acq400_continuous_read():getFull() : %d", hbm->ix);

	/* update every hb0 or at least once per second */
	now = get_seconds();
	/* ratelimited to 1Hz - client gets current and previous hbm
	 * set hb0_no_rate_limit negative to increase this ..
	 */
	if (adev->rt.hbm_m1 != 0 && (now + hb0_no_ratelimit) > adev->rt.hb0_last){
		adev->rt.hb0_count++;
		adev->rt.hb0_ix[0] = adev->rt.hbm_m1->ix;
		adev->rt.hb0_ix[1] = hbm->ix;
		adev->rt.hb0_last = now;
		dev_dbg(DEVP(adev), "hb0 %d now:%lu", adev->rt.hb0_count, now);
		dma_sync_single_for_cpu(DEVP(adev), hbm->pa, hbm->len, hbm->dir);
		wake_up_interruptible(&adev->hb0_marker);
	}else if (sync_continuous){
		/* this operation is surprisingly expensive. Use sparingly */
		dma_sync_single_for_cpu(DEVP(adev), hbm->pa, hbm->len, hbm->dir);
	}

	nread = sprintf(lbuf, "%02d\n", hbm->ix);
	adev->rt.hbm_m1 = hbm;

	acq400_bq_notify(adev, hbm);
#if 0
	/* pick off any other queued buffers ..
	 * enable only when acq400_stream can handle multiple responses..
	 */
	while(getFull(adev, &hbm, GF_NOWAIT) == GET_FULL_OK){
		acq400_bq_notify(adev, hbm);
		nread += sprintf(lbuf+nread, "%02d\n", hbm->ix);
		if (nread + 10 > MAXLBUF){
			break;
		}
	}
#endif
	rc = copy_to_user(buf, lbuf, nread);
	return rc? -rc: nread;
}



void _acq420_continuous_dma_stop(struct acq400_dev *adev)
{
	unsigned long work_task_wait = 0;

	while(mutex_lock_interruptible(&adev->mutex)) {
		;
	}
	if (adev->task_active && !IS_ERR_OR_NULL(adev->w_task)){
		kthread_stop(adev->w_task);
	}

	if (adev->h_task != 0){
		kthread_stop(adev->h_task);
		adev->h_task = 0;
	}
	mutex_unlock(&adev->mutex);

	wake_up_interruptible(&adev->DMA_READY);
	wake_up_interruptible(&adev->w_waitq);

	while(adev->task_active){
		work_task_wait = jiffies + msecs_to_jiffies(1000);
		while (adev->task_active){
			yield();
			if (time_after_eq(jiffies, work_task_wait)){
				dev_warn(DEVP(adev), "WAITING for work task\n");
				break;
			}
		}
	}
	mutex_lock(&adev->mutex);
	adev->w_task = 0;
	mutex_unlock(&adev->mutex);

	move_list_to_empty(adev, &adev->OPENS);
	move_list_to_empty(adev, &adev->REFILLS);
	move_list_to_empty(adev, &adev->INFLIGHT);
	adev->busy = 0;

	release_dma_channels(adev);
	adev->stats.run = 0;
}

void _acq420_continuous_stop(struct acq400_dev *adev, int dma_stop)
{
	adev->onStop(adev);

	dev_dbg(DEVP(adev), "acq420_continuous_stop() kthread_stop called\n");

	if (dma_stop){
		_acq420_continuous_dma_stop(adev);
	}else{
		adev->stats.run = 0;
	}
	adev->stats.completed_shot = adev->stats.shot;
	if (acq400_event_count_limit &&
		adev->rt.event_count >= acq400_event_count_limit){
		acq400_enable_event0(adev, 1);
	}
	dev_dbg(DEVP(adev), "acq420_continuous_stop(): quitting ctrl:%08x",
			acq400rd32(adev, ADC_CTRL));
}
int acq420_continuous_stop(struct inode *inode, struct file *file)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	fiferr = 0;				/* don't care about any errs now */
	dev_dbg(DEVP(adev), "acq420_continuous_stop() fiferr clr from %08x", FIFERR);
	clr_continuous_reader(adev);
	_acq420_continuous_stop(ACQ400_DEV(file), 1);

	return acq400_release(inode, file);
}

int acq2006_continuous_stop(struct inode *inode, struct file *file)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	struct acq400_dev *adev1 = acq400_devices[1];

	fiferr = 0;				/* don't care about any errs now */
	dev_dbg(DEVP(adev), "acq2006_continuous_stop() fiferr clr from %08x", FIFERR);
	sc_data_engine_disable(DATA_ENGINE_0);
	acq2006_aggregator_disable(adev);
	_acq420_continuous_dma_stop(adev);
	clr_continuous_reader(adev);

	if (adev->rt.event_count >= acq400_event_count_limit){
		dev_dbg(DEVP(adev), "acq2006_continuous_stop() restore event..");
		acq400_enable_event0(adev, 1);
	}


	dev_info(DEVP(adev), "shot complete %d", adev1? adev1->stats.shot: adev->stats.shot );
	return acq400_release(inode, file);
}

void acq2006_estop(struct acq400_dev *adev)
{
	fiferr = 0;				/* don't care about any errs now */

	dev_dbg(DEVP(adev), "acq2006_estop() fiferr clr from %08x", FIFERR);
	acq2006_aggregator_disable(adev);
	_acq420_continuous_dma_stop(adev);
	dev_dbg(DEVP(adev), "acq2006_estop() 99");
}

static unsigned int acq420_continuous_poll(
	struct file *file, struct poll_table_struct *poll_table)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	int rc = 0;
	int wait = 0;

	if (!list_empty(&adev->REFILLS)){
		rc = POLLIN|POLLRDNORM;
	}else if (adev->rt.refill_error){
		rc = POLLERR;
	}else if (adev->rt.please_stop){
		rc = POLLHUP;
	}else{
		wait = 1;
		poll_wait(file, &adev->refill_ready, poll_table);
		if (!list_empty(&adev->REFILLS)){
			rc = POLLIN|POLLRDNORM;
		}else if (adev->rt.refill_error){
			rc = POLLERR;
		}else if (adev->rt.please_stop){
			rc = POLLHUP;
		}
	}
	dev_dbg(DEVP(adev), "acq420_continuous_poll() %d return %d", wait, rc);
	return rc;
}


int acq420_open_continuous(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_continuous = {
			.open = acq420_continuous_start,
			.read = acq400_continuous_read,
			.release = acq420_continuous_stop,
			.poll = acq420_continuous_poll
	};
	static struct file_operations acq2006_fops_continuous = {
			.open = acq2006_continuous_start,
			.read = acq400_continuous_read,
			.release = acq2006_continuous_stop,
			.poll = acq420_continuous_poll
	};
	struct acq400_dev *adev = ACQ400_DEV(file);
	int rc = acq400_open_main(inode, file);
	if (rc){
		return rc;
	}
	if (IS_SC(adev)){
		file->f_op = &acq2006_fops_continuous;
	}else{
		file->f_op = &acq400_fops_continuous;
	}
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}

int acq420_sideported_start(struct inode *inode, struct file *file)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	int rc;
	int tmp = adev->RW32_debug;
	adev->RW32_debug = agg_reset_dbg;
	dev_dbg(DEVP(adev), "acq420_sideported_start()");


	if (sideport_does_not_touch_trg == 0){
		adev->onStop(adev);
		acq400_enable_trg(adev, 0);
	}
	rc = _acq420_continuous_start(adev, 0);

	adev->RW32_debug = tmp;
	return rc;
}

ssize_t acq400_sideported_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	int rc;
	set_continuous_reader(adev);

	if (wait_event_interruptible(
			adev->refill_ready,
			!list_empty(&adev->REFILLS) ||
			adev->rt.refill_error ||
			adev->rt.please_stop)){
		return -EINTR;
	} else if (adev->rt.please_stop){
		rc = copy_to_user(buf, "D", 1);
		if (rc) {
			return -1;
		}
		return GET_FULL_DONE;
	} else if (adev->rt.refill_error){
		rc = copy_to_user(buf, "RE", 2);
		if (rc){
			return -1;
		}
		return GET_FULL_REFILL_ERR;
	}
	return 0;
}

int acq420_sideported_stop(struct inode *inode, struct file *file)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	int trg_was_enabled = acq400_enable_trg(adev, 0);

	clr_continuous_reader(adev);
	_acq420_continuous_stop(adev, 0);

	if (trg_was_enabled){
		/* and restore, because otherwise it's too confusing */
		acq400_enable_trg(adev, 1);
	}
	return acq400_release(inode, file);
}

static unsigned int acq420_sideported_poll(
	struct file *file, struct poll_table_struct *poll_table)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned int rc;

	if (!list_empty(&adev->REFILLS)){
		rc = POLLIN|POLLRDNORM;
	}else if (adev->rt.refill_error){
		rc = POLLERR;
	}else if (adev->rt.please_stop){
		rc = POLLHUP;
	}else{
		poll_wait(file, &adev->refill_ready, poll_table);
		if (!list_empty(&adev->REFILLS)){
			rc = POLLIN|POLLRDNORM;
		}else if (adev->rt.refill_error){
			rc = POLLERR;
		}else if (adev->rt.please_stop){
			rc = POLLHUP;
		}else{
			rc = 0;
		}
	}
	dev_dbg(DEVP(adev), "acq420_sideported_poll() ret %u\n", rc);
	return rc;
}
int acq420_open_sideported(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_sideported = {
			.open = acq420_sideported_start,
			.read = acq400_sideported_read,
			.release = acq420_sideported_stop,
			.poll = acq420_sideported_poll
	};
	int rc = acq400_open_main(inode, file);
	if (rc){
		return rc;
	}
	file->f_op = &acq400_fops_sideported;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}









int acq400_init_descriptor(struct acq400_path_descriptor** pd)
{
	struct acq400_path_descriptor* pdesc = kzalloc(PDSZ, GFP_KERNEL);
	init_waitqueue_head(&pdesc->waitq);

	*pd = pdesc;
	return 0;
}





int acq400_open(struct inode *inode, struct file *file)
{
	struct acq400_dev *adev;
	int minor;
	int rc;

	acq400_init_descriptor((struct acq400_path_descriptor**)&file->private_data);


	PD(file)->dev = adev = container_of(inode->i_cdev, struct acq400_dev, cdev);
	PD(file)->minor = minor = MINOR(inode->i_rdev);
	INIT_LIST_HEAD(&PD(file)->RESERVED);

	dev_dbg(DEVP(adev), "hello: minor:%d\n", minor);


	switch(minor){
	case ACQ420_MINOR_SIDEPORTED:
		rc = acq420_open_sideported(inode, file);
		break;
	case ACQ420_MINOR_CONTINUOUS:
		rc = acq420_open_continuous(inode, file);
		break;
	case ACQ420_MINOR_0:
		rc = acq400_open_main(inode, file);
		break;

	default:
		rc = acq400_open_ui(inode, file);
	}

	if (rc != 0){
		dev_err(DEVP(adev), "acq400_open FAIL minor:%d rc:%d", minor, rc);
		if (PD(file)) kfree(PD(file));
		SETPD(file, 0);
	}
	return rc;
}


int acq400_release(struct inode *inode, struct file *file)
{
        struct acq400_dev *adev = ACQ400_DEV(file);

        if (mutex_lock_interruptible(&adev->mutex)) {
                return -EINTR;
        }

        /* Manage writes via reference counts */
        switch (file->f_flags & O_ACCMODE) {
        case O_RDONLY:
                break;
        case O_WRONLY:
                adev->writers--;
                break;
        case O_RDWR:
        default:
                adev->writers--;
        }

        adev->stats.closes++;

        mutex_unlock(&adev->mutex);

        if (PD(file)) kfree(PD(file));
        return 0;
}





void xo400_getDMA(struct acq400_dev* adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if (adev->dma_chan[0] == 0 &&
	    (xo_use_distributor || ao420_dma_threshold < xo_dev->xo.max_fifo_samples)){
		if (get_dma_channels(adev)){
			dev_err(DEVP(adev), "no dma chan");
			ao420_dma_threshold = xo_dev->xo.max_fifo_samples;
		}else{
			if (adev->dma_chan[0] == 0){
				dev_err(DEVP(adev), "BAD DMA CHAN!");
			}else{
				dev_dbg(DEVP(adev),
					"ao420_reset_playloop() channel %d",
					adev->dma_chan[0]->chan_id);
			}
		}
	}
}


/* minimum PL330 size
 * could be a physical limit, but also pragmatic given slow setup time
 */


static int xo400_write_fifo_dma(struct acq400_dev* adev, int frombyte, int bytes)
{
	/* index from cursor to find correct hb.
	 * HB's are NOT physically contiguous ..
	 * WARNING: assumes all HB's are the same length ..
	 * WARNING: assumes NEVER overlaps HB end ..
	 * This is OK provided xo400_write_fifo_dma is a factor of hb->len (it is).
	 * easy to catch anyway .. warn that we took the catch ..
	 */
	unsigned len = adev->cursor.hb[0]->len;
	unsigned ib = frombyte/len;
	unsigned offset = frombyte - ib*len;
	int rc;

	if (unlikely(offset + bytes > len)){
		if (xo_use_contiguous_pa_if_possible && adev->cursor.hb[ib]->pa + len == adev->cursor.hb[ib+1]->pa){
			dev_dbg(DEVP(adev), "xo400_write_fifo_dma() [%d] %d %d unlikely happened we have contiguous pa, keep going",
							ib, frombyte, bytes);
		}else{
			bytes = len - offset;		// buffer head room
			dev_dbg(DEVP(adev), "xo400_write_fifo_dma() [%d] %d %d unlikely happened",
				ib, frombyte, bytes);
		}
	}
	if (bytes >= min_dma_bytes){
		int rbytes = (bytes/min_dma_bytes)*min_dma_bytes;
		if (rbytes != bytes){
			dev_dbg(DEVP(adev), "xo400_write_fifo_dma() rounding bytes from %d to %d", bytes, rbytes);
			bytes = rbytes;
		}
	}else{
		return xo400_write_fifo(adev, frombyte, bytes);
	}

	rc = dma_memcpy(adev,
			adev->dev_physaddr+AXI_FIFO,
			adev->cursor.hb[ib]->pa+offset, bytes);

	if (rc != bytes){
		dev_err(DEVP(adev), "dma_memcpy FAILED :%d\n", rc);
	}
	return rc;
}





#define DAC_FIFO_STA_ERR (ADC_FIFO_STA_EMPTY|ADC_FIFO_STA_ERR)

void xo_check_fiferr(struct acq400_dev* adev, unsigned fsr)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	u32 fifo_sta = acq400rd32(adev, fsr);

	dev_dbg(DEVP(adev), "check_fiferr() fsr:%04x sta:%08x", fsr, fifo_sta);

	if ((fifo_sta&ADC_FIFO_STA_ACTIVE) == 0){
		return;
	}
	if ((fifo_sta&DAC_FIFO_STA_ERR) != 0){
		unsigned stat2  = acq400rd32(adev, fsr);

		acq400wr32(adev, fsr, fifo_sta&0x0000000f);

		if (++adev->stats.fifo_errors < 10){
			if ((fifo_sta & ADC_FIFO_STA_EMPTY) != 0){
				dev_err(DEVP(adev),
				"ERROR FIFO underrun at %d %08x samples:%d",
				adev->stats.fifo_interrupts, fifo_sta,
				xo_dev->xo.getFifoSamples(adev));
			}else{
				dev_err(DEVP(adev), "ERROR FIFO at %d %08x samples:%d stat2:%08x",
					adev->stats.fifo_interrupts, fifo_sta,
					xo_dev->xo.getFifoSamples(adev),
					stat2);
			}
		}

	}
}

int ao_auto_rearm(void *clidat)
/* poll for AWG complete, then rearm it */
{
	struct acq400_dev* adev = (struct acq400_dev*)clidat;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned dio_sc;
	int pollcat = 0;
	wait_queue_head_t wait;
	init_waitqueue_head(&wait);

	dev_dbg(DEVP(adev), "ao_auto_rearm() 01");

	while (xo_dev->xo.getFifoSamples(adev)){
		if ((pollcat++ & 0x3f) == 0){
			dev_dbg(DEVP(adev), "ao_auto_rearm() 20 %d", pollcat);
		}
		wait_event_interruptible_timeout(wait, 0, 1);
	}
	dio_sc = acq400rd32(adev, DIO432_DIO_SAMPLE_COUNT);
	adev->onStop(adev);
	if (IS_DIO432X(adev)){
		dev_dbg(DEVP(adev), "ao_auto_rearm FIFO drained count %u pollcat %d", dio_sc, pollcat);
	}

	if (xo_dev->AO_playloop.length > 0 &&
		xo_dev->AO_playloop.oneshot == AO_oneshot_rearm){
		dev_dbg(DEVP(adev), "ao_auto_rearm() reset %d", xo_dev->AO_playloop.length);
		xo_dev->AO_playloop.cursor = 0;
		xo400_reset_playloop(adev, xo_dev->AO_playloop.length);
	}
	dev_dbg(DEVP(adev), "ao_auto_rearm() 99");
	return 0;
}

static inline unsigned xo400_getFillThreshold(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);

	return xo_dev->xo.max_fifo_samples/128;
}

int ao420_getFifoHeadroom(struct acq400_dev* adev) {
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	/* pgm: don't trust it to fill to the top */
	unsigned samples = xo_dev->xo.getFifoSamples(adev);
	unsigned maxsam = xo_dev->xo.max_fifo_samples - 8;

	if (samples > maxsam){
		return 0;
	}else{
		return maxsam - samples;
	}
}
static int xo400_fill_fifo(struct acq400_dev* adev)
/* returns 1 if further interrupts are required */
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int headroom;
	int rc = 0;		/* assume more ints wanted unless complete */
	int maxiter = 1000;
	static int next_one_verbose;

	dev_dbg(DEVP(adev), "%s 01\n",__FUNCTION__);

	if (mutex_lock_interruptible(&adev->awg_mutex)) {
		return 0;
	}
	go_rt(adev->nchan_enabled>8? MAX_RT_PRIO: MAX_RT_PRIO-2);

	while(xo_dev->AO_playloop.length != 0 &&
	      (headroom = ao420_getFifoHeadroom(adev)) > xo400_getFillThreshold(adev) &&
	      xo_dev->AO_playloop.length > xo_dev->AO_playloop.cursor			 ){

		int remaining = xo_dev->AO_playloop.length - xo_dev->AO_playloop.cursor;
		int headroom_lt_remaining = headroom < remaining;

		rc = 1;
		remaining = min(remaining, headroom);

		dev_dbg(DEVP(adev), "headroom:%d remaining:%d using:%d\n",
			headroom, xo_dev->AO_playloop.length - xo_dev->AO_playloop.cursor, remaining);

		if (remaining){
			int cursor = AOSAMPLES2BYTES(adev, xo_dev->AO_playloop.cursor);
			int lenbytes = AOSAMPLES2BYTES(adev, remaining);

			lenbytes = min(lenbytes, AO420_MAX_FILL_BLOCK);

			if (adev->dma_chan[0] != 0 && lenbytes > max(min_dma_bytes, ao420_dma_threshold)){
				int nbuf = lenbytes/min_dma_bytes;
				int dma_bytes = nbuf*min_dma_bytes;

				dev_dbg(DEVP(adev), "dma: cursor:%5d lenbytes: %d", cursor, dma_bytes);
				lenbytes = xo400_write_fifo_dma(adev, cursor, dma_bytes);
				if (next_one_verbose || lenbytes != dma_bytes){
					dev_info(DEVP(adev), "dma: [%d] cursor:%5d dma_bytes %d lenbytes: %d",
							next_one_verbose, cursor, dma_bytes, lenbytes);
					next_one_verbose = !next_one_verbose;
				}
			}else if (headroom_lt_remaining){
				/* FIFO nearly full, catch it next time */
				break;
			}else{
				/* we really have to write the end of the buffer .. */
				dev_dbg(DEVP(adev), "pio: cursor:%5d lenbytes: %d", cursor, lenbytes);
				lenbytes = xo400_write_fifo_dma(adev, cursor, lenbytes);
			}
			/* UGLY: has divide. */
			if (lenbytes < 0){
				dev_err(DEVP(adev), "ERROR in DMA");
				break;
			}
			xo_dev->AO_playloop.cursor += AOBYTES2SAMPLES(adev, lenbytes);
		}


		if (--maxiter == 0){
			dev_warn(DEVP(adev), "xo400_fill_fifo() working too hard breaking to release mutex");
			break;
		}
	}
	if (xo_dev->AO_playloop.length > 0 && xo_dev->AO_playloop.cursor >= xo_dev->AO_playloop.length){
		if (xo_dev->AO_playloop.oneshot &&
			(xo_dev->AO_playloop.repeats==0 || --xo_dev->AO_playloop.repeats==0)){
			dev_dbg(DEVP(adev), "%s done disable interrupt", __FUNCTION__);
			x400_disable_interrupt(adev);
			rc = 0;
			kthread_run(ao_auto_rearm, adev, "%s.awgrearm", adev->dev_name);
			goto done_no_check;
		}else{
			xo_dev->AO_playloop.cursor = 0;
		}
	}

	xo_check_fiferr(adev, xo_dev->xo.fsr);
done_no_check:
	mutex_unlock(&adev->awg_mutex);
	dev_dbg(DEVP(adev), "%s done filling, samples:%08x headroom %d\n", __FUNCTION__,
			xo_dev->xo.getFifoSamples(adev), ao420_getFifoHeadroom(adev));
	return rc;
}
static irqreturn_t xo400_dma(int irq, void *dev_id)
/* keep the AO420 FIFO full. Recycle buffer only */
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);

	if (xo_use_distributor){
		dev_dbg(DEVP(adev), "xo400_dma() using distributor interrupt");
	}else if (xo_dev->AO_playloop.length){
		u32 start_samples = xo_dev->xo.getFifoSamples(adev);
		dev_dbg(DEVP(adev), "xo400_dma() start_samples: %u, headroom %d\n",
					start_samples, ao420_getFifoHeadroom(adev));
		if (xo400_fill_fifo(adev) && adev->lotide){
			x400_enable_interrupt(adev);
		}
		add_fifo_histo_ao42x(adev, start_samples);
	}

	return IRQ_HANDLED;
}







void xo400_distributor_feeder_control(struct acq400_dev* adev, int enable)
{
	int pollcat = 0;
	/* adev is the site (1?) adev. so wtask is dedicated to xo */
	/* @@todo start stop feeder loop */
	if (enable){
		adev->w_task = kthread_run(
			xo_data_loop, adev,
			"%s.xo", adev->dev_name);
		while(!adev->task_active){
			msleep(10);
			if (++pollcat > 100){
				dev_warn(DEVP(adev), "waiting for task start");
			}
		}
	}else{
		if (adev->task_active && adev->w_task != 0){
			kthread_stop(adev->w_task);
		}
		while(adev->task_active){
			msleep(10);
			if (++pollcat > 100){
				dev_warn(DEVP(adev), "waiting for task stop");
			}
		}
		adev->w_task = 0;
	}
}


void set_awg_abort_en(struct acq400_dev* adev)
{
	u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);
	acq400wr32(adev, DAC_CTRL, dac_ctrl|DAC_CTRL_AWG_ABORT);
}

void set_awg_abort_clr(struct acq400_dev* adev){
	u32 dac_ctrl = acq400rd32(adev, DAC_CTRL);
	acq400wr32(adev, DAC_CTRL, dac_ctrl &= ~DAC_CTRL_AWG_ABORT);
}

int xo400_reset_playloop(struct acq400_dev* adev, unsigned playloop_length)
{
	struct acq400_dev *adev0 = acq400_devices[0];
	struct acq400_sc_dev* sc_dev = container_of(adev0, struct acq400_sc_dev, adev);
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int first_in_set = xo_use_distributor && (adev == sc_dev->distributor_set[0]);
	int use_frontside = !xo_use_distributor;


	if (xo_use_distributor && !first_in_set){
		dev_warn(DEVP(adev), "xo400_reset_playloop() xo_use_distributor but not first_in_set");
	}
	dev_dbg(DEVP(adev), "xo400_reset_playloop(%d) => %d",
			xo_dev->AO_playloop.length, playloop_length);



	if (playloop_length && xo_use_distributor && adev->task_active){
		dev_warn(DEVP(adev), "XO AWG is already tee'd up, not possible to abort");
		return -1;
	}

	if (playloop_length == 0 && xo_use_distributor){
		dev_dbg(DEVP(adev), "xo400_reset_playloop set awg_abort");
		xo_dev->AO_playloop.oneshot = AO_oneshot;
		acq400_visit_set(sc_dev->distributor_set, set_awg_abort_en);
		while(adev->task_active){
			dev_dbg(DEVP(adev), "xo400_reset_playloop wait task_active -> 0 ");
			msleep(50);
		}
		acq400_visit_set(sc_dev->distributor_set, set_awg_abort_clr);
		dev_dbg(DEVP(adev), "xo400_reset_playloop clr awg_abort");
	}

	if (mutex_lock_interruptible(&adev->awg_mutex)) {
		return -1;
	}else{
		adev->onStop(adev);

		if (first_in_set){
			xo400_distributor_feeder_control(adev, 0);
		}
		mutex_unlock(&adev->awg_mutex);
	}
	if (xo_use_distributor){
		acq2106_distributor_reset_enable(adev0);
	}

	while(adev->task_active){
		dev_dbg(DEVP(adev), "xo400_reset_playloop wait task_active -> 0 ");
		msleep(100);
	}

	if (playloop_length == XO400_PLAYCONTINUOUS){
		xo_dev->AO_playloop.oneshot = AO_continuous;
	}else{
		xo_dev->AO_playloop.length = playloop_length;
	}

	if (playloop_length != 0){
		if (IS_DIO432X(adev)){
			dio432_set_mode(adev, DIO432_CLOCKED, 1);
		}
		if (xo_dev->xo.getFifoSamples(adev) != 0){
			dev_err(DEVP(adev), "ERROR: FIFO not EMPTY at start fill %d",
					xo_dev->xo.getFifoSamples(adev));
		}

		if (first_in_set){
			xo400_getDMA(adev);
			if (playloop_length != XO400_PLAYCONTINUOUS){
				xo400_distributor_feeder_control(adev, 1);
				while(!adev->task_active){
					dev_dbg(DEVP(adev), "xo400_reset_playloop wait task_active -> 0 ");
					msleep(100);
				}
			}
			acq400_visit_set(sc_dev->distributor_set, adev->onStart);
		}else if (use_frontside){
			xo400_getDMA(adev);
			xo400_fill_fifo(adev);
			ao420_clear_fifo_flags(adev);
			adev->onStart(adev);
		}
		/* else do nothing */
	}else{
		if (first_in_set){
			acq400_visit_set(sc_dev->distributor_set, adev->onStop);
		}else if (use_frontside){
			adev->onStop(adev);
			xo_dev->AO_playloop.cursor = 0;
		}
	}

	return 0;
}


void null_put_empty(struct acq400_dev *adev, struct HBM* hbm) {}


int axi64_data_loop(void* data)
{
	struct acq400_dev *adev = (struct acq400_dev *)data;
	/* wait for event from OTHER channel */
	int nloop = 0;
	struct HBM* hbm;
	int rc;

	dev_dbg(DEVP(adev), "axi64_data_loop() 01");

	adev->onPutEmpty = axi_oneshot? null_put_empty: poison_one_buffer_fastidious;

	if (AXI_CALL_HELPER){
		if ((rc = axi64_load_dmac(adev, CMASK0)) != 0){
			dev_err(DEVP(adev), "axi64_load_dmac() failed %d", rc);
			return -1;
		}else{
			dev_info(DEVP(adev), "axi64_load_dmac() helper done");
		}
	}

	poison_all_buffers(adev);
	if (check_all_buffers_are_poisoned(adev) || kthread_should_stop()){
		dev_err(DEVP(adev), "axi64_dual_data_loop()#%d %s", __LINE__, kthread_should_stop()? "STOP": "FAIL");
		goto quit;
	}

	hbm = getEmpty(adev);
	dev_info(DEVP(adev), "axi64_data_loop() holding hbm:%d", hbm->ix);

	go_rt(MAX_RT_PRIO-4);
	adev->task_active = 1;

	for(; !kthread_should_stop(); ++nloop){
		int ddone = 0;
		dev_dbg(DEVP(adev), "axi64_data_loop() 10");

		if (wait_event_interruptible_timeout(
			adev->DMA_READY,
			(ddone = dma_done(adev, hbm)) || kthread_should_stop(),
			AXI_BUFFER_CHECK_TICKS) <= 0){
			if (kthread_should_stop()){
				goto quit;
			}
		}
		if (adev->rt.axi64_firstups) adev->rt.axi64_wakeups++;
		if (!ddone){
			dev_dbg(DEVP(adev), "axi64_data_loop() 22 %d", hbm->ix);
			continue;
		}

		putFull(adev);
		adev->rt.axi64_firstups++;

		if (AXI_DMA_HAS_CATCHUP){
			while((hbm = getEmpty(adev)) != 0){
				if (poison_overwritten(adev, hbm)){
					putFull(adev);
					adev->rt.axi64_catchups++;
				}else{
					break;	/* now wait for this hbm to complete */
				}
			}
		}else{
			hbm = getEmpty(adev);
		}

		if (check_fifo_statuses(adev)){
			dev_err(DEVP(adev), "ERROR: quit on fifo status check");
			goto quit;
		}

		if (!IS_SC(adev)){
			//acq420_enable_interrupt(adev);
		}
		if (hbm == 0){
			++adev->stats.errors;
			move_list_to_empty(adev, &adev->REFILLS);
			dev_warn(DEVP(adev), "discarded FULL Q\n");
			++adev->rt.buffers_dropped;

			if (quit_on_buffer_exhaustion){
				adev->rt.refill_error = 1;
				wake_up_interruptible_all(&adev->refill_ready);
				dev_warn(DEVP(adev), "quit_on_buffer_exhaustion\n");
				if (adev->h_task != 0){
					kthread_stop(adev->h_task);
					adev->h_task = 0;
				}
				goto quit;
			}else{
				hbm = getEmpty(adev);
				if (hbm == 0){
					dev_err(DEVP(adev), "STILL NO EMPTIES");
					goto quit;
				}else{
					poison_one_buffer(adev, hbm);
				}
			}
		}
	}
quit:
	dev_dbg(DEVP(adev), "ai_data_loop() 98 calling DMA_TERMINATE_ALL");
	axi64_terminate(adev->dma_chan[0]);


	clear_poison_all_buffers(adev);
	adev->task_active = 0;
	dev_dbg(DEVP(adev), "ai_data_loop() 99");
	return 0;
}

int axi64_dual_int_stats[3];
int axi64_dual_int_stats_len = 3;
module_param_array(axi64_dual_int_stats, int, &axi64_dual_int_stats_len, 0644);
MODULE_PARM_DESC(axi64_dual_int_stats, "dual_data_loop wait histo: [0]: signals, [1]: timeouts, [2] wakeups");

int axi64_dual_poison_stats_len = 4;
int axi64_dual_poison_stats0[4];
module_param_array(axi64_dual_poison_stats0, int, &axi64_dual_poison_stats_len, 0644);
MODULE_PARM_DESC(axi64_dual_poison_stats0, "dual_data_loop hbuf histo before delay: [0]: none, [1]: hbm0, [2] hbm1, [3] hbm0+hbm1 (Good)");
int axi64_dual_poison_stats1[4];
module_param_array(axi64_dual_poison_stats1, int, &axi64_dual_poison_stats_len, 0644);
MODULE_PARM_DESC(axi64_dual_poison_stats1, "dual_data_loop hbuf histo after delay: [0]: none, [1]: hbm0, [2] hbm1, [3] hbm0+hbm1 (Good)");

int axi64_dual_poison_udelay = 100;
module_param(axi64_dual_poison_udelay, int, 0644);


int axi64_dual_data_loop(void* data)
{
	struct acq400_dev *adev = (struct acq400_dev *)data;
	/* wait for event from OTHER channel */
	int nloop = 0;
	struct HBM* hbm0;
	struct HBM* hbm1;
	int rc;

	int oneshot_estop = 0;

	dev_dbg(DEVP(adev), "axi64_dual_data_loop() 01");

	adev->onPutEmpty = axi_oneshot? null_put_empty: poison_one_buffer_fastidious;

	if (AXI_CALL_HELPER){
		if ((rc = axi64_load_dmac(adev, CMASK0|CMASK1)) != 0){
			dev_err(DEVP(adev), "axi64_load_dmac() failed %d", rc);
			return -1;
		}else{
			dev_info(DEVP(adev), "axi64_load_dmac() helper done");
		}
	}
	poison_all_buffers(adev);
	if (check_all_buffers_are_poisoned(adev) || kthread_should_stop()){
		dev_err(DEVP(adev), "axi64_dual_data_loop()#%d %s", __LINE__,
				kthread_should_stop()? "STOP": "FAIL");
		goto quit;
	}

	hbm0 = getEmpty(adev); hbm1 = getEmpty(adev);

	dev_dbg(DEVP(adev), "axi64_dual_data_loop() holding hbm:%d,%d",
			hbm0->ix, hbm1->ix);

	go_rt(MAX_RT_PRIO-4);
	adev->task_active = 1;

	for(; !kthread_should_stop(); ++nloop){
		int ddone = 0;
		int oneshot_estop_threshold = adev->axi64[0].ndesc - 10;

		int rc = wait_event_interruptible_timeout(
			adev->DMA_READY,
			(oneshot_estop = (axi_oneshot && oneshot_estop_threshold && adev->rt.axi64_ints > oneshot_estop_threshold)) ||
			(ddone = dma_done(adev, hbm0)+2*dma_done(adev, hbm1)) ||
								kthread_should_stop(),
			AXI_BUFFER_CHECK_TICKS);

		dev_dbg(DEVP(adev), "axi64_dual_data_loop() %d wait rc :%d hbm:%03d,%03d ddone:%d,%d  ",
				nloop, rc, hbm0->ix, hbm1->ix, !!(ddone&1), !!(ddone&2));


		if (rc <= 0 && kthread_should_stop()){
			goto quit;
		}
		if (oneshot_estop){
			axi64_terminate(adev->dma_chan[0]);
			axi64_terminate(adev->dma_chan[1]);
			dev_warn(DEVP(adev), "axi64_dual_data_loop() oneshot_estop after %d ints", adev->rt.axi64_ints);
		}else{
			axi64_dual_int_stats[rc<0? 0: rc==0? 1: 2]++;
			axi64_dual_poison_stats0[ddone&0x3]++;
			if (adev->rt.axi64_firstups) adev->rt.axi64_wakeups++;

			switch (ddone){
			case 0:
				axi64_dual_poison_stats1[ddone&0x3]++;
				continue;
			case 3:
				axi64_dual_poison_stats1[ddone&0x3]++;
				break;
			case 1:
			case 2:
				if (axi64_dual_poison_udelay){
					usleep_range(axi64_dual_poison_udelay, 2*axi64_dual_poison_udelay);
					ddone = dma_done(adev, hbm0)+2*dma_done(adev, hbm1);
				}
				axi64_dual_poison_stats1[ddone&0x3]++;
				dev_dbg(DEVP(adev), "axi64_dual_data_loop() mismatch %d,%d  hbm:%03d,%03d",
									!!(ddone&1), !!(ddone&2), hbm0->ix, hbm1->ix);
				break;
			default:
				dev_err(DEVP(adev), "ERROR at line %d", __LINE__);
			}
		}

		putFull(adev); putFull(adev);
		adev->rt.axi64_firstups++;

		if (AXI_DMA_HAS_CATCHUP){
			while(multipleEmptiesWaiting(adev)){
				hbm0 = getEmpty(adev);
				hbm1 = getEmpty(adev);
				if (dma_done(adev, hbm0)&&dma_done(adev, hbm1)){
					putFull(adev); putFull(adev);
					adev->rt.axi64_catchups++;
				}else{
					break;	/* now wait for this hbm to complete */
				}
			}
		}else{
			hbm0 = getEmpty(adev); hbm1 = getEmpty(adev);
		}

		if (check_fifo_statuses(adev)){
			dev_err(DEVP(adev), "ERROR: quit on fifo status check");
			goto quit;
		}

		if (!IS_SC(adev)){
			//acq420_enable_interrupt(adev);
		}
		if (hbm0 == 0 || hbm1 == 0){
			++adev->stats.errors;
			move_list_to_empty(adev, &adev->REFILLS);
			move_list_to_empty(adev, &adev->REFILLS);
			dev_warn(DEVP(adev), "discarded FULL Q\n");
			++adev->rt.buffers_dropped;

			if (quit_on_buffer_exhaustion){
				adev->rt.refill_error = 1;
				wake_up_interruptible_all(&adev->refill_ready);
				dev_warn(DEVP(adev), "quit_on_buffer_exhaustion\n");
				if (adev->h_task != 0){
					kthread_stop(adev->h_task);
					adev->h_task = 0;
				}
				goto quit;
			}else{
				hbm0 = getEmpty(adev);
				hbm1 = getEmpty(adev);
				if (hbm0 == 0 || hbm1 == 0){
					dev_err(DEVP(adev), "STILL NO EMPTIES");
					goto quit;
				}else{
					poison_one_buffer(adev, hbm0);
					poison_one_buffer(adev, hbm1);
				}
			}
		}
	}
quit:
	dev_dbg(DEVP(adev), "axi64_dual_data_loop() 98 calling DMA_TERMINATE_ALL");
	if (!oneshot_estop){
		axi64_terminate(adev->dma_chan[0]);
		axi64_terminate(adev->dma_chan[1]);
	}

	clear_poison_all_buffers(adev);
	adev->task_active = 0;
	dev_dbg(DEVP(adev), "axi64_dual_data_loop() 99");
	return 0;
}


int ai_data_loop(void *data)
{
	/* wait for event from OTHER channel */
	static const unsigned wflags[2] = { DMA_WAIT_EV0, DMA_WAIT_EV1 };
	static const unsigned sflags[2] = { DMA_SET_EV1,  DMA_SET_EV0  };

	struct acq400_dev *adev = (struct acq400_dev *)data;
	int nloop = 0;
	int ic;
	long dma_timeout = START_TIMEOUT;
	struct HBM* hbm0;
	struct HBM* hbm1;

#define DMA_ASYNC_MEMCPY(adev, chan, hbm) \
	dma_async_memcpy_callback(adev->dma_chan[chan], hbm->pa, \
			FIFO_PA(adev), adev->bufferlen, \
			DMA_SC_FLAGS|wflags[chan]|sflags[chan], \
			acq400_dma_callback, adev)
#define DMA_ASYNC_MEMCPY_NWFE(adev, chan, hbm) \
	dma_async_memcpy_callback(adev->dma_chan[chan], hbm->pa, \
			FIFO_PA(adev), adev->bufferlen, \
			DMA_SC_FLAGS|sflags[chan], \
			acq400_dma_callback, adev)


	dev_dbg(DEVP(adev), "ai_data_loop() 01");

	adev->onPutEmpty = null_put_empty;
	hbm0 = getEmpty(adev);
	hbm1 = getEmpty(adev);


	dev_dbg(DEVP(adev), "hbm0:0x%08x chan:%d\n", hbm0->pa, adev->dma_chan[0]->chan_id);
	dev_dbg(DEVP(adev), "hbm1:0x%08x chan:%d\n", hbm1->pa, adev->dma_chan[1]->chan_id);

	/* prime the DMAC with buffers 0 and 1 ready to go. */
	adev->dma_cookies[1] = DMA_ASYNC_MEMCPY(adev, 1, hbm1);
	dma_async_issue_pending(adev->dma_chan[1]);
	adev->dma_cookies[0] = DMA_ASYNC_MEMCPY_NWFE(adev, 0, hbm0);
	dma_async_issue_pending(adev->dma_chan[0]);

	yield();
	go_rt(MAX_RT_PRIO-4);
	adev->task_active = 1;

	if (adev->fifo_isr_done){
		dev_warn(DEVP(adev), "fifo_isr_done EARLY\n");
	}
#if 0
		/* @@todo pgm: no site 0 interrupt for now */
	/* wait initial hitide interrupt, avoid dma timeout on TRIG */
	if (wait_event_interruptible(adev->w_waitq,
			adev->fifo_isr_done || kthread_should_stop())){
		goto quit;
	}
#endif
	if (kthread_should_stop()){
		goto quit;
	}
#if 0
	dev_dbg(DEVP(adev), "rx initial FIFO interrupt, into the work loop\n");
#endif

	for(; !kthread_should_stop(); ++nloop){
		for (ic = 0; ic < 2 && !kthread_should_stop();
				++ic, dma_timeout = DMA_TIMEOUT){
			struct HBM* hbm;
			int emergency_drain_request = 0;

			dev_dbg(DEVP(adev), "wait for dma_chan[%d] %p %d timeout=%ld\n",
					ic, adev->dma_chan[ic], adev->dma_cookies[ic],
					dma_timeout);

			if (wait_event_interruptible_timeout(
					adev->DMA_READY,
					adev->dma_callback_done || kthread_should_stop(),
					dma_timeout) <= 0){
				adev->rt.refill_error = 2;
				wake_up_interruptible_all(&adev->refill_ready);
				dev_err(DEVP(adev), "TIMEOUT %ld ticks waiting for DMA[%d] loop:%d aggsta:%08x line:%d\n",
						dma_timeout, ic, nloop, acq400rd32(adev, AGGSTA), __LINE__);
				goto quit;
			}
			--adev->dma_callback_done;
			if (kthread_should_stop()){
				goto quit;
			}
			if(dma_sync_wait(adev->dma_chan[ic], adev->dma_cookies[ic]) != DMA_SUCCESS){
				dev_err(DEVP(adev), "dma_sync_wait nloop:%d chan:%d timeout", nloop, ic);
				goto quit;
			}
			dev_dbg(DEVP(adev), "DMA_SUCCESS: SYNC %d done\n", ic);

			putFull(adev);
			if ((hbm = getEmpty(adev)) == 0){
				dev_warn(DEVP(adev), "Pulling data from FullQ\n");
				hbm = getEmptyFromRefills(adev);
				if (hbm == 0){
					dev_err(DEVP(adev), "FAILED to pull data from FullQ, quitting\n");
					goto quit;
				}else{
					++adev->stats.errors;
					emergency_drain_request = 1;
				}
			}
			adev->dma_cookies[ic] = DMA_ASYNC_MEMCPY(adev, ic, hbm);
			dma_async_issue_pending(adev->dma_chan[ic]);

			if (check_fifo_statuses(adev)){
				dev_err(DEVP(adev), "ERROR: quit on fifo status check");
				goto quit;
			}

			if (!IS_SC(adev)){
				//acq420_enable_interrupt(adev);
			}
			if (emergency_drain_request){
				move_list_to_empty(adev, &adev->REFILLS);
				dev_warn(DEVP(adev), "discarded FULL Q\n");
				++adev->rt.buffers_dropped;

				if (quit_on_buffer_exhaustion){
					adev->rt.refill_error = 1;
					wake_up_interruptible_all(&adev->refill_ready);
					dev_warn(DEVP(adev), "quit_on_buffer_exhaustion\n");
					goto quit;
				}
			}
		}
	}
quit:
	dev_dbg(DEVP(adev), "ai_data_loop() 98 calling DMA_TERMINATE_ALL");
	for (ic = 0; ic < 2; ++ic){
		struct dma_chan *chan = adev->dma_chan[ic];
		chan->device->device_control(chan, DMA_TERMINATE_ALL, 0);
	}

	adev->task_active = 0;

	if (!kthread_should_stop()){
		dev_dbg(DEVP(adev), "ai_data_loop() wait stop 88");
		do_exit(0);
	}else{
		dev_dbg(DEVP(adev), "ai_data_loop() 99");
		return 0;
	}
#undef DMA_ASYNC_MEMCPY
#undef DMA_ASYNC_MEMCPY_NWFE

}

/* this is the much-vaunted hrtimer - program in nsec,
 * still with 10msec resolution .. or maybe it's really good (second time around)
 */
static enum hrtimer_restart pulse_stretcher_timer_handler(struct hrtimer
					  *handle)
{
	struct XTD_dev *xtd_dev = container_of(handle, struct XTD_dev, atd.timer);
	struct acq400_dev *adev = &xtd_dev->adev;

	acq400_clearDelTrgEvent(adev);
	acq400wr32(adev, ATD_TRIGGERED, xtd_dev->atd.event_source);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart pulse_stretcher_display_timer_handler(
		struct hrtimer *handle)
{
	struct XTD_dev *xtd_dev = container_of(handle, struct XTD_dev, atd_display.timer);

	xtd_dev->atd_display.event_source = 0;
	return HRTIMER_NORESTART;
}

int cosco_count=4;
int cosco[4];

module_param_array(cosco, int, &cosco_count, 0644);
MODULE_PARM_DESC(lotide, "histogram of cos isr");

static void xtd_action(struct acq400_dev *adev)
{
	struct XTD_dev *xtd_dev = container_of(adev, struct XTD_dev, adev);
	xtd_dev->atd.event_source = acq400rd32(adev, ATD_TRIGGERED);

	if (xtd_dev->atd.event_source){
		xtd_dev->atd_display.event_source = xtd_dev->atd.event_source;

		if (HAS_DTD(adev)){
			hrtimer_start(&xtd_dev->atd.timer, ktime_set(0, dtd_pulse_nsec),
					HRTIMER_MODE_REL);
		}

		hrtimer_start(&xtd_dev->atd_display.timer,
			ktime_set(0, dtd_display_pulse_nsec), HRTIMER_MODE_REL);
	}
}

/** @@TODO what if two DMA channels busy? */

#define CHANNEL0	0


static void event_action(struct acq400_dev *adev, u32 status)
{
	struct acq400_dev* adev0 = acq400_devices[0];

	++adev->rt.event_count;

	if ((status&event_status_mask) != 0){
		u32 sc, lc;

		sc = adev->rt.samples_at_event = acq400_agg_sample_count();

		if (acq400_event_count_limit &&
		    adev->rt.event_count >= acq400_event_count_limit){
			acq400_enable_event0(adev, 0);
		}
		if (adev0->axi_buffers_after_event) {
			axi64_tie_off_dmac(adev0, CHANNEL0, adev0->axi_buffers_after_event);
			adev0->axi_buffers_after_event = 0;
		}
		lc = adev->rt.samples_at_event_latch = acq400rd32(adev, EVT_SC_LATCH);
		adev->rt.sample_clocks_at_event =
					acq400rd32(adev, ADC_SAMPLE_CLK_CTR);
		if (HAS_XTD(adev)){
			xtd_action(adev);
		}

		wake_up_interruptible(&adev->event_waitq);
		dev_dbg(DEVP(adev), "event_action() sc:%08x %s lc %08x %d",
				sc, sc>lc? ">": "<", lc,
				sc>lc? sc-lc: lc-sc);
	}
	if ((status&ADC_INT_CSR_EVENT1) != 0){
		if (HAS_XTD(adev)){
			xtd_action(adev);
		}
	}
	cosco[EVX_TO_INDEX(status)]++;

	dev_dbg(DEVP(adev), "event_action() %08x\n", status);
}

static void hitide_action(struct acq400_dev *adev, u32 status)
{
	if ((status&ADC_INT_CSR_HITIDE) != 0){
		add_fifo_histo(adev, acq420_get_fifo_samples(adev));
		adev->stats.fifo_interrupts++;
		adev->fifo_isr_done = 1;
		wake_up_interruptible(&adev->w_waitq);
	}
}

static irqreturn_t acq400_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	volatile u32 status = x400_get_interrupt(adev);
	event_action(adev, status);
	hitide_action(adev, status);

	x400_set_interrupt(adev, status);
	dev_dbg(DEVP(adev), "acq400_isr %08x\n", status);

	return IRQ_HANDLED;
}

static irqreturn_t cos_isr(struct acq400_dev *adev)
{
	struct XTD_dev *xtd_dev = container_of(adev, struct XTD_dev, adev);
	u32 cos = acq400rd32(adev, DIO482_COS_STA);

	u32 sc = adev->rt.samples_at_event = acq400_agg_sample_count();
	u32 lc;

	adev->rt.event_count++;
	acq400wr32(adev, DIO482_COS_STA, cos);
	xtd_dev->atd.event_source |= cos;

	lc = adev->rt.samples_at_event_latch = acq400_adc_latch_count();
	wake_up_interruptible(&adev->event_waitq); 
	dev_dbg(DEVP(adev), "cos_isr() sc:%08x cos:0x%08x %u", adev->rt.samples_at_event, cos, cos);
	dev_dbg(DEVP(adev), "cos_isr() sc %08x %s lc %08x diff %d",
			sc, sc>lc? ">": "<", lc,
			    sc>lc? sc-lc: lc-sc);
	return IRQ_HANDLED;
}

static irqreturn_t ao400_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	//volatile u32 status =
	u32 int_sta = x400_get_interrupt(adev);

	adev->stats.interrupts++;
	// @@todo check this.
	//acq420_clear_interrupt(adev, status);

	if ((int_sta & DIO_INT_CSR_COS) != 0){
		return cos_isr(adev);
	}

	x400_clr_interrupt(adev, int_sta);
	x400_disable_interrupt(adev);
	dev_dbg(DEVP(adev), "ao400_isr() status %08x NOW clear AND DISABLE => %08x",
			int_sta, x400_get_interrupt(adev));

	adev->stats.fifo_interrupts++;

	if (xo_dev->AO_playloop.length){
		return IRQ_WAKE_THREAD;	/* canned */
	}else{
		wake_up_interruptible(&adev->w_waitq);  /* stream_dac */
		return IRQ_HANDLED;
	}
}

struct file_operations acq400_fops = {
        .owner = THIS_MODULE,
        .read = acq400_read,
        .write = acq400_write,
        .open = acq400_open,
        .release = acq400_release,
        .mmap = acq400_mmap_bar
};

extern int axi_dma_agg32;

void acq400sc_init_defaults(struct acq400_dev *adev)
{
	u32 mcr = acq400rd32(adev, MCR);
	if (IS_ACQ2006B(adev)||IS_ACQ1001SC(adev)){
		mcr |= MCR_PSU_SYNC;
	}
	acq400wr32(adev, MCR, mcr|MCR_MOD_EN);

	if (IS_AXI64(adev)){
		acq400_set_AXI_DMA_len(adev, bufferlen);
		sync_continuous = 0;
		if (IS_AXI64_AGG32(adev)){
			axi_dma_agg32 = 1;
		}
	}
	if (IS_ACQ2106_STACK(adev)){
		first_axi_channel = 1;
		dev_info(DEVP(adev), "acq2106 STACK%s setting first_axi_channel=1",
				IS_ACQ2106_STAGGER(adev)? " and STAGGER": "");
	}
}

#ifdef CONFIG_OF
static struct of_device_id acq400_of_match[] /* __devinitdata */ = {
        { .compatible = "D-TACQ,acq400fmc"  },
        { .compatible = "D-TACQ,acq2006sc"  },
        { .compatible = "D-TACQ,acq1001sc"  },
        { .compatible = "D-TACQ,acq1002sc"  },
        { .compatible = "D-TACQ,acq400pmod" },
        { /* end of table */}
};
MODULE_DEVICE_TABLE(of, acq400_of_match);
#else
#define acq400_of_match NULL
#endif /* CONFIG_OF */


static int _acq400_device_tree_init(
		struct acq400_dev* adev, struct device_node *of_node)
{
	u32 site;

	if (of_property_read_u32(of_node, "site", &site) < 0){
		dev_warn(DEVP(adev), "error: site NOT specified in DT\n");
		return -1;
	}else{
		if (!isGoodSite(site)){
			dev_warn(DEVP(adev), "warning: site %d NOT GOOD\n", site);
			return -1;
		}else{
			adev->of_prams.site = site;
			dev_info(DEVP(adev), "site:%d GOOD\n", site);
		}
	}

	return 0;
}

static int acq400_device_tree_init(struct acq400_dev* adev)
{
	struct device_node *of_node = adev->pdev->dev.of_node;

        if (of_node) {
        	return _acq400_device_tree_init(adev, of_node);
        }else{
        	return -1;
        }
}


static struct acq400_dev* acq400_allocate_dev(struct platform_device *pdev)
/* Allocate and init a private structure to manage this device */
{
	struct acq400_dev* adev = kzalloc(sizeof(struct acq400_dev), GFP_KERNEL);
        if (adev == NULL) {
                return NULL;
        }else{
        	adev->pdev = pdev;
        	return adev;
        }
        return adev;
}

static struct acq400_dev* _acq400_init_dev(struct acq400_dev* adev)
/* Allocate and init a private structure to manage this device */
{
        init_waitqueue_head(&adev->waitq);
        init_waitqueue_head(&adev->DMA_READY);
        init_waitqueue_head(&adev->refill_ready);
        init_waitqueue_head(&adev->hb0_marker);

        mutex_init(&adev->mutex);
        mutex_init(&adev->awg_mutex);

        adev->fifo_histo = kzalloc(FIFO_HISTO_SZ*sizeof(u32), GFP_KERNEL);

        INIT_LIST_HEAD(&adev->EMPTIES);
        INIT_LIST_HEAD(&adev->INFLIGHT);
        INIT_LIST_HEAD(&adev->REFILLS);
        INIT_LIST_HEAD(&adev->OPENS);
        INIT_LIST_HEAD(&adev->STASH);
        INIT_LIST_HEAD(&adev->GRESV);
        mutex_init(&adev->list_mutex);

        init_waitqueue_head(&adev->w_waitq);
        init_waitqueue_head(&adev->event_waitq);
        adev->onStart = acqXXX_onStartNOP;
        adev->onStop = acqXXX_onStopNOP;
        adev->clkdiv_mask = ADC_CLK_DIV_MASK;

        return adev;
}

#define SPECIALIZE(sdev, adev, _type, _id) \
	do { \
		sdev = kzalloc(sizeof(_type), GFP_KERNEL); \
		strcpy(sdev->id, _id); \
		memcpy(&sdev->adev, adev, sizeof(struct acq400_dev)); \
		kfree(adev); \
		adev = &sdev->adev; \
	} while(0)


void acq400_subrate_init(struct Subrate* subrate)
{

}
static struct acq400_dev*
acq400_allocate_module_device(struct acq400_dev* adev)
/* subclass adev for module specific device, where required.
 * struct acq400_dev has become a huge kitchen sink with all
 * bits for all modules in it. Works, but it's careless.
 * Does this "subclass by deep clone to embedded object" technique
 * work .. apparently, it's not foolproof, so keep prior init to the
 * minimum.
 * */
{
	acq400_getID(adev);

	if (IS_SC(adev)){
		struct acq400_sc_dev *sc_dev;
		if (IS_ACQ2106_TIGA(adev)){
			dev_info(DEVP(adev), "acq2106_tiga device detected");
			SPECIALIZE(sc_dev, adev, struct acq400_tiga_dev, "SC");
		}else{
			SPECIALIZE(sc_dev, adev, struct acq400_sc_dev, "SC");
		}
	        INIT_LIST_HEAD(&sc_dev->bqw.bq_clients);
	        mutex_init(&sc_dev->bqw.bq_clients_mutex);

		mutex_init(&sc_dev->sewFifo[0].sf_mutex);
		mutex_init(&sc_dev->sewFifo[1].sf_mutex);

	        INIT_LIST_HEAD(&sc_dev->stream_dac.sd_bqw.bq_clients);
	        BQ_init(&sc_dev->stream_dac.refills, AWG_BACKLOG);
	        init_waitqueue_head(&sc_dev->stream_dac.sd_waitq);

	}else if (IS_ADC(adev)){
		struct ADC_dev *adc_dev;
		SPECIALIZE(adc_dev, adev, struct ADC_dev, IS_ACQ480(adev)? "ACQ480": "ACQ400");
		acq400_subrate_init(&adc_dev->subrate);
	}else if (IS_DIO482_PG(adev)){
		struct PG_dev *pg_dev;
		SPECIALIZE(pg_dev, adev, struct PG_dev, "PG");
	}else if (IS_XO(adev)){
		struct XO_dev *xo_dev;
		SPECIALIZE(xo_dev, adev, struct XO_dev, "XO");
	}else if (IS_BOLO8(adev)){
		struct acq400_bolo_dev *b8_dev;
		SPECIALIZE(b8_dev, adev, struct acq400_bolo_dev, "bolo8");
	}else if (HAS_XTD(adev)){
		struct XTD_dev *xtd_dev;
		SPECIALIZE(xtd_dev, adev, struct XTD_dev, "XTD");
	        acq400_timer_init(&xtd_dev->atd.timer, pulse_stretcher_timer_handler);
	        acq400_timer_init(&xtd_dev->atd_display.timer,
	        		pulse_stretcher_display_timer_handler);
	}
	_acq400_init_dev(adev);
        acq400_devices[ndevices++] = adev;
        acq400_sites[adev->of_prams.site] = adev;
        return adev;
}

static int acq400_remove(struct platform_device *pdev);



static int allocate_hbm(struct acq400_dev* adev, int nb, int bl, int dir)
{
	int ix = 0;
	struct HBM* cursor;

	dev_info(DEVP(adev), "allocate_hbm() nb:%d bl:0x%08x\n", nb, bl);
	adev->hb = kmalloc(nb*sizeof(struct HBM*), GFP_KERNEL);

	if (IS_SC(adev)){
	    ix += hbm_allocate(DEVP(adev), ix, reserve_buffers, bl, &adev->GRESV, dir);
	    nb -= reserve_buffers;
	}
	ix += hbm_allocate(DEVP(adev), ix, nb, bl, &adev->EMPTIES, dir);

	dev_info(DEVP(adev), "setting nbuffers %d\n", ix);
	ix = 0;
	list_for_each_entry(cursor, &adev->GRESV, list){
		dev_dbg(DEVP(adev), "setting ix G %d %d\n", ix, cursor->ix);
		adev->hb[ix] = cursor;
		cursor->bstate = BS_RESERVED;
		ix++;
	}
	list_for_each_entry(cursor, &adev->EMPTIES, list){
		dev_dbg(DEVP(adev), "setting ix E %d %d\n", ix, cursor->ix);
		adev->hb[ix] = cursor;
		ix++;
	}
	dev_info(DEVP(adev), "setting nbuffers %d\n", ix);
	adev->nbuffers = ix;
	adev->bufferlen = bl;
	return 0;
}


int init_axi64_hbm1(struct acq400_dev* adev, int axi_buffer_count, unsigned cmask)
{
	int isrc = reserve_buffers;
	int ic = cmask==CMASK1? 1: 0;
	int ib;

	adev->axi64[ic].axi64_hb = kmalloc(axi_buffer_count*sizeof(struct HBM*), GFP_KERNEL);

	for (ib = 0; ib < axi_buffer_count; ++ib){
		adev->axi64[ic].axi64_hb[ib] = adev->hb[isrc++];
	}
	return 0;
}
int init_axi64_hbm2(struct acq400_dev* adev, int axi_buffer_count)
{
	int isrc = reserve_buffers;
	int nb = axi_buffer_count/2;
	int ib;

	adev->axi64[0].axi64_hb = kmalloc(nb*sizeof(struct HBM*), GFP_KERNEL);
	adev->axi64[1].axi64_hb = kmalloc(nb*sizeof(struct HBM*), GFP_KERNEL);

	for (ib = 0; ib < nb; ++ib){
		adev->axi64[ first_axi_channel].axi64_hb[ib] = adev->hb[isrc++];
		adev->axi64[!first_axi_channel].axi64_hb[ib] = adev->hb[isrc++];
		dev_dbg(DEVP(adev), "init_axi64_hbm2() %d %d", ib, isrc);
	}
	return 0;
}
int init_axi64_hbm(struct acq400_dev* adev, int axi_buffer_count)
{
	unsigned cmask = (adev->dma_chan[0]!=0? CMASK0: 0) |
			 (adev->dma_chan[1]!=0? CMASK1: 0);
	switch(cmask){
	case CMASK1:
		dev_info(DEVP(adev), "INFO: selected AXI DMA1 only"); /* fall thru */
	case CMASK0:
		return init_axi64_hbm1(adev, axi_buffer_count, cmask);
	case CMASK1|CMASK0:
		return init_axi64_hbm2(adev, axi_buffer_count);
	default:
		dev_err(DEVP(adev), "ERROR no AXI DMA");
		return -1;
	}
}


int acq400_mod_init_irq(struct acq400_dev* adev)
{
	int rc;
	int irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET);
	if (irq <= 0){
		return 0;
	}
	dev_info(DEVP(adev), "acq400_mod_init_irq %d", irq);

	if (IS_AO42X(adev)||IS_DIO432X(adev)){
		rc = devm_request_threaded_irq(
				DEVP(adev), irq, ao400_isr, xo400_dma, IRQF_NO_THREAD,
				adev->dev_name,	adev);
	}else{
		rc = devm_request_irq(
				DEVP(adev), irq, acq400_isr, IRQF_NO_THREAD, adev->dev_name, adev);
	}
	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d K414 KLUDGE IGNORE\n", irq);
		return 0;
	}
	return rc;
}

void init_gpg_buffer(struct acq400_dev* adev, struct GPG_buffer *gpg, unsigned mem_base, unsigned dbgr)
{
	gpg->gpg_base = adev->dev_virtaddr + mem_base;
	gpg->gpg_buffer = kzalloc(4096, GFP_KERNEL);
	gpg->gpg_cursor = 0;
	gpg->gpg_dbgr = dbgr;
	dev_info(DEVP(adev), "gpg:%p base:%p buffer:%p cursor:%u", gpg, gpg->gpg_base, gpg->gpg_buffer, gpg->gpg_cursor);
}


int acq400_modprobe_sc(struct acq400_dev* adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	if (allocate_hbm(adev, nbuffers, bufferlen, default_dma_direction)){
		dev_err(DEVP(adev), "failed to allocate buffers");
		return -1;
	}
	init_gpg_buffer(adev, &sc_dev->gpg, GPG_MEM_BASE, GPG_DEBUG);

	acq400_createSysfs(DEVP(adev));
	acq400_init_proc(adev);
	acq2006_createDebugfs(adev);
	acq400sc_init_defaults(adev);
	if (IS_ACQ2106_WR(adev)){
		if (acq400_wr_init_irq(adev)){
			return -1;
		}
	}
	return 0;
}
int acq400_modprobe(struct acq400_dev* adev)
{
	adev->isFifoError = acq420_isFifoError;	/* REMOVE me: better default wanted */

	dev_info(DEVP(adev), "%s id %x %s", __FUNCTION__, GET_MOD_ID(adev), IS_SC(adev)? "SC": "MODULE");

	if (IS_SC(adev)){
		return acq400_modprobe_sc(adev);
	}if (IS_XO(adev) && xo_use_bigbuf){
		adev->hb = acq400_devices[0]->hb;
		dev_info(DEVP(adev), "site %d using MAIN HB", adev->of_prams.site);
	}else if (IS_AO424(adev)){
		if (allocate_hbm(adev, AO420_NBUFFERS,
				ao424_buffer_length, DMA_TO_DEVICE)){
			dev_err(DEVP(adev), "failed to allocate buffers");
			return -1;
		}
	}else if (IS_AO42X(adev) || IS_DIO432X(adev)){
		if (allocate_hbm(adev, AO420_NBUFFERS,
				ao420_buffer_length, DMA_TO_DEVICE)){
			dev_err(DEVP(adev), "failed to allocate buffers");
			return -1;
		}
	}

	if (acq400_mod_init_irq(adev)){
		return -1;
	}
	acq400_mod_init_defaults(adev);
	acq400_createSysfs(DEVP(adev));
	acq400_init_proc(adev);
	acq400_createDebugfs(adev);
	return 0;
}


void init_axi_dma(struct acq400_dev* adev)
{
	dev_info(DEVP(adev), "init_axi_dma() 01 %s %s",
			IS_AXI64(adev)? "AXI64": "",
			adev->axi_private == 0? "init now": "already done");

	if (IS_AXI64(adev) && adev->axi_private == 0){
		dev_info(DEVP(adev), "init_axi_dma() 10");
          	if (nbuffers-reserve_buffers < AXI_BUFFER_COUNT){
          		AXI_BUFFER_COUNT = nbuffers-reserve_buffers;
          		dev_warn(DEVP(adev),
          			".. not enough buffers limit to %d", nbuffers-reserve_buffers);
          	}
          	axi64_claim_dmac_channels(adev);
          	init_axi64_hbm(adev, AXI_BUFFER_COUNT);
          	axi64_init_dmac(adev);
	}
	dev_info(DEVP(adev), "init_axi_dma() 99");
}

static int acq400_probe(struct platform_device *pdev)
{
        int rc;
        struct resource *acq400_resource;
        struct acq400_dev* adev = acq400_allocate_dev(pdev);

        if (!adev){
        	dev_err(&pdev->dev, "unable to allocate device structure\n");
        	return -ENOMEM;
        }
        pdev->dev.id = ndevices;
        /* Get our platform device resources */
        dev_dbg(DEVP(adev), "id:%d We have %d resources\n", pdev->id, pdev->num_resources);
        acq400_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (acq400_resource == NULL) {
                dev_err(&pdev->dev, "No resources found\n");
                return -ENODEV;
        }

        if (acq400_device_tree_init(adev)){
        	rc = -ENODEV;
        	goto remove;
        }
        snprintf(adev->site_no, 4, "%d", adev->of_prams.site);
        snprintf(adev->dev_name, 16, "acq400.%d", adev->of_prams.site);
        adev->dev_physaddr = acq400_resource->start;
        adev->dev_addrsize = acq400_resource->end - acq400_resource->start + 1;

        dev_info(DEVP(adev), "request_mem_region(%x %x %s)", adev->dev_physaddr, adev->dev_addrsize, adev->dev_name);

        adev->dev_virtaddr = devm_ioremap_resource(&pdev->dev, acq400_resource);
        if (IS_ERR(adev->dev_virtaddr)){
        	dev_err(DEVP(adev), "failed to ioremap resource for %s", adev->dev_name);
        	rc = -ENODEV;
        	goto fail;
        }
        dev_info(DEVP(adev), "acq400: site_no:%s dev_name:%s mapped 0x%0x to 0x%0x\n",
        	adev->site_no, adev->dev_name,
        	adev->dev_physaddr, (unsigned int)adev->dev_virtaddr);

        adev = acq400_allocate_module_device(adev);

        if (IS_DUMMY(adev)){
        	acq400_createSysfs(&pdev->dev);
        	dev_info(DEVP(adev), "DUMMY device detected, quitting\n");
        	return 0;
        }

        rc = alloc_chrdev_region(&adev->devno, ACQ420_MINOR_0, ACQ420_MINOR_MAX, adev->dev_name);
        //status = register_chrdev_region(acq420_dev->devno, 1, MODULE_NAME);
        if (rc < 0) {
                dev_err(&pdev->dev, "unable to register chrdev\n");
                goto fail;
        }
        /* Register with the kernel as a character device */
        cdev_init(&adev->cdev, &acq400_fops);
        adev->cdev.owner = THIS_MODULE;
        rc = cdev_add(&adev->cdev, adev->devno, ACQ420_MINOR_MAX);
        if (rc < 0){
        	goto fail;
        }

        if (acq400_modprobe(adev) == 0){
        	return 0;
        }
 fail:
 	--ndevices;
       	dev_err(&pdev->dev, "Bailout!\n");
 remove:
        acq400_remove(pdev);
        return rc;
}

static int acq400_remove(struct platform_device *pdev)
/* undo all the probe things in reverse */
{
	if (pdev->id == -1){
		return -1;
	}else{
		struct acq400_dev* adev = acq400_devices[pdev->id];
		struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

		if (adev == 0){
			return -1;
		}
		axi64_free_dmac(adev);
		acq400_removeDebugfs(adev);
		hbm_free(&pdev->dev, &adev->EMPTIES);
		hbm_free(&pdev->dev, &adev->REFILLS);
		acq400_delSysfs(&adev->pdev->dev);
		acq400_del_proc(adev);
		dev_dbg(&pdev->dev, "cdev_del %p\n", &adev->cdev);
		cdev_del(&adev->cdev);
		unregister_chrdev_region(adev->devno, 1);

		/* Unmap the I/O memory */
		if (adev->dev_virtaddr) {
			iounmap(adev->dev_virtaddr);
			release_mem_region(adev->dev_physaddr,
					adev->dev_addrsize);
		}

		if (sc_dev->gpg.gpg_buffer){
			kfree(sc_dev->gpg.gpg_buffer);
		}

		kfree(adev->fifo_histo);
		kfree(adev);

		/** @todo What about other size, esp DIO482_PG_GPGMEM */
		return 0;
	}
}
static struct platform_driver acq400_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = acq400_of_match,
        },
        .probe = acq400_probe,
        .remove = acq400_remove,
};

static void __exit acq400_exit(void)
{
	platform_driver_unregister(&acq400_driver);
	acq400_module_remove_proc();
}

static int __init acq400_init(void)
{
        int status;

        DMA_TIMEOUT = DEFAULT_DMA_TIMEOUT;
	printk("D-TACQ ACQ400 FMC Driver %s\n", REVID);
	acq400_module_init_proc();
	a400fs_init();
        status = platform_driver_register(&acq400_driver);

        event_status_mask |= event0_feeds_ev_device? ADC_INT_CSR_EVENT0: 0;
        event_status_mask |= event1_feeds_ev_device? ADC_INT_CSR_EVENT1: 0;

        return status;
}

module_init(acq400_init);
module_exit(acq400_exit);


EXPORT_SYMBOL_GPL(acq400_devices);
EXPORT_SYMBOL_GPL(acq400_sites);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);




