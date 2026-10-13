/* ------------------------------------------------------------------------- */
/* acq400_deltrg.c ACQ420_FMC						     */
/*
 * acq400_deltrg.c
 *
 *  Created on: 22 Jan 2016
 *      Author: pgm
 */

/* ------------------------------------------------------------------------- */
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

#define ATD_OFFSET(ch) ((ch) * sizeof(int) + AXI_ATD_RAM)

#define POSMAX 32767
#define NEGMAX -32768

int acq400_setDelTrg(struct acq400_dev *adev, int ch, int threshold)
{
	short A = POSMAX;
	short B = NEGMAX;
	unsigned wval;

	if (threshold > 0) {
		A = min(threshold, POSMAX);
	} else if (threshold < 0) {
		B = max(threshold, NEGMAX);
	}
	wval = A;
	wval <<= 16;
	wval |= ((unsigned)B) & 0x0000ffff;

	dev_dbg(DEVP(adev), "acq400_setDelTrg %d %08x = %08x", ch,
		ATD_OFFSET(ch), wval);
	acq400wr32(adev, ATD_OFFSET(ch), wval);
	return 0;
}
int acq400_getDelTrg(struct acq400_dev *adev, int ch, int *threshold)
{
	unsigned rval = acq400rd32(adev, ATD_OFFSET(ch));
	short A = rval >> 16;
	short B = rval & 0x0ffff;
	int th;

	if (A == POSMAX) {
		if (B == NEGMAX) {
			th = 0;
		} else {
			th = B;
		}
	} else {
		th = A;
	}
	if (threshold)
		*threshold = th;
	return 0;
}
int acq400_clearDelTrg(struct acq400_dev *adev)
{
	int ch;
	dev_dbg(DEVP(adev), "acq400_clearDelTrg(%d) ", adev->nchan_enabled);

	for (ch = 0; ch < adev->nchan_enabled; ++ch) {
		acq400_setDelTrg(adev, ch, 0);
	}
	acq400wr32(adev, DTD_CTRL, 1);
	return 0;
}

int acq400_clearDelTrgEvent(struct acq400_dev *adev)
{
	u32 ctrl = acq400rd32(adev, DTD_CTRL);

	acq400wr32(adev, DTD_CTRL, ctrl | DTD_CTRL_CLR);
	dev_dbg(DEVP(adev), "acq400_clearDelTrgEvent() 45");
	acq400wr32(adev, DTD_CTRL, ctrl &= ~DTD_CTRL_CLR);
	return 0;
}
