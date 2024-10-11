/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2011 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    http://www.d-tacq.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file regfs.h DESCR
 * 
 *  Created on: Feb 10, 2012
 *      Author: pgm
 */

#ifndef REGFS_DEV_H_
#define REGFS_DEV_H_

#define MAXSTACK 4

#undef DEVP
#undef PD
#undef PDSZ

#define GROUP_FIRST_N_TRIGGERS_ALL	0

struct REGFS_DEV {
	void* va;
	struct platform_device* pdev;
	struct resource *mem;

	int istack;
	struct dentry *dstack[MAXSTACK];
	struct dentry *top;
	struct dentry *create_hook;


	struct cdev cdev;
	struct list_head list;
	wait_queue_head_t w_waitq;

	unsigned ints;
	unsigned status;
	unsigned status_latch;
	unsigned group_status_latch[ATD_TRG_MAXREG];
	unsigned group_trigger_mask[ATD_TRG_MAXREG];
	unsigned group_first_n_triggers[ATD_TRG_MAXREG];      /* trigger if N in the group are set. N=0 -> ALL */
	enum GSMODE { GS_NOW, GS_HISTORIC } gsmode;
	unsigned sample_count;
	unsigned latch_count;
	unsigned event_client_pid;
	unsigned client_ready;	/* client requests interrupt status. isr: DO NOT update unless set. */

	void* client;				/* stash subclass data here */
	struct ATD atd;				/* pulse timers */
	struct ATD soft_trigger;
};

extern irqreturn_t (*regfs_isr)(int irq, void *dev_id);
extern int regfs_probe(struct platform_device *pdev);
extern int regfs_remove(struct platform_device *pdev);

#endif /* REGFS_DEV_H_ */
