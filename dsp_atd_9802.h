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

/** @file dsp_atd_908.h DESCR
 * 
 *  Created on: Feb 10, 2012
 *      Author: pgm
 */

#ifndef _DSP_ATD_9802_H_
#define _DSP_ATD_9802_H_

#define MAXSTACK 4

#undef DEVP
#undef PD
#undef PDSZ

#define GROUP_FIRST_N_TRIGGERS_ALL 0

#include "regfs.h"

struct ATD_9802_DEV {
	struct REGFS_DEV rdev;

	unsigned status[ATD_TRG_MAXREG];
	unsigned status_latch[ATD_TRG_MAXREG];
	unsigned group_status_latch[ATD_TRG_MAXREG];
	unsigned group_trigger_mask[ATD_TRG_MAXREG];
	unsigned group_first_n_triggers; /* trigger if N in the group are set. N=0 -> ALL */
	enum GSMODE { GS_NOW, GS_HISTORIC } gsmode;
	unsigned sample_count;
	unsigned latch_count;

	void *client; /* stash subclass data here */
	struct ATD atd; /* pulse timers */
	struct ATD soft_trigger;
};

#define ATD_DEVP(adev) (&adev->rdev.pdev->dev)

extern irqreturn_t (*regfs_isr)(int irq, void *dev_id);
extern int regfs_probe(struct platform_device *pdev);
extern int regfs_probe_rdev(struct platform_device *pdev,
			    struct REGFS_DEV *rdev);
extern int regfs_remove(struct platform_device *pdev);

#endif /* _DSP_ATD_9802_H_ */
