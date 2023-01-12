/*
 * xo_workers.c
 *
 *  Created on: 16 Feb 2021
 *      Author: pgm
 */


#include "acq400.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"
#include "acq400_ui.h"

#include "dmaengine.h"


int xo_distributor_sample_size = sizeof(unsigned);
module_param(xo_distributor_sample_size, int, 0644);
MODULE_PARM_DESC(xo_distributor_sample_size, "sample size in distributor set");

int xo_wait_site_stop = -1;
module_param(xo_wait_site_stop, int, 0644);
MODULE_PARM_DESC(xo_wait_site_stop, "hold off xo stop until this site has stopped");

int xo_verbose = 0;
module_param(xo_verbose, int, 0644);
MODULE_PARM_DESC(xo_verbose, "print detail debug");

extern int ao_auto_rearm(void *clidat);

static char* flags2str(unsigned flags)
{
	static char str[128];
	sprintf(str, "%s %s %s %s",
			flags&DMA_WAIT_EV0? "DMA_WAIT_EV0":"",
			flags&DMA_WAIT_EV1? "DMA_WAIT_EV1":"",
			flags&DMA_SET_EV0? "DMA_SET_EV0":"",
			flags&DMA_SET_EV1? "DMA_SET_EV1":""
	);
	return str;
}


void incr_push(struct acq400_dev *adev, struct XO_dev* xo_dev)
{
	xo_dev->AO_playloop.pull_buf += 1;
	if (xo_dev->AO_playloop.pull_buf == xo_dev->AO_playloop.push_buf){
		dev_err(DEVP(adev), "pull == push : buffer overrun");
	}
}

/* DS0 uses DMA with peripheral control */
#define DS0_PRI	1
#define DS0_EV	10

#define DMA_DS0_FLAGS \
	(DS0_EV << DMA_CHANNEL_EV0_SHL        | \
	 DMA_DST_NO_INCR | \
	 PRI_TO_CTRL_FLAGS(DS0_PRI) << DMA_CHANNEL_STARTS_WFP_SHL | \
	 PRI_TO_CTRL_FLAGS(DS0_PRI) << DMA_CHANNEL_ENDS_FLUSHP_SHL)


void _dma_async_issue_pending(struct acq400_dev *adev, struct dma_chan *chan, int line)
{
	dev_dbg(DEVP(adev), "xo_data_loop()#%d dma_async_issue_pending %d", line, chan->chan_id);
	dma_async_issue_pending(chan);			\
	++adev->stats.xo.dma_buffers_out;
}

#define XO_MAX_POLL 100

int waitXoFifoEmpty(struct acq400_dev *adev)
{
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int pollcat = 0;
	int s1, s0 = 0;
	while ((s1 = xo_dev->xo.getFifoSamples(adev)) > 0){
		msleep(20);
		if (s1 == s0 || ++pollcat > XO_MAX_POLL){
			dev_err(DEVP(adev), "TIMEOUT waiting for XO FIFO EMPTY");
			return -1;
		}
		s0 = s1;
	}
	dev_dbg(DEVP(adev), "waitXoFifoEmpty() DAC_SAMPLE_CTR 0x%08x",
					acq400rd32(adev, DAC_SAMPLE_CTR));
	return 0;
}

void _waitOtherSiteStop(struct acq400_dev *adev, int site)
{
	struct acq400_dev *adev_wait = acq400_sites[site];

	while(adev_wait->stats.run){
		if (xo_wait_site_stop == -1){
			dev_warn(DEVP(adev), "_waitOtherSiteStop() external ABORT");
			break;
		}else{
			msleep(10);
		}
	}

}

void waitOtherSiteStop(struct acq400_dev *adev)
{
	int site;
	if ((site = xo_wait_site_stop) == -1){
		return;
	}else{
		_waitOtherSiteStop(adev, site);
		dev_dbg(DEVP(adev), "waitSiteStop() site:%d stopped", site);
	}
}


static const unsigned wflags[2] = { DMA_WAIT_EV0, DMA_WAIT_EV1 };
static const unsigned sflags[2] = { DMA_SET_EV1,  DMA_SET_EV0  };
static const unsigned xflags[2] = { DMA_WAIT_EV0|DMA_SET_EV1,  DMA_WAIT_EV1|DMA_SET_EV0  };


#define NOFL nflags
#define WFEV wflags		/* Wait For EV 	*/
#define STEV sflags 		/* Set EV 	*/
#define WFST xflags		/* Wait For EV, Set EV */


#define DMA_ASYNC_PUSH(lvar, adev, chan, hbm, flags)	do {		\
	unsigned _flags = DMA_DS0_FLAGS|(flags[chan]);			\
	lvar = dma_async_memcpy_callback(adev->dma_chan[chan], 		\
			FIFO_PA(adev0), hbm->pa, hbm->len, 	\
			_flags, acq400_dma_callback, adev);		\
	dev_dbg(DEVP(adev), "DMA_ASYNC_PUSH #%d [%d] ix:%d pa:0x%08x len:0x%08x %s",\
		__LINE__, chan, hbm->ix, hbm->pa, hbm->len, flags2str(_flags)); \
	} while(0)

#define DMA_ASYNC_ISSUE_PENDING(chan) _dma_async_issue_pending(adev, chan, __LINE__)

#define DMA_COUNT_IN do { 				\
		++adev->stats.xo.dma_buffers_in;	\
		dev_dbg(DEVP(adev), "DMA_COUNT_IN %d", adev->stats.xo.dma_buffers_in); \
	} while(0)


void xo_data_loop_stats_init(struct acq400_dev *adev)
{
	adev->stats.shot++;
	adev->stats.run = 1;
	adev->stats.xo.dma_buffers_out =
			adev->stats.xo.dma_buffers_in = 0;
}
void xo_data_loop_cleanup(struct acq400_dev *adev, long dma_timeout)
{
	struct acq400_dev *adev0 = acq400_devices[0];

	dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);

	while (adev->stats.xo.dma_buffers_in < adev->stats.xo.dma_buffers_out){
		if (wait_event_interruptible_timeout(
			adev->DMA_READY,
			adev->dma_callback_done, dma_timeout) <= 0){
			dev_err(DEVP(adev), "TIMEOUT waiting for DMA %d\n", __LINE__);
		}
		--adev->dma_callback_done;
		DMA_COUNT_IN;
		dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
	}

	dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
	waitXoFifoEmpty(adev);
	dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
	waitOtherSiteStop(adev);

	{
		struct acq400_sc_dev* sc_dev = container_of(adev0, struct acq400_sc_dev, adev);
		acq400_visit_set(sc_dev->distributor_set, adev->onStop);
	}

	dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);

	adev->stats.completed_shot = adev->stats.shot;
	adev->stats.run = 0;
	adev->task_active = 0;
}


int xo_data_loop(void *data)
/** xo_data_loop() : outputs using distributor and PRI on SC, but loop is
 * actually associated with the master site
 * (usually 1, where playloop_length is set)
 * hence: adev: master site, adev0: distributor access.
 */
{
//	static const unsigned nflags[2] = { 0, 0 };

	struct acq400_dev *adev = (struct acq400_dev *)data;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct acq400_dev *adev0 = acq400_devices[0];
	struct HBM** hbm0 = adev0->hb;

	int ic = 0;
#define IB 	(xo_dev->AO_playloop.pull_buf)
	int ao_samples_per_hb = hbm0[firstDistributorBuffer()]->len / xo_distributor_sample_size;
#define IBINCR	incr_push(adev, xo_dev)
#define IBRESET do { IB = firstDistributorBuffer(); } while(0)

	long dma_timeout = START_TIMEOUT;
	int maxlen = max(xo_dev->AO_playloop.maxlen, xo_dev->AO_playloop.length);
	int shot_buffer_count0 = maxlen/ao_samples_per_hb;
	int shot_buffer_count = shot_buffer_count0;

#define AO_CONTINUOUS	(xo_dev->AO_playloop.oneshot == AO_continuous)
#define LAST_PUSH \
	(!AO_CONTINUOUS && adev->stats.xo.dma_buffers_out+1 >= shot_buffer_count)

	int continuous_at_start = AO_CONTINUOUS;
	int last_push_done = 0;

	dev_dbg(DEVP(adev), "xo_data_loop() ib set %d playloop:%d hbs:%d shot_buffer_count:%d",
				IB, xo_dev->AO_playloop.length, ao_samples_per_hb, shot_buffer_count);

	if (shot_buffer_count*ao_samples_per_hb < xo_dev->AO_playloop.length){
		shot_buffer_count += 1;
		dev_dbg(DEVP(adev), "ao play data buffer overspill");
	}
	IBRESET;
	xo_data_loop_stats_init(adev);

	go_rt(MAX_RT_PRIO-4);
	adev->task_active = 1;

	/* prime the DMAC with buffers 0 and 1 ready to go.
	 * 0 starts filling right away
	 * */
	if (shot_buffer_count > 1){
		DMA_ASYNC_PUSH(adev->dma_cookies[1], adev, 1, hbm0[IB+1], WFST);
		DMA_ASYNC_PUSH(adev->dma_cookies[0], adev, 0, hbm0[IB+0], STEV);
		IBINCR;
		IBINCR;

		DMA_ASYNC_ISSUE_PENDING(adev->dma_chan[1]);
	}else{
		dev_warn(DEVP(adev), "Single buffer won't work");
		DMA_ASYNC_PUSH(adev->dma_cookies[0], adev, 0, hbm0[IB], STEV);
		IBINCR;
	}

	sc_data_engine_reset_enable(DATA_ENGINE_1);
	DMA_ASYNC_ISSUE_PENDING(adev->dma_chan[0]);

	dev_dbg(DEVP(adev), "xo_data_loop() 01 :out:%d in:%d",
			adev->stats.xo.dma_buffers_out, adev->stats.xo.dma_buffers_in);

	for(xo_dev->AO_playloop.cursor = 0;
	    xo_dev->AO_playloop.cursor < xo_dev->AO_playloop.length && !kthread_should_stop();
	    	    	    ic = !ic, dma_timeout = DMA_TIMEOUT){
		if (wait_event_interruptible_timeout(
				adev->DMA_READY,
				adev->dma_callback_done || kthread_should_stop(),
				dma_timeout) <= 0){
			dev_err(DEVP(adev), "TIMEOUT waiting for DMA %d\n", __LINE__);
			goto quit;
		}


		if (kthread_should_stop()){
			dev_info(DEVP(adev), "kthread_should_stop");
			goto quit;
		}else if (adev->dma_callback_done){
			--adev->dma_callback_done;
			DMA_COUNT_IN;
		}else {
			dev_err(DEVP(adev), "here with no callback int should not happen ..");
		}

		if (xo_verbose) dev_dbg(DEVP(adev), " 22 calling dma_sync_wait() ..");

		if(dma_sync_wait(adev->dma_chan[ic], adev->dma_cookies[ic]) != DMA_SUCCESS){
			dev_err(DEVP(adev), "dma_sync_wait TIMEOUT cursor:%d chan:%d timeout:%ld",
					xo_dev->AO_playloop.cursor, ic, dma_timeout);
			goto quit;
		}
		if (xo_verbose) dev_dbg(DEVP(adev), "44 back from dma_sync_wait() oneshot:%d", xo_dev->AO_playloop.oneshot);

		xo_dev->AO_playloop.cursor += ao_samples_per_hb;

		if (last_push_done && continuous_at_start){
			dev_dbg(DEVP(adev), "continuous going down ..");
			goto quit;
		}
		if (adev->stats.xo.dma_buffers_out >= shot_buffer_count){
			xo_dev->AO_playloop.cycles++;

			if (AO_CONTINUOUS || xo_dev->AO_playloop.repeats > 0){
				shot_buffer_count += shot_buffer_count0;
				IBRESET;
				xo_dev->AO_playloop.cursor = 0;
				/** @@todo dodgy : output or input dep on mode */
				if (AO_CONTINUOUS){
					xo_dev->AO_playloop.repeats++;
				}else{
					--xo_dev->AO_playloop.repeats;
				}
			}
		}
		if (adev->stats.xo.dma_buffers_out < shot_buffer_count){
			int cc;
			if (LAST_PUSH){
				DMA_ASYNC_PUSH(cc, adev, ic, hbm0[IB], WFEV);
				last_push_done = 1;
			}else{
				DMA_ASYNC_PUSH(cc, adev, ic, hbm0[IB], WFST);
			}
			adev->dma_cookies[ic] = cc;
			IBINCR;
			DMA_ASYNC_ISSUE_PENDING(adev->dma_chan[ic]);
		}
		yield();
		if (xo_verbose) dev_dbg(DEVP(adev), "66 oneshot:%d", xo_dev->AO_playloop.oneshot);
	}

quit:
	dev_dbg(DEVP(adev), "xo_data_loop() quit out:%d in:%d oneshot:%d",
			adev->stats.xo.dma_buffers_out, adev->stats.xo.dma_buffers_in,
			xo_dev->AO_playloop.oneshot);

	xo_data_loop_cleanup(adev, dma_timeout);

	dev_dbg(DEVP(adev), "88 oneshot:%d", xo_dev->AO_playloop.oneshot);

	if (xo_dev->AO_playloop.oneshot == AO_oneshot_rearm &&
	    (xo_dev->AO_playloop.maxshot==0 || adev->stats.shot < xo_dev->AO_playloop.maxshot)){
		dev_dbg(DEVP(adev), "xo_data_loop() spawn auto_rearm");
		kthread_run(ao_auto_rearm, adev, "%s.awgrearm", adev->dev_name);
	}
	dev_dbg(DEVP(adev), "xo_data_loop() 99 out:%d in:%d",
			adev->stats.xo.dma_buffers_out, adev->stats.xo.dma_buffers_in);
	if (adev->stats.xo.dma_buffers_out != adev->stats.xo.dma_buffers_in){
		dev_err(DEVP(adev), "xo_data_loop() 99 out:%d in:%d",
				adev->stats.xo.dma_buffers_out, adev->stats.xo.dma_buffers_in);
	}
	return 0;
}

unsigned distributor_underrun(struct acq400_dev *adev0)
{
	unsigned dissta = acq400rd32(adev0, DISTRIBUTOR_STA);
	if ((dissta&DISSTA_FIFO_ANYSKIP) != 0){
		dev_err(DEVP(adev0), "AWG LOSS OF DATA DETECTED %d 0x%08x\n", __LINE__, dissta);
		return 1;
	}else{
		return 0;
	}
}


void dump_buffer(struct acq400_dev *adev, struct BQ* bq, char *id)
{

	int cursor = bq->tail;
	int bx[32];
	int nb;

	for (nb = 0; cursor != bq->head; cursor = BQ_incr(bq, cursor), ++nb){
		if (nb >= 32){
			break;
		}else{
			bx[nb] = bq->buf[cursor];
		}
	}

	{
		static char text[132];
		char* cc = text;
		int ii;

		text[0] = '\0';

		for (ii = 0; ii < nb; ++ii){
			cc += snprintf(cc, 132, "%03d ", bx[ii] );
		}
		dev_info(DEVP(adev), "%s [%d] %s", id, nb, text);
	}

}
#define MAX_STALL_RETRIES	20
#define STALL_TO_MS		10
#define STALL_BIN_WIDTH		2


int streamdac_stalls[MAX_STALL_RETRIES/STALL_BIN_WIDTH+1];
int streamdac_stalls_count = MAX_STALL_RETRIES/STALL_BIN_WIDTH+1;
module_param_array(streamdac_stalls, int, &streamdac_stalls_count, 0444);


int streamdac_data_loop(void *data)
{
	struct acq400_path_descriptor* pdesc = (struct acq400_path_descriptor*)(data);
	struct acq400_dev *adev0 = acq400_devices[0];
	struct acq400_sc_dev* sc_dev = container_of(adev0, struct acq400_sc_dev, adev);
	struct acq400_dev *adev = sc_dev->distributor_set[0];
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct HBM** hbm0 = adev0->hb;
	struct BQ* refills = &sc_dev->stream_dac.refills;
	struct BQ* empties = &pdesc->bq;
	int rc = 0;
	int ic = 0;
	int running = 0;
	long dma_timeout = START_TIMEOUT;
	enum { LP_IDLE, LP_REQUEST, LP_DONE } last_push = LP_IDLE;
	int stall_count = 0;
	int ab[2];				/* buffer num, index by ic */
	struct HBM* hbm;

	dev_info(DEVP(adev0), "streamdac_data_loop 01");
	xo400_reset_playloop(adev, XO400_PLAYCONTINUOUS);
	xo400_getDMA(adev);

	xo_data_loop_stats_init(adev);
	memset(streamdac_stalls, 0, sizeof(streamdac_stalls));

	go_rt(MAX_RT_PRIO-4);
	adev->task_active = 1;

	for (ic = 0; last_push != LP_DONE; ic = !ic){
		dev_dbg(DEVP(adev0), "streamdac_data_loop() wait_event");
		if (!running){
			if (wait_event_interruptible(
				sc_dev->stream_dac.sd_waitq,
				BQ_count(refills)>1 || kthread_should_stop())){
				dev_err(DEVP(adev0), "streamdac_data_loop %d", __LINE__);
				rc = -EINTR;
				goto quit;
			}else if (kthread_should_stop()){
				goto quit;
			}
			ab[0] = BQ_get(DEVP(adev), refills); hbm = hbm0[ab[0]];
			dma_sync_single_for_device(DEVP(adev), hbm->pa, hbm->len, DMA_TO_DEVICE);

			ab[1] = BQ_get(DEVP(adev), refills); hbm = hbm0[ab[1]];
			dma_sync_single_for_device(DEVP(adev), hbm->pa, hbm->len, DMA_TO_DEVICE);

			DMA_ASYNC_PUSH(adev->dma_cookies[1], adev, 1, hbm0[ab[1]], WFST);
			DMA_ASYNC_PUSH(adev->dma_cookies[0], adev, 0, hbm0[ab[0]], STEV);
			DMA_ASYNC_ISSUE_PENDING(adev->dma_chan[1]);
			sc_data_engine_reset_enable(DATA_ENGINE_1);
			DMA_ASYNC_ISSUE_PENDING(adev->dma_chan[0]);
			running = 1;
		}
		if (xo_verbose > 1){
			dump_buffer(adev, refills, "refills");
			dev_info(DEVP(adev), "%s [%d] %03d %03d", "inflite", 2, ab[ic], ab[!ic]);
			dump_buffer(adev, empties, "empties");
		}

		if (wait_event_interruptible_timeout(
				adev->DMA_READY, adev->dma_callback_done, dma_timeout) <= 0){
			dev_err(DEVP(adev0), "TIMEOUT waiting for DMA %d\n", __LINE__);
			goto quit;
		}else{
			if (adev->dma_callback_done){
				--adev->dma_callback_done;
				DMA_COUNT_IN;
			}
		}

		if(dma_sync_wait(adev->dma_chan[ic], adev->dma_cookies[ic]) != DMA_SUCCESS){
			dev_err(DEVP(adev0), "dma_sync_wait TIMEOUT chan:%d timeout:%ld",
					ic, dma_timeout);
			goto quit;
		}

		while (BQ_count(refills) < 1){
			if (++stall_count == MAX_STALL_RETRIES || kthread_should_stop()){
				dev_err(DEVP(adev0), "STALL quitting %s\n",
						kthread_should_stop()? "STOP REQUEST": "RETRIES");
				last_push = LP_REQUEST;
				break;
			}
			msleep(STALL_TO_MS+stall_count*2);
		}
		streamdac_stalls[stall_count/STALL_BIN_WIDTH]++;
		stall_count = 0;


		xo_dev->AO_playloop.pull_buf = ab[ic];
		BQ_put(DEVP(adev), empties, ab[ic]);

		if (last_push == LP_IDLE){
			/* BQ_get ONLY if data .. test last_push is a faster proxy for BQ_count(refills) */
			ab[ic] = BQ_get(DEVP(adev), refills); hbm = hbm0[ab[ic]];
			dma_sync_single_for_device(DEVP(adev), hbm->pa, hbm->len, DMA_TO_DEVICE);
		}

		if (last_push == LP_REQUEST || kthread_should_stop() || distributor_underrun(adev0)){
			DMA_ASYNC_PUSH(adev->dma_cookies[ic], adev, ic, hbm, WFEV);
			last_push = LP_DONE;
		}else{
			DMA_ASYNC_PUSH(adev->dma_cookies[ic], adev, ic, hbm, WFST);
		}
		DMA_ASYNC_ISSUE_PENDING(adev->dma_chan[ic]);
		wake_up_interruptible(&pdesc->waitq);
	}
quit:
	BQ_clear(refills);
	xo_data_loop_cleanup(adev, dma_timeout);
	xo400_reset_playloop(adev, 0);
	WORKER_DONE(pdesc) = 1;
	wake_up_interruptible(&pdesc->waitq);

	dev_info(DEVP(adev0), "streamdac_data_loop 99 last_push:%d", last_push);
	return rc;
}

