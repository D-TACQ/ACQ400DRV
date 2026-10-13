/* ------------------------------------------------------------------------- */
/* hbm.h  Host Buffer Mapping interface     	                     	     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 pgm, D-TACQ Solutions Ltd                            *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Mar 23, 2013                                                *
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

#ifndef HBM_H_
#define HBM_H_

struct list_head;

enum BSTATE {
	BS_EMPTY,
	BS_FILLING,
	BS_FULL,
	BS_FULL_APP,
	BS_RESERVED,
	_BS_MAX
};

struct HBM { /* Host Buffer Mapping */
	u32 *va;
	dma_addr_t pa;
	size_t len;
	short dir;
	short ix;
	enum BSTATE bstate;
	struct list_head list;
	unsigned poison_data[2]; /* store data here when poisoning */
};
struct HBM *hbm_allocate1(struct device *dev, int len, int ix,
			  enum dma_data_direction dir);
int hbm_allocate(struct device *dev, int ix, int nbuffers, int len,
		 struct list_head *buffers, enum dma_data_direction dir);
int hbm_free(struct device *dev, struct list_head *buffers);
int hbm_free_buffer_only(struct device *dev, struct list_head *buffers);
#endif /* HBM_H_ */
