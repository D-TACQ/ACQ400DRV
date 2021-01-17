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

#define REVID "0.005"

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME 	"bwg"

char* MODEL = "";
module_param(MODEL, charp, 0444);

int dummy_bram = 0;
module_param(dummy_bram, int, 0644);


/* index from 0. There's only one physical MGT400, but may have 2 channels */
struct bwg_dev* bwg_devices[1];
int ndev;

#undef DEVP
#define DEVP(mdev)		(&(mdev)->pdev->dev)


void bwg_wr32(struct bwg_dev *bdev, int offset, u32 value)
{
	if (bdev->RW32_debug){
		dev_info(DEVP(bdev), "bwgwr32 %p [0x%02x] = %08x\n",
				bdev->va + offset, offset, value);
	}else{
		dev_dbg(DEVP(bdev), "bwgwr32 %p [0x%02x] = %08x\n",
				bdev->va + offset, offset, value);
	}

	iowrite32(value, bdev->va + offset);
}

u32 bwg_rd32(struct bwg_dev *bdev, int offset)
{
	u32 rc = ioread32(bdev->va + offset);
	if (bdev->RW32_debug){
		dev_info(DEVP(bdev), "bwgrd32 %p [0x%02x] = %08x\n",
			bdev->va + offset, offset, rc);
	}else{
		dev_dbg(DEVP(bdev), "bwgrd32 %p [0x%02x] = %08x\n",
			bdev->va + offset, offset, rc);
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

int bwg_init_descriptor(struct bwg_path_descriptor** pd)
{
	struct bwg_path_descriptor* pdesc = kzalloc(PDSZ, GFP_KERNEL);
	*pd = pdesc;
	return 0;
}

int bwg_open(struct inode *inode, struct file *file)
{
	bwg_init_descriptor((struct bwg_path_descriptor**)&file->private_data);
	PD(file)->dev = container_of(inode->i_cdev, struct bwg_dev, cdev);
	PD(file)->minor = MINOR(inode->i_rdev);


	if (file->f_flags & O_WRONLY) {
		struct bwg_dev* bwg_dev = BWG_DEV(file);
		int chix = CHIX(PD(file)->minor);
		int iw;
		for (iw = 0; iw < CH_MAX_SAM; ++iw){
			bwg_wr32(bwg_dev, CH_OFF(chix, iw), 0);
		}
		bwg_dev->bwg_chan[chix].cursor = 0;
	}
	return 0;
}
ssize_t bwg_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct bwg_dev* bwg_dev = BWG_DEV(file);
	int chix = CHIX(PD(file)->minor);
	int len = bwg_dev->bwg_chan[chix].cursor*sizeof(u32);
	unsigned bcursor = *f_pos;	/* f_pos counts in bytes */
	int rc;
	int iw;

	if (count%sizeof(u32)){
		return -1;
	}
	if (bcursor >= len){
		return 0;
	}else{
		int headroom = (len - bcursor);
		if (count > headroom){
			count = headroom;
		}
	}
	for (iw = 0; iw < count/sizeof(u32); ++iw){
		PD(file)->ch_buf[iw] = bwg_rd32(bwg_dev, bcursor+CH_OFF(chix, iw));
	}
	rc = copy_to_user(buf, PD(file)->ch_buf, count);
	if (rc){
		return -1;
	}

	*f_pos += count;
	return count;
}

ssize_t bwg_write(struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned bcursor = *f_pos;			/* f_pos counts in bytes */
	int rc;

	if (count%sizeof(u32)){
		return -1;
	}else if (bcursor >= CH_LEN){
		return 0;
	}else{
		int headroom = (CH_LEN - bcursor);
		if (count > headroom){
			count = headroom;
		}
	}
	rc = copy_from_user(PD(file)->ch_buf+PD(file)->cursor, buf, count);
	if (rc){
		return -1;
	}
	*f_pos += count;
	PD(file)->cursor += count/sizeof(u32);
	return count;

}

int bwg_release(struct inode *inode, struct file *file)
{
	int rc = 0;

	if (file->f_flags & O_WRONLY) {
		struct bwg_dev* bwg_dev = BWG_DEV(file);
		unsigned* src = PD(file)->ch_buf;
		int chix = CHIX(PD(file)->minor);
		int iw;
		dev_dbg(DEVP(bwg_dev), "%s write\n", __FUNCTION__);
		for (iw = 0; iw < PD(file)->cursor; ++iw){
			bwg_wr32(bwg_dev, CH_OFF(chix, iw), src[iw]);
		}
		bwg_dev->bwg_chan[chix].cursor = PD(file)->cursor;
	}
	kfree(PD(file));
	return rc;
}



struct file_operations bwg_fops = {
        .owner = THIS_MODULE,
        .open = bwg_open,
        .read = bwg_read,
	.write = bwg_write,
        .release = bwg_release
};

static int bwg_probe(struct platform_device *pdev)
{
        int rc = 0;
        dev_t devno;
        struct bwg_dev* mdev = bwg_allocate_dev(pdev);
        dev_info(&pdev->dev, "%s", __FUNCTION__);

        if (!mdev){
        	dev_err(&pdev->dev, "unable to allocate device structure\n");
        	rc = -ENODEV;
        	goto remove;
        }
        if (ndev){
        	dev_err(DEVP(mdev), "ONE device only allowed");
        	goto remove;
        }
        mdev->pdev->dev.id = 0;
        ndev += 1;

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
        snprintf(mdev->devname, 16, "bwg.%d", mdev->pdev->dev.id);
        if (dummy_bram){
        	unsigned blen = mdev->mem->end-mdev->mem->start+1;
        	mdev->va = kzalloc(blen, GFP_KERNEL);
        	dev_info(&pdev->dev, "%s dummy_bram %p len %x", __FUNCTION__, mdev->va, blen);
        }else{
        	mdev->va = ioremap(mdev->mem->start, mdev->mem->end-mdev->mem->start+1);
        }

        rc = alloc_chrdev_region(&devno, 0, BWG_MINOR_COUNT, mdev->devname);
        if (rc < 0) {
        	dev_err(DEVP(mdev), "unable to register chrdev\n");
                goto fail;
        }

        mdev->mod_id = bwg_rd32(mdev, MOD_ID);

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
