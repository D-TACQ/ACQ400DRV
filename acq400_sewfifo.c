/*
 * acq400_sewfifo.c
 *
 *  Created on: 28 Sep 2014
 *      Author: pgm
 */
/* ------------------------------------------------------------------------- */
/* acq400_sewfifo.c  D-TACQ ACQ400 FMC  DRIVER		                     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 * Copyright 2002, 2003 Jonathan Corbet <corbet@lwn.net>
 * This file may be redistributed under the terms of the GNU GPL.
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


int sew_fifo_msec = 10;
module_param(sew_fifo_msec, int, 0644);
MODULE_PARM_DESC(ndevices, "rate to service sew_fifo at");

int sew_stats[4];
module_param_array(sew_stats, int, NULL, 0444);
MODULE_PARM_DESC(sew_stats, "payload per fifo_work update entries in [0] mean the wf is faster than the data");

#define SEW_FIFO_LEN	4096


#define PAYLOAD_EMPTY	' '


int acq400_sew_fifo_work(void *data)
/* runs at fixed interval sew_fifo_msec
 * dequeues up to 3 bytes each interval.
 * embeds 4 bytes every time.
 * b[0] = count:6|payload_count:2, b[1], b[2], b[3] = payload
 */
{
	struct SewFifo* sf = (struct SewFifo*)data;
	struct acq400_dev* adev = sf->adev;
	union {
		u32 lw;
		u8 bytes[4];
	} xx = {};
	unsigned modulo_count = 0;
	wait_queue_head_t local_waitq;

	init_waitqueue_head(&local_waitq);
	memset(sew_stats, 0, sizeof(sew_stats));

	for (; !kthread_should_stop();
		xx.bytes[1] = xx.bytes[2] = xx.bytes[3] = PAYLOAD_EMPTY,
		modulo_count = (modulo_count+1)&0x3f){

		if (wait_event_interruptible_timeout(
			local_waitq, false, msecs_to_jiffies(sew_fifo_msec)) == 0){
			int payload_count = min(buf_count(&sf->sf_buf, SEW_FIFO_LEN), 3);
			int ipay;
			xx.bytes[0] = modulo_count<<2 | payload_count;
			for (ipay = 1; ipay <= payload_count; ++ipay){
				xx.bytes[ipay] = buf_get(&sf->sf_buf, SEW_FIFO_LEN);
			}
			acq400wr32(adev, sf->regoff, xx.lw);
			sew_stats[payload_count]++;
			wake_up_interruptible(&sf->sf_waitq);
		}else{
			break;
		}
	}
	return 0;
}

void acq400_sew_fifo_init(struct acq400_dev* adev, int ix)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct SewFifo* sf = &sc_dev->sewFifo[ix];

	init_waitqueue_head(&sf->sf_waitq);
	if (sf->sf_buf.buf == 0){
		sf->sf_buf.buf = kzalloc(SEW_FIFO_LEN, GFP_KERNEL);
	}
	sf->sf_buf.head = sf->sf_buf.tail = 0;
	sf->adev = adev;
	sf->regoff = ix? ACQ435_SW_EMB_WORD2: ACQ435_SW_EMB_WORD1;
	sf->sf_task = kthread_run(acq400_sew_fifo_work, sf,
						"%s.sf", adev->dev_name);
}
int acq400_sew_fifo_destroy(struct acq400_dev* adev, int ix)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct SewFifo* sf = &sc_dev->sewFifo[ix];
	kthread_stop(sf->sf_task);
	sf->sf_task = 0;
	if (sf->sf_buf.buf != 0){
		kfree(sf->sf_buf.buf);
		sf->sf_buf.buf = 0;
	}
	return 0;
}
int acq400_sew_fifo_write_bytes(
		struct acq400_dev* adev, int ix, const char __user *buf, size_t count)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct SewFifo* sf = &sc_dev->sewFifo[ix];
	unsigned char tmp;
	int iwrite = 0;
	int rc;
	for(; iwrite < count; ++iwrite){
		if (get_user(tmp, buf+iwrite)){
			return -EFAULT;
		}

		rc = wait_event_interruptible_timeout(
				sf->sf_waitq,
				buf_space(&sf->sf_buf, SEW_FIFO_LEN),
				20*msecs_to_jiffies(sew_fifo_msec));
		if (rc < 0){
			return -ERESTARTSYS;
		}
		buf_put(&sf->sf_buf, tmp, SEW_FIFO_LEN);
	}
	return iwrite;
}
