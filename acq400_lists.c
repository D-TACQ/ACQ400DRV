/* ------------------------------------------------------------------------- *
 * acq400_lists.c  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 9 Mar 2014  
 *    Author: pgm                                                         
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
#include "acq400_lists.h"
#include <linux/list_sort.h>

int run_buffers = 0;
module_param(run_buffers, int, 0644);
MODULE_PARM_DESC(run_buffers,
		 "#buffers to process in continuous (0: infinity)");

struct HBM *_getEmpty(struct acq400_dev *adev)
/* call ONLY inside mutex */
{
	if (!list_empty(&adev->EMPTIES)) {
		struct HBM *hbm =
			list_first_entry(&adev->EMPTIES, struct HBM, list);
		if (likely(hbm != 0)) {
			list_move_tail(&hbm->list, &adev->INFLIGHT);
			hbm->bstate = BS_FILLING;
			++adev->rt.nget;
			adev->rt.getEmptyErrors.report_active = 0;
		} else {
			dev_err(DEVP(adev), "getEmpty() NULL HBM");
		}
		return hbm;
	} else {
		if (!adev->rt.getEmptyErrors.report_active) {
			dev_warn(DEVP(adev),
				 "get Empty: EMPTIES Q is EMPTY!\n");
			adev->rt.getEmptyErrors.report_active = 1;
		}
		++adev->rt.getEmptyErrors.errors;
		return 0;
	}
}

struct HBM *getEmpty(struct acq400_dev *adev)
{
	struct HBM *hbm = 0;
	mutex_lock(&adev->list_mutex);
	hbm = _getEmpty(adev);
	mutex_unlock(&adev->list_mutex);
	return hbm;
}

int multipleEmptiesWaiting(struct acq400_dev *adev)
{
	return !list_empty(&adev->EMPTIES) && !list_is_singular(&adev->EMPTIES);
}

int _putFull(struct acq400_dev *adev)
/* called with MUTEX locked */
{
	if (!list_empty(&adev->INFLIGHT)) {
		struct HBM *hbm =
			list_first_entry(&adev->INFLIGHT, struct HBM, list);
		hbm->bstate = BS_FULL;
		list_move_tail(&hbm->list, &adev->REFILLS);
		return 0;
	} else {
		return -1;
	}
}

void putFull(struct acq400_dev *adev)
{
	int rc;
	mutex_lock(&adev->list_mutex);
	rc = _putFull(adev);
	mutex_unlock(&adev->list_mutex);

	if (rc == 0) {
		++adev->rt.nput;
		if (run_buffers && adev->rt.nput >= run_buffers) {
			adev->rt.please_stop = 1;
		}
		dev_dbg(DEVP(adev), "putFull() wake refill_ready %p",
			&adev->refill_ready);
		wake_up_interruptible(&adev->refill_ready);
		adev->rt.putFullErrors.report_active = 0;
	} else {
		if (!adev->rt.putFullErrors.report_active) {
			dev_warn(DEVP(adev), "putFull: INFLIGHT Q is EMPTY!\n");
			adev->rt.putFullErrors.report_active = 1;
		}
		++adev->rt.putFullErrors.errors;
		msleep(10);
	}
}

struct HBM *getEmptyFromRefills(struct acq400_dev *adev)
{
	if (!list_empty(&adev->REFILLS)) {
		struct HBM *hbm;
		mutex_lock(&adev->list_mutex);
		hbm = list_first_entry(&adev->REFILLS, struct HBM, list);
		list_move_tail(&hbm->list, &adev->INFLIGHT);
		hbm->bstate = BS_FILLING;
		mutex_unlock(&adev->list_mutex);

		++adev->rt.ngetr;
		return hbm;
	} else {
		dev_warn(DEVP(adev), "getEmptyFromRefills: Q is EMPTY!\n");
		return 0;
	}
}

int __getFull(struct acq400_dev *adev, struct HBM **first, int wait)
{
	struct HBM *hbm;

	mutex_lock(&adev->list_mutex);
	hbm = list_first_entry(&adev->REFILLS, struct HBM, list);
	list_move_tail(&hbm->list, &adev->OPENS);
	hbm->bstate = BS_FULL_APP;
	mutex_unlock(&adev->list_mutex);

	if (first)
		*first = hbm;

	dev_dbg(DEVP(adev), "__getFull() %d %s", hbm->ix, wait ? "WAITED" : "");
	return GET_FULL_OK;
}
int getFull(struct acq400_dev *adev, struct HBM **first, int wait)
{
	if (list_empty(&adev->REFILLS)) {
		if (!wait) {
			return -EINTR;
		}
	} else {
		return __getFull(adev, first, 0);
	}
	dev_dbg(DEVP(adev), "getFull() wait event:%p", &adev->refill_ready);
	if (wait_event_interruptible(adev->refill_ready,
				     !list_empty(&adev->REFILLS) ||
					     adev->rt.refill_error ||
					     adev->rt.please_stop)) {
		return -EINTR;
	} else if (adev->rt.please_stop) {
		return GET_FULL_DONE;
	} else if (adev->rt.refill_error) {
		dev_err(DEVP(adev), "getFull() return GET_FULL_REFILL_ERR");
		return GET_FULL_REFILL_ERR;
	} else {
		return __getFull(adev, first, wait);
	}
}

void putEmpty(struct acq400_dev *adev)
{
	struct HBM *hbm;
	mutex_lock(&adev->list_mutex);
	hbm = list_first_entry(&adev->OPENS, struct HBM, list);
	hbm->bstate = BS_EMPTY;
	adev->onPutEmpty(adev, hbm);
	list_move_tail(&hbm->list, &adev->EMPTIES);
	mutex_unlock(&adev->list_mutex);
}

struct HBM *ao_getEmpty(struct acq400_dev *adev)
{
	if (!list_empty(&adev->EMPTIES)) {
		struct HBM *hbm;
		mutex_lock(&adev->list_mutex);
		hbm = list_first_entry(&adev->EMPTIES, struct HBM, list);
		list_move_tail(&hbm->list, &adev->INFLIGHT);
		hbm->bstate = BS_FILLING;
		mutex_unlock(&adev->list_mutex);

		++adev->rt.nget;
		return hbm;
	} else {
		return 0;
	}
}

void ao_putFull(struct acq400_dev *adev, struct HBM *hbm_full)
{
	mutex_lock(&adev->list_mutex);
	hbm_full->bstate = BS_FULL;
	list_move_tail(&hbm_full->list, &adev->REFILLS);
	mutex_unlock(&adev->list_mutex);
	++adev->rt.nput;
	wake_up_interruptible(&adev->w_waitq);
}

struct HBM *ao_getFull(struct acq400_dev *adev)
{
	struct HBM *hbm = 0;
	if (!list_empty(&adev->REFILLS)) {
		mutex_lock(&adev->list_mutex);
		hbm = list_first_entry(&adev->REFILLS, struct HBM, list);
		list_del(&hbm->list);
		mutex_unlock(&adev->list_mutex);
	}

	return hbm;
}

void ao_putEmpty(struct acq400_dev *adev, struct HBM *hbm_empty)
{
	mutex_lock(&adev->list_mutex);
	hbm_empty->bstate = BS_EMPTY;
	list_add_tail(&hbm_empty->list, &adev->EMPTIES);
	mutex_unlock(&adev->list_mutex);
}

void move_list_to_empty(struct acq400_dev *adev, struct list_head *elist)
{
	struct HBM *cur;
	struct HBM *tmp;
	mutex_lock(&adev->list_mutex);
	list_for_each_entry_safe(cur, tmp, elist, list) {
		cur->bstate = BS_EMPTY;
		list_move_tail(&cur->list, &adev->EMPTIES);
	}
	mutex_unlock(&adev->list_mutex);
}
/** return number of available buffers. ALWAYS OMIT #0 */
int move_list_to_stash(struct acq400_dev *adev, struct list_head *elist)
{
	struct HBM *cur;
	struct HBM *tmp;
	int usable_buffers = 0;
	mutex_lock(&adev->list_mutex);
	list_for_each_entry_safe(cur, tmp, elist, list) {
		cur->bstate = BS_EMPTY;
		if (cur->ix != 0) {
			++usable_buffers;
		}
		list_move_tail(&cur->list, &adev->STASH);
	}
	mutex_unlock(&adev->list_mutex);
	return usable_buffers;
}

static int _reserve(struct acq400_path_descriptor *pd, int ibuf,
		    struct list_head *list, struct list_head *rlist)
{
	struct acq400_dev *adev = pd->dev;
	struct HBM *cur;
	struct HBM *tmp;

	list_for_each_entry_safe(cur, tmp, list, list) {
		dev_dbg(DEVP(adev), "%s want %d consider ix %d\n", __FUNCTION__,
			ibuf, cur->ix);
		if (cur->ix == ibuf) {
			cur->bstate = BS_RESERVED;
			list_move_tail(&cur->list, rlist);
			dev_dbg(DEVP(adev), "reserve %d", cur->ix);
			return 0;
		}
	}
	return -1;
}
int reserve(struct acq400_path_descriptor *pd, int ibuf)
{
	struct acq400_dev *adev = pd->dev;
	int rc = -1;

	dev_dbg(DEVP(adev), "reserve 01 %d", ibuf);
	mutex_lock(&adev->list_mutex);
	rc = _reserve(pd, ibuf, &adev->STASH, &pd->RESERVED);
	if (rc != 0) {
		rc = _reserve(pd, ibuf, &adev->EMPTIES, &pd->RESERVED);
	}
	mutex_unlock(&adev->list_mutex);
	dev_dbg(DEVP(adev), "reserve 99");
	return rc;
}

int remove(struct acq400_path_descriptor *pd, int ibuf, struct list_head *rlist)
{
	struct acq400_dev *adev = pd->dev;
	int rc = -1;

	dev_dbg(DEVP(adev), "remove 01 %d", ibuf);
	mutex_lock(&adev->list_mutex);
	rc = _reserve(pd, ibuf, &adev->STASH, rlist);
	if (rc != 0) {
		rc = _reserve(pd, ibuf, &adev->EMPTIES, rlist);
	}
	mutex_unlock(&adev->list_mutex);
	dev_dbg(DEVP(adev), "remove 99");
	return rc;
}
int replace(struct acq400_path_descriptor *pd, int ibuf)
{
	struct acq400_dev *adev = pd->dev;

	mutex_lock(&adev->list_mutex);
	/* reserved off the head .. replace at the head */
	list_move(&pd->RESERVED, &adev->EMPTIES);
	mutex_unlock(&adev->list_mutex);
	return 0;
}

int hbm_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct HBM *ha = list_entry(a, struct HBM, list);
	struct HBM *hb = list_entry(b, struct HBM, list);

	if (ha->ix < hb->ix) {
		return -1;
	} else if (ha->ix > hb->ix) {
		return 1;
	} else {
		return 0;
	}
}

int is_sorted(struct acq400_dev *adev, struct list_head *hblist)
{
	struct HBM *cursor;
	int entries = 0;
	int badsort = 0;

	int ix1 = -1;
	list_for_each_entry(cursor, hblist, list) {
		++entries;
		if (++ix1 != cursor->ix) {
			++badsort;
		}
	}
	if (badsort)
		dev_dbg(DEVP(adev), "is_sorted() total:%d bad:%d", entries,
			badsort);
	return badsort == 0;
}
void count_empties(struct acq400_dev *adev)
{
	struct HBM *cursor;
	int ecount = 0;

	mutex_lock(&adev->list_mutex);
	list_for_each_entry(cursor, &adev->EMPTIES, list) {
		++ecount;
	}
	mutex_unlock(&adev->list_mutex);
	dev_dbg(DEVP(adev), "count_empties:%d", ecount);
}

void sort_empties(struct acq400_dev *adev)
{
	if (is_sorted(adev, &adev->EMPTIES))
		return;

	dev_dbg(DEVP(adev), "NB: empties list not in order, sorting it .. ");
	list_sort(NULL, &adev->EMPTIES, hbm_cmp);
	if (is_sorted(adev, &adev->EMPTIES)) {
		dev_dbg(DEVP(adev), ".. sorted");
	} else {
		dev_dbg(DEVP(adev), "failed to sort EMPTIES");
	}
	count_empties(adev);
}

void replace_all(struct acq400_path_descriptor *pd)
{
	struct acq400_dev *adev = pd->dev;
	struct HBM *cur;
	struct HBM *tmp;

	dev_dbg(DEVP(adev), "replace_all 01");

	count_empties(adev);
	mutex_lock(&adev->list_mutex);
	list_for_each_entry_safe(cur, tmp, &pd->RESERVED, list) {
		cur->bstate = BS_EMPTY;
		list_move(&cur->list, &adev->EMPTIES);
		dev_dbg(DEVP(adev), "replace_all return %d", cur->ix);
	}
	mutex_unlock(&adev->list_mutex);
	sort_empties(adev);

	if (!is_sorted(adev, &adev->EMPTIES)) {
		sort_empties(adev);
	}
	if (!is_sorted(adev, &adev->EMPTIES)) {
		dev_dbg(DEVP(adev), "failed to sort EMPTIES");
	}
	dev_dbg(DEVP(adev), "replace 99");
}

void empty_lists(struct acq400_dev *adev)
{
	move_list_to_empty(adev, &adev->OPENS);
	move_list_to_empty(adev, &adev->REFILLS);
	move_list_to_empty(adev, &adev->INFLIGHT);

	count_empties(adev);
	sort_empties(adev);
}
