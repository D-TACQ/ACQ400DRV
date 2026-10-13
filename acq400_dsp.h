/* ------------------------------------------------------------------------- */
/* acq400_dsp.h  D-TACQ ACQ400 DSP top level defs	                     */
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

/*
 * acq400_dsp.h
 *
 *  Created on: 2 Aug 2019
 *      Author: pgm
 */

#ifndef ACQ400_DSP_H_
#define ACQ400_DSP_H_

struct acq400_base_dev {
	dev_t devno;
	struct cdev cdef;
	struct platform_device *pdev;
	char dev_name[8];
	int site;
	int irq;
	u32 mod_id;

	/* Hardware device constants */
	u32 dev_physaddr;
	void *dev_virtaddr;
	u32 dev_addrsize;
};

extern int acq400dsp_devicetree_init(struct platform_device *pdev,
				     struct device_node *of_node);

#endif /* ACQ400_DSP_H_ */
