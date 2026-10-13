/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2011 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    http://www.d-tacq.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file regfs_fs.c DESCR
 * 
 *  Created on: August 18, 2015
 *      Author: pgm
 *

# cpsc register definition file
# formatting: NO TABS, ONLY SPACES
# "TABSTOPS" at 0, 32, 48, 64, 72
# By all means use a tabbing editor, but tabs will be replaced by spaces:
# ./fixup-tabs cpsc_regdef

cd PEX
PEX_CSR                         0x000           0xffffffff      r       %08x
PEX_MWE                         0x004           0x00000001      r
PEX_DEBUG                       0x008           0xffffffff      rw      %08x
PEX_INT                         0x00c           0xffffffff      r       %08x

 */

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/types.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mm.h>
//#include <linux/mm_types.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <linux/slab.h>

#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/dma-direction.h>
#include "hbm.h"
#include "acq400.h"

#include "debugfs2.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#include <linux/slab.h>
#include <linux/kdev_t.h>

#include <linux/moduleparam.h>

#include <linux/interrupt.h>
#include <linux/wait.h>

#include "regfs.h"

#define DEVP(rd) (&(rd)->pdev->dev)

#define REVID "regfs_fs B1016"

static int mem_pages(struct REGFS_DEV *rdev)
{
	if (rdev->mem == 0) {
		dev_err(DEVP(rdev), "error no memory region");
		return 0;
	} else {
		unsigned roundup =
			(rdev->mem->end & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
		return (roundup - rdev->mem->start) / PAGE_SIZE;
	}
}

static void cd(struct REGFS_DEV *rdev, char *dir)
{
	if (strcmp(dir, "/") == 0) {
		rdev->istack = 0;
	} else if (strcmp(dir, "..") == 0) {
		if (rdev->istack > 0) {
			rdev->istack -= 1;
		}
	} else {
		if (rdev->istack + 1 >= MAXSTACK) {
			dev_err(DEVP(rdev), "stack depth %d reached", MAXSTACK);
		} else {
			struct dentry *cwd = debugfs_create_dir(
				dir, rdev->dstack[rdev->istack]);
			if (cwd == 0) {
				dev_err(DEVP(rdev), "failed to create subdir");
			} else {
				rdev->dstack[++rdev->istack] = cwd;
				dev_dbg(DEVP(rdev), "cd %s OK", dir);
			}
		}
	}
}

static struct dentry *cwd(struct REGFS_DEV *dev)
{
	return dev->dstack[dev->istack];
}

#define CD "cd "

static struct DebugFs2NodeInfo *nodeCreate(const struct DebugFs2NodeInfo *def)
{
	struct DebugFs2NodeInfo *nodeInfo = kmalloc(DBGFS2_NI_SZ, GFP_KERNEL);
	memcpy(nodeInfo, def, DBGFS2_NI_SZ);
	return nodeInfo;
}
static ssize_t regfs_direct_write(struct file *file,
				  const char __user *user_buf, size_t count,
				  loff_t *ppos)
{
	struct DebugFs2NodeInfo defaultNodeInfo = {
		0,
	};
	char myline[80];
	ssize_t rc;

	struct REGFS_DEV *dev = (struct REGFS_DEV *)file->private_data;

	defaultNodeInfo.pread = defaultNodeInfo.pwrite =
		defaultNodeInfo.pcache = dev->va;

	rc = debugfs2_write_line(file, user_buf, min(count, sizeof(myline) - 1),
				 ppos, myline);
	myline[79] = '\0';

	if (rc > 0 && myline[0] != '#' && strlen(myline) > 3) {
		if (strncmp(myline, CD, strlen(CD)) == 0) {
			cd(dev, myline + strlen(CD));
		} else {
			struct DebugFs2NodeInfo *nodeInfo =
				nodeCreate(&defaultNodeInfo);
			debugfs2_create_file_def(cwd(dev), nodeInfo, myline,
						 (int)*ppos);
		}
		/* @@todo: newfile -> list */
	}

	return rc;
}

static int regfs_direct_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

const struct file_operations regfs_direct_fops = { .write = regfs_direct_write,
						   .open = regfs_direct_open };

int regfs_device_tree_init(struct REGFS_DEV *dev)
{
	/* get site? */
	return 0;
}
int regfs_init_fs(struct REGFS_DEV *dev)
{
	struct resource *mem;
	int rc = 0;
	dev_info(DEVP(dev), REVID);

	dev_dbg(DEVP(dev), "nres = %d [0].type: %lx", dev->pdev->num_resources,
		dev->pdev->resource[0].flags);

	if (dev->pdev->dev.of_node != 0) {
		if (regfs_device_tree_init(dev) != 0) {
			dev_err(DEVP(dev), "regfs_device_tree_init() failed");
			rc = -ENODEV;
			goto init99;
		}
	}

	mem = platform_get_resource(dev->pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(DEVP(dev), "No resources found");
		rc = -ENODEV;
		goto init99;
	}

	dev_dbg(DEVP(dev), "request_mem_region()");
	if (!request_mem_region(mem->start, mem->end - mem->start + 1,
				mem->name)) {
		dev_err(DEVP(dev), "can't reserve i/o memory at 0x%08X\n",
			mem->start);
		rc = -ENODEV;
		goto init99;
	}
	dev_dbg(DEVP(dev), "ioremap() %08x %08x", mem->start,
		mem->end - mem->start + 1);
	dev->mem = mem;
	dev->va = ioremap(mem->start, mem->end - mem->start + 1);

	dev_dbg(DEVP(dev), "create_dir %s", mem->name);

	dev->dstack[dev->istack = 0] = dev->top =
		debugfs_create_dir(mem->name, NULL);
	debugfs_create_u32("site", 0444, dev->top, &dev->pdev->id);
	dev->create_hook = debugfs_create_file(".create", S_IWUGO, dev->top,
					       dev, &regfs_direct_fops);

	dev_dbg(DEVP(dev), "all good");
init99:
	return rc;
}

void regfs_remove_fs(struct REGFS_DEV *dev)
{
	dev_info(DEVP(dev), REVID);
	debugfs_remove_recursive(dev->top);
}

static struct file_operations regfs_page_fops;
struct file_operations regfs_event_fops;

int regfs_open(struct inode *inode, struct file *file)
{
	SETPD(file, kzalloc(PDSZ, GFP_KERNEL));
	PD(file)->rdev = container_of(inode->i_cdev, struct REGFS_DEV, cdev);
	PD(file)->minor = MINOR(inode->i_rdev);

	if (PD(file)->minor == MINOR_EV) {
		PD(file)->int_count = PD(file)->rdev->ints;
		file->f_op = &regfs_event_fops;
		if (file->f_op->open) {
			return file->f_op->open(inode, file);
		}
	} else {
		/* minor is page # */
		file->f_op = &regfs_page_fops;
		if (file->f_op->open) {
			return file->f_op->open(inode, file);
		}
	}

	return 0;
}
int regfs_release(struct inode *inode, struct file *file)
{
	void *buf = PD(file);
	SETPD(file, 0);
	if (buf) {
		kfree(buf);
	}
	return 0;
}

ssize_t regfs_page_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *f_pos)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;
	char *va = rdev->va + PD(file)->minor * PAGE_SIZE;
	int len = PAGE_SIZE;
	unsigned bcursor = *f_pos; /* f_pos counts in bytes */
	unsigned uwrite;
	int cursor;
	int rc;

	if (bcursor >= len) {
		return 0;
	} else {
		int headroom = (len - bcursor);
		if (count > headroom) {
			count = headroom;
		}
		count = count & ~3;
	}
	for (cursor = 0; cursor < count; cursor += sizeof(unsigned)) {
		rc = copy_from_user(&uwrite, buf + cursor, sizeof(unsigned));
		dev_dbg(DEVP(rdev), "0x%08x => iowrite(%p)", uwrite,
			va + cursor);
		iowrite32(uwrite, va + cursor);
		if (rc) {
			return -1;
		}
	}

	*f_pos += count;
	return count;
}

ssize_t regfs_page_read(struct file *file, char __user *buf, size_t count,
			loff_t *f_pos)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;
	char *va = rdev->va + PD(file)->minor * PAGE_SIZE;
	int len = PAGE_SIZE;
	unsigned bcursor = *f_pos; /* f_pos counts in bytes */
	unsigned uread;
	int cursor;
	int rc;

	if (bcursor >= len) {
		return 0;
	} else {
		int headroom = (len - bcursor);
		if (count > headroom) {
			count = headroom;
		}
		count = count & ~3;
	}
	for (cursor = 0; cursor < count; cursor += sizeof(unsigned)) {
		uread = ioread32(va + cursor);
		dev_dbg(DEVP(rdev), "0x%08x <= ioread(%p)", uread, va + cursor);
		rc = copy_to_user(buf + cursor, &uread, sizeof(unsigned));

		if (rc) {
			return -1;
		}
	}

	*f_pos += count;
	return count;
}

/* ONLY ONE consumer allowed */
int regfs_event_open(struct inode *inode, struct file *file)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;

	if (rdev->event_client_pid != 0) {
		regfs_release(inode, file);
		return -EBUSY;
	} else {
		rdev->event_client_pid = current->pid;
		rdev->client_ready = 1;
		return 0;
	}
}

int regfs_event_release(struct inode *inode, struct file *file)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;
	void *buf = PD(file);
	rdev->event_client_pid = 0;
	rdev->client_ready = 0;
	SETPD(file, 0);
	if (buf) {
		kfree(buf);
	}
	return 0;
}

unsigned int regfs_event_poll(struct file *file,
			      struct poll_table_struct *poll_table)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;
	int int_count = PD(file)->int_count;
	unsigned rc;

	if (rdev->ints == int_count) {
		poll_wait(file, &rdev->w_waitq, poll_table);
	}
	rc = rdev->ints != int_count ? POLLIN : 0;
	dev_dbg(DEVP(rdev), "regfs_event_poll() return %u", rc);
	return rc;
}
int regfs_page_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct REGFS_DEV *rdev = PD(file)->rdev;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = 0x1000;
	unsigned long pa = rdev->mem->start + PD(file)->minor * PAGE_SIZE;
	unsigned pfn = pa >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	dev_dbg(DEVP(rdev), "regfs_page_mmap pa:0x%08lx vsize %lu psize %lu %s",
		pa, vsize, psize, vsize > psize ? "EINVAL" : "OK");

	if (vsize > psize) {
		return -EINVAL; /* request too big */
	}
	if (io_remap_pfn_range(vma, vma->vm_start, pfn, vsize,
			       vma->vm_page_prot)) {
		return -EAGAIN;
	} else {
		return 0;
	}
}

struct file_operations regfs_event_fops = { .owner = THIS_MODULE,
					    .open = regfs_event_open,
					    .release = regfs_event_release,
					    /* 	.read = regfs_event_read, */
					    .poll = regfs_event_poll };

static struct file_operations regfs_page_fops = {
	.owner = THIS_MODULE,
	.release = regfs_release,
	.mmap = regfs_page_mmap,
	.write = regfs_page_write,
	.read = regfs_page_read,
	.llseek = generic_file_llseek,
};

static struct file_operations regfs_fops = {
	.owner = THIS_MODULE,
	.open = regfs_open,
	.release = regfs_release,
};

int regfs_probe_rdev(struct platform_device *pdev, struct REGFS_DEV *rdev)
{
	dev_t devno;
	int npages;
	int rc;
	int minor_max = MINOR_EV;

	rdev->pdev = pdev;

	rc = regfs_init_fs(rdev);
	if (rc != 0) {
		dev_err(&pdev->dev, "regfs_init_fs() failed");
		goto fail;
	}

	npages = mem_pages(rdev);
	if (npages == 0) {
		return 0;
	}
	if (npages > minor_max) {
		dev_err(DEVP(rdev), "MINOR_EV NOT AVAILABLE\n");
		minor_max = npages;
	}
	rc = alloc_chrdev_region(&devno, 0, minor_max + 1, rdev->mem->name);
	if (rc < 0) {
		dev_err(DEVP(rdev), "unable to register chrdev\n");
		goto fail;
	}

	cdev_init(&rdev->cdev, &regfs_fops);
	rdev->cdev.owner = THIS_MODULE;
	rc = cdev_add(&rdev->cdev, devno, minor_max + 1);

	dev_set_drvdata(&pdev->dev, rdev);
fail:
	return rc;
}

int regfs_probe(struct platform_device *pdev)
{
	return regfs_probe_rdev(pdev,
				kzalloc(sizeof(struct REGFS_DEV), GFP_KERNEL));
}

int regfs_remove(struct platform_device *pdev)
{
	struct REGFS_DEV *rdev = 0; // TODO LOOKUP
	regfs_remove_fs(rdev);
	// cdev_delete() ?
	kfree(rdev);
	return 0;
}

EXPORT_SYMBOL_GPL(regfs_probe);
EXPORT_SYMBOL_GPL(regfs_probe_rdev);
EXPORT_SYMBOL_GPL(regfs_remove);
EXPORT_SYMBOL_GPL(regfs_event_fops);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ regfs driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
