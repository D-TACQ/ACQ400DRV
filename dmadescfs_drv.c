/* ------------------------------------------------------------------------- */
/* dmadescfs.c ACQ420_FMC						     */
/*
 * dmadescfs.c
 *  N device nodes : 0..N-1
 *  open(),
 *  mmap() : first mmap() on device allocates a buffer.
 *  	buffer is allocated from COHERENT memory aka uncached, this means
 *  	it should work well with eg, dma descriptors.
 *  ioctl() :
 *   	DD_GETPA : returns buffer PA
 *   	... not needed with coherent buffer:
 *   	DD_TOCPU : invalidate cache for cpu read after device write
 *   	DD_TODEV : flush cache for device use after cpu write
 *
 *
 *  Created on: 18 Feb 2016
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

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/platform_device.h>

#include <linux/dma-mapping.h>

#include "dmadescfs_ioctl.h"

#define REVID "4"
#define MODULE_NAME "dmadescfs"

#define MAXBLOCKS 16 /* # minor nodes one buffer per minor */

/** buffer definition. could be many buffers per device */
struct desc_buf {
	int id; /* matches minor in device node */
	void *va;
	unsigned long length;
	dma_addr_t pa;
	struct list_head db_list;
};

/** device definition. could be many devices, in practise, only one */
struct dmadescfs_dev {
	struct cdev cdev;
	struct platform_device *pdev;
	struct list_head buffers;
};

/** path descriptor : descriptor per open file handle */
struct dmadescfs_pathdescriptor {
	struct dmadescfs_dev *dev;
	int id;
};

#define DEVP(ddev) (&ddev->pdev->dev)
#define PD(file) ((struct dmadescfs_pathdescriptor *)file->private_data)
#define SETPD(file, value) (file->private_data = (value))
#define PDSZ sizeof(struct dmadescfs_pathdescriptor)
#define DMADESCFS_DEV(file) (PD(file)->dev)

struct dmadescfs_dev *devices[1]; // only ONE device

static int getOrder(int len)
{
	int order;
	len /= PAGE_SIZE;

	for (order = 0; 1 << order < len; ++order) {
		;
	}
	return order;
}

struct desc_buf *dmadescfs_find_buf(struct dmadescfs_dev *ddev, int id)
{
	struct desc_buf *cur;
	dev_dbg(DEVP(ddev), "dmadescfs_find_buf() 01");

	list_for_each_entry(cur, &ddev->buffers, db_list) {
		dev_dbg(DEVP(ddev), "dmadescfs_find_buf() 01 cur id %d",
			cur->id);
		if (cur->id == id) {
			dev_dbg(DEVP(ddev), "dmadescfs_find_buf() return %p",
				cur);
			return cur;
		}
	}
	dev_dbg(DEVP(ddev), "dmadescfs_find_buf() return 0");
	return 0;
}

/** dmadescfs_get_buf : if the buffer exists and is the right size,
 *  return it. If it exists, wrong size, return NULL
 *  If it doesn't exist, create it
 */
struct desc_buf *dmadescfs_get_buf(struct dmadescfs_dev *ddev, int id,
				   unsigned long vsize)
{
	struct desc_buf *cur = dmadescfs_find_buf(ddev, id);

	if (cur != 0) {
		if (cur->length >= vsize) {
			dev_dbg(DEVP(ddev), "dmadescfs_get_buf() recycle %p",
				cur);
			return cur;
		} else {
			return 0;
		}
	}

	cur = kzalloc(sizeof(struct desc_buf), GFP_KERNEL);
	cur->va = (void *)__get_free_pages(GFP_KERNEL, getOrder(vsize));

	if (cur->va == 0) {
		dev_err(DEVP(ddev), "dma_alloc_coherent %lu FAIL", vsize);
		kfree(cur);
		return 0;
	}
	cur->pa = dma_map_single(DEVP(ddev), cur->va, vsize, DMA_BIDIRECTIONAL);
	cur->length = vsize;
	cur->id = id;
	INIT_LIST_HEAD(&cur->db_list);
	list_add_tail(&cur->db_list, &ddev->buffers);
	dev_dbg(DEVP(ddev), "dmadescfs_get_buf() return new %p", cur);
	return cur;
}

int dmadescfs_open(struct inode *inode, struct file *file)
{
	SETPD(file, kzalloc(PDSZ, GFP_KERNEL));
	PD(file)->dev = container_of(inode->i_cdev, struct dmadescfs_dev, cdev);
	PD(file)->id = MINOR(inode->i_rdev);

	dev_dbg(DEVP(PD(file)->dev), "dmadescfs_open mode dev:%p id:%d %x",
		PD(file)->dev, PD(file)->id, file->f_mode);

	return 0;
}

int dmadescfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct dmadescfs_dev *ddev = DMADESCFS_DEV(file);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	struct desc_buf *db = dmadescfs_get_buf(ddev, PD(file)->id, vsize);

	dev_dbg(DEVP(ddev), "dmadescfs_mmap db:%p", db);
	if (db == 0) {
		dev_err(&ddev->pdev->dev, "Buffer not available");
		return -ENOMEM;
	}
	if (remap_pfn_range(vma, vma->vm_start, db->pa >> PAGE_SHIFT, vsize,
			    vma->vm_page_prot)) {
		return -EAGAIN;
	} else {
		return 0;
	}
	return 0;
}

static long dmadescfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				     unsigned long arg)
{
	struct dmadescfs_dev *ddev = DMADESCFS_DEV(file);
	struct desc_buf *db = dmadescfs_find_buf(ddev, PD(file)->id);

	if (db == 0) {
		return -ENOMEM;
	}
	switch (cmd) {
	case DD_GETPA:
		dev_dbg(DEVP(ddev), "ioctl DD_GETPA 0x%08x", db->pa);
		*(unsigned long *)arg = db->pa;
		return 0;
	case DD_TOCPU:
		dev_dbg(DEVP(ddev), "ioctl DD_TOCPU pa:0x%08x, len:0x%08lx",
			db->pa, db->length);
		dma_sync_single_for_cpu(DEVP(ddev), db->pa, db->length,
					DMA_FROM_DEVICE);
		return 0;
	case DD_TODEV:
		dev_dbg(DEVP(ddev), "ioctl DD_TODEV pa:0x%08x, len:0x%08lx",
			db->pa, db->length);
		dma_sync_single_for_device(DEVP(ddev), db->pa, db->length,
					   DMA_TO_DEVICE);
		return 0;
	default:
		return -ENODEV;
	}
}

int dmadescfs_release(struct inode *inode, struct file *file)
{
	kfree(PD(file));
	return 0;
}
struct file_operations dmadescfs_fops = { .owner = THIS_MODULE,
					  .open = dmadescfs_open,
					  .release = dmadescfs_release,
					  .mmap = dmadescfs_mmap,
					  .unlocked_ioctl =
						  dmadescfs_unlocked_ioctl };

struct dmadescfs_dev *dmadescfs_alloc_dev(struct platform_device *pdev)
{
	struct dmadescfs_dev *ddev =
		kzalloc(sizeof(struct dmadescfs_dev), GFP_KERNEL);
	INIT_LIST_HEAD(&ddev->buffers);
	ddev->pdev = pdev;
	return ddev;
}

static int dmadescfs_probe(struct platform_device *pdev)
{
	struct dmadescfs_dev *ddev = dmadescfs_alloc_dev(pdev);
	dev_t devno;
	int rc;

	if (pdev->id != 0) {
		goto fail;
	}

	rc = alloc_chrdev_region(&devno, 0, MAXBLOCKS - 1, "dmadescfs");
	if (rc < 0) {
		dev_err(&pdev->dev, "unable to register chrdev\n");
		goto fail;
	}
	cdev_init(&ddev->cdev, &dmadescfs_fops);
	ddev->cdev.owner = THIS_MODULE;
	rc = cdev_add(&ddev->cdev, devno, MAXBLOCKS - 1);
	if (rc < 0) {
		goto fail;
	}

	devices[pdev->id] = ddev;
	dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	return 0;

fail:
	kfree(ddev);
	return -1;
}

static int dmadescfs_remove(struct platform_device *pdev)
{
	struct dmadescfs_dev *ddev = devices[pdev->id];
	struct desc_buf *cur;

	BUG_ON(pdev->id != 0);

	list_for_each_entry(cur, &ddev->buffers, db_list) {
		dma_unmap_single(DEVP(ddev), cur->pa, cur->length,
				 DMA_BIDIRECTIONAL);
		free_pages((unsigned long)cur->va, getOrder(cur->length));
	}
	kfree(ddev);
	platform_device_unregister(pdev);
	return 0;
}

static struct platform_driver dmadescfs_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
        },
        .probe = dmadescfs_probe,
        .remove = dmadescfs_remove,
};

static void __exit dmadescfs_exit(void)
{
	platform_driver_unregister(&dmadescfs_driver);
}

static int __init dmadescfs_init(void)
{
	int status = 0;
	struct platform_device *pdev;

	printk("D-TACQ dmadescfs Driver %s\n", REVID);

	platform_driver_register(&dmadescfs_driver);

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	pdev->name = MODULE_NAME;
	pdev->id = 0;
	platform_device_register(pdev);

	return status;
}

module_init(dmadescfs_init);
module_exit(dmadescfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ dmadescfs Driver");
MODULE_AUTHOR("D-TACQ Solutions");
MODULE_VERSION(REVID);
