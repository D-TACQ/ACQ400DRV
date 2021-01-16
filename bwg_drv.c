/* ------------------------------------------------------------------------- */
/* bwg_drv.c  D-TACQ bwg_comms driver
 *
 *  Created on: 12 Jan 2015
 *      Author: pgm
 *
 * ------------------------------------------------------------------------- */
/*   Copyright (C) 2021 Peter Milne, D-TACQ Solutions Ltd                    *
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

#define REVID "0.001"

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME 	"bwg"

char* MODEL = "";
module_param(MODEL, charp, 0444);

/* index from 0. There's only one physical MGT400, but may have 2 channels */
struct bwg_dev* bwg_dev_devices[1];

#undef DEVP
#define DEVP(mdev)		(&(mdev)->pdev->dev)


void bwgwr32(struct bwg_dev *mdev, int offset, u32 value)
{
	if (mdev->RW32_debug){
		dev_info(DEVP(mdev), "bwgwr32 %p [0x%02x] = %08x\n",
				mdev->va + offset, offset, value);
	}else{
		dev_dbg(DEVP(mdev), "bwgwr32 %p [0x%02x] = %08x\n",
				mdev->va + offset, offset, value);
	}

	iowrite32(value, mdev->va + offset);
}

u32 bwgrd32(struct bwg_dev *mdev, int offset)
{
	u32 rc = ioread32(mdev->va + offset);
	if (mdev->RW32_debug){
		dev_info(DEVP(mdev), "bwgrd32 %p [0x%02x] = %08x\n",
			mdev->va + offset, offset, rc);
	}else{
		dev_dbg(DEVP(mdev), "bwgrd32 %p [0x%02x] = %08x\n",
			mdev->va + offset, offset, rc);
	}
	return rc;
}


static struct bwg_dev* bwg_allocate_dev(struct platform_device *pdev)
/* Allocate and init a private structure to manage this device */
{
	struct bwg_dev* mdev = kzalloc(sizeof(struct bwg_dev), GFP_KERNEL);
        if (mdev == NULL) {
                return NULL;
        }
        mdev->pdev = pdev;
        return mdev;
}



struct file_operations bwg_fops = {
        .owner = THIS_MODULE,
/* @@todo
        .open = bwg_open,
        .read = bwg_read,
		.write = bwg_write,
        .release = bwg_release,
*/
};

static int bwg_probe(struct platform_device *pdev)
{
        int rc = 0;
        dev_t devno;
        struct bwg_dev* mdev = bwg_allocate_dev(pdev);
        dev_info(&pdev->dev, "bwg_probe()");

        if (!mdev){
        	dev_err(&pdev->dev, "unable to allocate device structure\n");
        	rc = -ENODEV;
        	goto remove;
        }

        mdev->pdev->dev.id = 0;

        mdev->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (mdev->mem == NULL){
        	dev_err(DEVP(mdev), "No resources found");
        	rc = -ENODEV;
        	goto remove;
        }
        if (!request_mem_region(mdev->mem->start,
                mdev->mem->end-mdev->mem->start+1, mdev->devname)) {
                dev_err(DEVP(mdev), "can't reserve i/o memory at 0x%08X\n",
                        mdev->mem->start);
                rc = -ENODEV;
                goto fail;
        }
        mdev->va = ioremap(mdev->mem->start, mdev->mem->end-mdev->mem->start+1);

        rc = alloc_chrdev_region(&devno, 0, BWG_MINOR_COUNT, mdev->devname);
        if (rc < 0) {
        	dev_err(DEVP(mdev), "unable to register chrdev\n");
                goto fail;
        }

        mdev->mod_id = bwgrd32(mdev, MOD_ID);

        cdev_init(&mdev->cdev, &bwg_fops);
        mdev->cdev.owner = THIS_MODULE;
        rc = cdev_add(&mdev->cdev, devno, BWG_MINOR_COUNT);
        if (rc < 0){
        	goto fail;
        }

        bwg_createSysfs(&mdev->pdev->dev);
        bwg_createDebugfs(mdev);
        return rc;

fail:
remove:
	kfree(mdev);
	return rc;
}

static int _bwg_remove(struct bwg_dev* mdev){
	kfree(mdev);
	return 0;
}

static int bwg_remove(struct platform_device *pdev)
/* undo all the probe things in reverse */
{
	if (pdev->id == -1){
		return -1;
	}else{
		return _bwg_remove(bwg_devices[pdev->id]);
	}
}

#ifdef CONFIG_OF
static struct of_device_id bwg_of_match[] /* __devinitdata */ = {
        { .compatible = "D-TACQ,cpsc2bwg"  },
        { /* end of table */}
};
MODULE_DEVICE_TABLE(of, bwg_of_match);
#else
#define bwg_of_match NULL
#endif /* CONFIG_OF */



static struct platform_driver bwg_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = bwg_of_match,
        },
        .probe = bwg_probe,
        .remove = bwg_remove,
};

static struct proc_dir_entry *bwg_proc_root;

void bwg_module_init_proc(void)
{
	bwg_proc_root = proc_mkdir("driver/acq400/bwg", 0);
}
void bwg_module_remove_proc(void)
{
	remove_proc_entry("driver/acq400/bwg", NULL);
}

static void __exit bwg_exit(void)
{
	platform_driver_unregister(&bwg_driver);
	bwg_module_remove_proc();
}

static int __init bwg_init(void)
{
        int status;

	printk("D-TACQ BWG Module Driver %s\n", REVID);
	bwg_module_init_proc();
        status = platform_driver_register(&bwg_driver);

        return status;
}

module_init(bwg_init);
module_exit(bwg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
