/* ------------------------------------------------------------------------- */
/* hbm.c  		                     	 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 pgm, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Mar 23, 2013
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
#include "hbm.h"

static int getOrder(int len)
{
	int order;
	len /= PAGE_SIZE;

	for (order = 0; 1 << order < len; ++order) {
		;
	}
	return order;
}

struct HBM *hbm_allocate1(struct device *dev, int len, int ix,
			  enum dma_data_direction dir)
{
	int order = getOrder(len);
	struct HBM *hbm = kzalloc(sizeof(struct HBM), GFP_KERNEL);
	hbm->va = (void *)__get_free_pages(GFP_KERNEL, order);
	hbm->pa = dma_map_single(dev, hbm->va, len, dir);
	hbm->len = len;
	hbm->dir = dir;
	hbm->ix = ix;
	hbm->bstate = BS_EMPTY;
	return hbm;
}

void fillpa(struct HBM *hbm)
{
	int lenw = hbm->len / sizeof(unsigned);
	unsigned *cursor = (unsigned *)hbm->va;
	unsigned pa = hbm->pa;

	while (lenw--) {
		*cursor++ = pa;
		pa += sizeof(unsigned);
	}
}
int hbm_allocate(struct device *dev, int ix, int nbuffers, int len,
		 struct list_head *buffers, enum dma_data_direction dir)
{
	int order = getOrder(len);
	int ii;

	len = (1 << order) * PAGE_SIZE;

	for (ii = 0; ii < nbuffers; ++ii, ++ix) {
		struct HBM *hbm = kzalloc(sizeof(struct HBM), GFP_KERNEL);
		hbm->va = (void *)__get_free_pages(GFP_KERNEL, order);
		hbm->pa = dma_map_single(dev, hbm->va, len, dir);
		hbm->len = len;
		hbm->dir = dir;
		hbm->ix = ix;
		hbm->bstate = BS_EMPTY;
		list_add_tail(&hbm->list, buffers);

		fillpa(hbm);
	}

	return nbuffers;
}

int hbm_free(struct device *dev, struct list_head *buffers)
{
	struct HBM *hbm;
	struct HBM *temp;

	list_for_each_entry_safe(hbm, temp, buffers, list) {
		list_del(&hbm->list);
		dma_unmap_single(dev, hbm->pa, hbm->len, hbm->dir);
		free_pages((unsigned long)hbm->va, getOrder(hbm->len));
		kfree(hbm);
	}

	return 0;
}

int hbm_free_buffer_only(struct device *dev, struct list_head *buffers)
{
	struct HBM *hbm;
	struct HBM *temp;

	list_for_each_entry_safe(hbm, temp, buffers, list) {
		list_del(&hbm->list);
		dma_unmap_single(dev, hbm->pa, hbm->len, hbm->dir);
		free_pages((unsigned long)hbm->va, getOrder(hbm->len));
	}

	return 0;
}
