/* ------------------------------------------------------------------------- */
/* acq400_drv.c  D-TACQ ACQ400 FMC  DRIVER   
 *
 * bwg_debugfs.c
 *
 *  Created on: 13 Jan 2015
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
#include "bwg.h"


/* @@REMOVEME */
#include "acq400_debugfs_internal.h"

struct dentry* bwg_debug_root;




void bwg_createDebugfs(struct bwg_dev* mdev)
{
#define adev	mdev
	char* pcursor;
	if (!bwg_debug_root){
		bwg_debug_root = debugfs_create_dir("bwg", 0);
		if (!bwg_debug_root){
			dev_warn(DEVP(adev), "failed create dir bwg");
			return;
		}
	}
#if 0
	dev_rc_init(DEVP(adev), &adev->reg_cache,
			adev->dev_virtaddr, adev->of_prams.site, MOD_REG_MAX);
#endif
	pcursor = adev->debug_names = kmalloc(4096, GFP_KERNEL);
	adev->debug_dir = debugfs_create_dir(
			adev->devname, bwg_debug_root);

	if (!mdev->debug_dir){
		dev_warn(&adev->pdev->dev, "failed create dir %s", adev->devname);
		return;
	}
	DBG_REG_CREATE(MOD_ID);

#if 0
	dev_rc_finalize(DEVP(adev), &adev->reg_cache, adev->of_prams.site);
#endif
#undef adev
}
void bwg_removeDebugfs(struct bwg_dev* adev)
{
	debugfs_remove_recursive(adev->debug_dir);
	kfree(adev->debug_names);
}
