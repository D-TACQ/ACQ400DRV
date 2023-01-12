/* ------------------------------------------------------------------------- */
/* acq400_axi_chain.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 29 Jul 2017  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2017 Peter Milne, D-TACQ Solutions Ltd         *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO 
 * TODO
 * ------------------------------------------------------------------------- */

/* IDEA: instead of making a backup then poisoning,
 * why not just make a backup, then check for differences ?
 * NO POISON required! or use a crc32. 
 * The problem is, this FAILS when data is completely repeatable (eg sim data).
 * */

#include "acq400.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"


unsigned AXI_POISON_OFFSET = 0;
module_param(AXI_POISON_OFFSET, uint, 0644);
MODULE_PARM_DESC(AXI_POISON_OFFSET, "DEBUG: locate POISON in buffer (0=END)");

int AXI_INIT_BUFFERS = 0;
module_param(AXI_INIT_BUFFERS, int, 0644);
MODULE_PARM_DESC(AXI_INIT_BUFFERS, "initialise buffers before start .. see exactly how far DMA got for debug");


extern int AXI_DEBUG_LOOPBACK_INDEX;


//#define NOPOISON 1

#ifdef NOPOISON
#warning NOPOISON variant handle with CARE
void clear_poison_all_buffers(struct acq400_dev *adev)				{}
int check_all_buffers_are_poisoned(struct acq400_dev *adev)
{
	return 1;
}
void poison_all_buffers(struct acq400_dev *adev)				{}

int poison_overwritten(struct acq400_dev *adev, struct HBM* hbm)
{
	return 1;
}

void poison_one_buffer(struct acq400_dev *adev, struct HBM* hbm)		{}
void poison_one_buffer_fastidious(struct acq400_dev *adev, struct HBM* hbm)	{}

int dma_done(struct acq400_dev *adev, struct HBM* hbm)
{
	return 1;
}

#else
void init_one_buffer(struct acq400_dev *adev, struct HBM* hbm)
{
	int ii;
	unsigned bufferlen = adev->bufferlen;
	int maxwords = bufferlen/USZ;
	u32* cursor = hbm->va;

	for (ii = 0; ii < maxwords; ++ii){
		cursor[ii] = ii;
	}
}

unsigned poison_offset(struct acq400_dev *adev)
{
	unsigned bufferlen = adev->bufferlen;

	if (likely(AXI_POISON_OFFSET == 0)){
		return bufferlen;
	}else{
		if (AXI_POISON_OFFSET > bufferlen) AXI_POISON_OFFSET = bufferlen;

		return AXI_POISON_OFFSET;
	}
}



void poison_one_buffer(struct acq400_dev *adev, struct HBM* hbm)
{
	unsigned po_bytes = poison_offset(adev);
	unsigned first_word = FIRST_POISON_WORD(po_bytes);
	unsigned p0 = hbm->va[first_word+0];
	unsigned p1 = hbm->va[first_word+1];

	if (p0 != POISON0){
		hbm->poison_data[0] = p0;
	}else{
		dev_warn(DEVP(adev), "%s refusing to stash poison buffer:%d", __FUNCTION__, hbm->ix);
	}
	if (p1 != POISON1){
		hbm->poison_data[1] = p1;
	}

	hbm->va[first_word+0] = POISON0;
	hbm->va[first_word+1] = POISON1;
	dma_sync_single_for_device(DEVP(adev),
				hbm->pa + po_bytes, POISON_SZ, hbm->dir);

	if (adev->rt.axi64_firstups == 0 && hbm->ix == 0){
		dev_dbg(DEVP(adev), "poison_one_buffer() poison applied at +%d bytes", first_word*USZ);
	}
}


void poison_one_buffer_fastidious(struct acq400_dev *adev, struct HBM* hbm)
{
	if (AXI_DEBUG_LOOPBACK_INDEX <= hbm->ix){
		poison_one_buffer(adev, hbm);
	}else{
		dev_dbg(DEVP(adev), "poison_one_buffer_fastidious() refuse to poison %d", hbm->ix);
	}
}

int poison_overwritten(struct acq400_dev *adev, struct HBM* hbm)
{
	unsigned po_bytes = poison_offset(adev);
	unsigned first_word = FIRST_POISON_WORD(po_bytes);

	dma_sync_single_for_cpu(DEVP(adev), hbm->pa + po_bytes, POISON_SZ, hbm->dir);
	return hbm->va[first_word+0] != POISON0 &&
	       hbm->va[first_word+1] != POISON1;
}

void clear_poison_from_buffer(struct acq400_dev *adev, struct HBM* hbm)
{
	unsigned po_bytes = poison_offset(adev);
	unsigned first_word = FIRST_POISON_WORD(po_bytes);

	if (!poison_overwritten(adev, hbm)){
		hbm->va[first_word+0] = hbm->poison_data[0];
		hbm->va[first_word+1] = hbm->poison_data[1];
	}
}

void _poison_all_buffers(struct acq400_dev *adev, int ichan)
{
	int ii;

	dev_dbg(DEVP(adev), "_poison_all_buffers(%d) : ndesc:%d", ichan, adev->axi64[ichan].ndesc);
	for (ii = 0; ii < adev->axi64[ichan].ndesc; ++ii){
		struct HBM* hbm = adev->axi64[ichan].axi64_hb[ii];
		if (AXI_INIT_BUFFERS){
			init_one_buffer(adev, hbm);
		}
		poison_one_buffer(adev, hbm);
	}
}
void poison_all_buffers(struct acq400_dev *adev)
{
	mutex_lock(&adev->list_mutex);
	_poison_all_buffers(adev, 0);
	_poison_all_buffers(adev, 1);
	mutex_unlock(&adev->list_mutex);
}

void _clear_poison_all_buffers(struct acq400_dev *adev, int ichan)
{
	int ii;

	for (ii = 0; ii < adev->axi64[ichan].ndesc; ++ii){
		clear_poison_from_buffer(adev, adev->axi64[ichan].axi64_hb[ii]);
	}
}
void clear_poison_all_buffers(struct acq400_dev *adev)
{
	mutex_lock(&adev->list_mutex);
	_clear_poison_all_buffers(adev, 0);
	_clear_poison_all_buffers(adev, 1);
	mutex_unlock(&adev->list_mutex);
}
int check_all_buffers_are_poisoned(struct acq400_dev *adev)
{
	struct HBM *cursor;
	int fails = 0;
	int pass = 0;
	mutex_lock(&adev->list_mutex);
	list_for_each_entry(cursor, &adev->EMPTIES, list){
		if (poison_overwritten(adev, cursor)){
			dev_err(DEVP(adev), "poison missing from %d", cursor->ix);
			++fails;
		}else{
			++pass;
		}
	}
	mutex_unlock(&adev->list_mutex);
	if (fails == 0){
		dev_info(DEVP(adev), "check_all_buffers_are_poisoned %d ALL GOOD", pass);
	}
	return fails;
}
int dma_done(struct acq400_dev *adev, struct HBM* hbm)
{
	int po = poison_overwritten(adev, hbm);
	dev_dbg(DEVP(adev), "poison_overwritten %d %s", hbm->ix, po? "YES": "NO <<<<<<<<<<<<<<<<<<");
	return po;
}
#endif
