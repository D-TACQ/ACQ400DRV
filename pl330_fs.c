/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2011 Peter Milne, D-TACQ Solutions Ltd
 *                    <Peter dot Milne at D hyphen TACQ dot com>

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

/** @file pl330_fs_.c DESCR
 * 
 *  Created on: Feb 10, 2012
 *      Author: pgm
 */

#include <asm/io.h>

#include <asm/types.h>

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <linux/slab.h>

#include "debugfs2.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#include <linux/slab.h>
#include <linux/kdev_t.h>

#include <linux/moduleparam.h>

#define REVID "pl330_fs B1001"

static struct dentry *top;
static struct dentry *create_hook;

static struct DebugFs2NodeInfo pl330_fs_regs_info;

u32 *va_base;

#define LO32(addr) (((unsigned)(addr) & 4) == 0)

#define MAXSTACK 4

static struct dentry *dstack[MAXSTACK];
static int istack;

static void cd(char *dir)
{
	if (strcmp(dir, "/") == 0) {
		istack = 0;
	} else if (strcmp(dir, "..") == 0) {
		if (istack > 0) {
			istack -= 1;
		}
	} else {
		if (istack + 1 >= MAXSTACK) {
			dev_err(0, "stack depth %d reached\n", MAXSTACK);
		} else {
			struct dentry *cwd =
				debugfs_create_dir(dir, dstack[istack]);
			if (cwd == 0) {
				dev_err(0, "failed to create subdir\n");
			} else {
				dstack[++istack] = cwd;
				dev_info(0, "cd %s OK\n", dir);
			}
		}
	}
}

static struct dentry *cwd(void)
{
	return dstack[istack];
}

#define CD "cd "

static struct DebugFs2NodeInfo *nodeCreate(const struct DebugFs2NodeInfo *def)
{
	struct DebugFs2NodeInfo *nodeInfo = kmalloc(DBGFS2_NI_SZ, GFP_KERNEL);
	memcpy(nodeInfo, def, DBGFS2_NI_SZ);
	return nodeInfo;
}
static ssize_t pl330_fs_write(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	static struct DebugFs2NodeInfo *defaultNodeInfo = &pl330_fs_regs_info;
	char myline[80];
	ssize_t rc = debugfs2_write_line(
		file, user_buf, min(count, sizeof(myline) - 1), ppos, myline);
	myline[79] = '\0';

	if (rc > 0 && myline[0] != '#' && strlen(myline) > 3) {
		if (strncmp(myline, CD, strlen(CD)) == 0) {
			cd(myline + strlen(CD));
		} else {
			struct DebugFs2NodeInfo *nodeInfo =
				nodeCreate(defaultNodeInfo);
			debugfs2_create_file_def(cwd(), nodeInfo, myline,
						 (int)*ppos);
		}
		/* @@todo: newfile -> list */
	}

	return rc;
}

static int pl330_fs_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

const struct file_operations pl330_fs_fops = { .write = pl330_fs_write,
					       .open = pl330_fs_open };

static int __init pl330_fs_init(void)
{
	dev_info(0, "%s pl330_fs_init()\n", REVID);

	pl330_fs_regs_info.pwrite = pl330_fs_regs_info.pread =
		pl330_fs_regs_info.pcache = ioremap(0xF8003000, 0x1000);

	dstack[istack = 0] = top = debugfs_create_dir("pl330", NULL);
	create_hook =
		debugfs_create_file(".create", S_IWUGO, top, 0, &pl330_fs_fops);
	return 0;
}

static void __exit pl330_fs_remove(void)
{
	dev_info(0, "%s pl330_fs_remove()\n", REVID);
	debugfs_remove(create_hook);
	/* big leak here ... need to rm all nodes, inc Info's */
	debugfs_remove(top);
}

module_init(pl330_fs_init);
module_exit(pl330_fs_remove);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter.Milne@d-tacq.com");
MODULE_DESCRIPTION("pl330 device knobs interface");
