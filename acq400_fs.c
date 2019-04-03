/* ------------------------------------------------------------------------- */
/* acq400_fs.c  D-TACQ ACQ400 FMC  DRIVER		                     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 * Copyright 2002, 2003 Jonathan Corbet <corbet@lwn.net>
 * This file may be redistributed under the terms of the GNU GPL.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pagemap.h> 	/* PAGE_CACHE_SIZE */
#include <linux/fs.h>     	/* This is where libfs stuff is declared */
#include <asm/atomic.h>
#include <asm/uaccess.h>	/* copy_to_user */

#include "acq400.h"
#include "hbm.h"

#include "acq400_fs_ioctl.h"

int dma_cache_invalidate_limit = -1;
module_param(dma_cache_invalidate_limit, int, 0644);
MODULE_PARM_DESC(dma_cache_invalidate_limit, "cache invalidate is expensive .. limit it");


int pcomp;
module_param(pcomp, int, 0644);
MODULE_PARM_DESC(pcomp, "compensate phase (bytes)");

#include "cpsc2.h"
#define HAS_I(adev) (IS_CSPC2_COMMS(adev))
#define ACQ400_FS_MAGIC	0xd1ac0400

#define XX	0

struct InodeMap {
	struct acq400_dev *adev;
	int site;
	int channel;			/* 0: XX */
	//struct inode* inode;
	unsigned int ino;
};

#define IS_RAW(map)     ((map)->channel == XX)

struct A400_FS_PDESC {
	struct InodeMap *map;
	int word_offset;
	unsigned buffer_offset;
	int stride;
	int lbuf_len;
	void* lbuf;
};

#define FS_DESC(file)		((struct A400_FS_PDESC*)((file)->private_data))

struct FS_NODES {
	struct super_block *sb;
	struct inode *root;
	struct dentry *root_dentry;
	struct dentry *rawdir;
	struct dentry *chandata[MAXDEVICES+1]; /* index from 1 */

	int nmap;
	struct InodeMap *site0;
	struct InodeMap *maps;
	int imap;

	struct timespec mtime;
} FSN;

struct CaptureData {
	int is_cooked;
	int nsamples;
	int nchan;	/* number of channels the demuxer thought it was working with */
	char typ[8];	/* "short" or "long" */
} CAPDAT;

struct InodeMap* lookup_ino(unsigned ino)
{
	int ii;
	struct InodeMap* pmap = FSN.maps;

	for (ii = 0; ii < FSN.imap; ++ii, ++pmap){
		if (pmap->ino == ino){
			return pmap;
		}
	}
	return 0;
}


static unsigned update_inode_stats(struct inode *inode)
{
	struct InodeMap* pmap = lookup_ino(inode->i_ino);

	if (!pmap){
		return inode->i_size;
	}else if (pmap->channel > pmap->adev->nchan_enabled){
		return 0;
	}else{
		loff_t i_size = 0;
		if (CAPDAT.is_cooked){
			if (pmap->site == XX){
				i_size = 0;
			}else{
				i_size = CAPDAT.nsamples * pmap->adev->word_size;
			}
		}else{
			if (pmap->site == XX && pmap->site == 0){
				/* valid site 0 only .. */
				i_size = CAPDAT.nsamples *
						pmap->adev->word_size *
						CAPDAT.nchan;
			}else{
				i_size = 0;
			}
		}
		inode->i_mtime = FSN.mtime;
		return inode->i_size = i_size;
	}
}

static int ai_getattr(const struct path *path, struct kstat *stat,
		 u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_inode(path->dentry);
	int was;

	generic_fillattr(inode, stat);
	was = stat->size;
	stat->size = update_inode_stats(inode);
	stat->mtime = inode->i_mtime;

	dev_dbg(0, "%s 99 inode:%lu was:%d size:%lld", __FUNCTION__, inode->i_ino,  was, stat->size);
	return 0;
}


static struct inode *a400fs_make_inode(struct super_block *sb, int mode)
{
	struct inode *inode = new_inode(sb);


	if (inode) {
		inode->i_ino = get_next_ino();
		inode->i_mode = mode;
		inode->i_uid = KUIDT_INIT(0);
		inode->i_gid = KGIDT_INIT(0);
		inode->i_blocks = 0;
		inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	}
	dev_dbg(0, "%s return inode %lu", __FUNCTION__, inode->i_ino);
	return inode;
}


int raw_offset_w(struct InodeMap* map)
{
	struct acq400_dev* adev0 = FSN.site0->adev;
	struct acq400_sc_dev* sc_dev = container_of(adev0, struct acq400_sc_dev, adev);

	int offset;
	int ia;
	for (offset = 0, ia = 0; ia < MAXDEVICES; ++ia){
		dev_dbg(DEVP(adev0), "compare site %d [%d] ret %d",
							map->site, ia, offset);
		if (sc_dev->aggregator_set[ia] == map->adev){
			dev_dbg(DEVP(adev0), "found site %d [%d] ret %d",
					map->site, ia, offset);
			return offset;
		}else{
			offset += sc_dev->aggregator_set[ia]->nchan_enabled;
		}
	}

	dev_err(DEVP(adev0), "ERROR: site %d not in aggregator", map->site);
	return -1;
}

int chan_offset_w(struct InodeMap* map)
{
	int raw_off = raw_offset_w(map);
	if (raw_off >= 0){
		return raw_off + map->channel - 1; /* assumes no gaps in channel set */
	}else{
		return raw_off;
	}
}

int get_buffer_offset_b(int word_offset)
{
	int buffer_len = FSN.site0->adev->bufferlen;
	int totchan = CAPDAT.nchan;

	return word_offset*buffer_len/totchan;
}
/*
 * Open a file.  All we have to do here is to copy over a
 * copy of the counter pointer so it's easier to get at.
 */
static int a400fs_open(struct inode *inode, struct file *file)
{
	struct A400_FS_PDESC* pd = kzalloc(sizeof(struct A400_FS_PDESC), GFP_KERNEL);
	int rc = 0;
#define RETERR(errcode) do { 						 	\
		rc = errcode;								\
		dev_err(0, "ERROR: a400fs_open() at line %d rc %d", __LINE__, rc);	\
}while(0)

	if (pd == 0){
		RETERR(-ENODEV);
	}else if ((pd->map = lookup_ino(inode->i_ino)) == 0){
		RETERR(-ENODEV);
	}else if ((pd->word_offset = IS_RAW(pd->map)? 0: chan_offset_w(pd->map)) < 0){
		RETERR(-ENODEV);
	}else{
		pd->buffer_offset = get_buffer_offset_b(pd->word_offset);
		pd->stride = 1;

		dev_dbg(DEVP(pd->map->adev), "a400fs_open() %d.%d wo:%d bo:%d",
				pd->map->site, pd->map->channel, pd->word_offset, pd->buffer_offset);

		file->private_data = pd;
		return 0;
	}

	dev_err(0, "a400fs_open FAIL\n");
	if (pd) kfree(pd);
	return rc;
}

int _a400fs_open_raw(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = FSN.site0->adev;
	int bufferlen = adev->bufferlen;
	int setlen = CAPDAT.nsamples*adev->word_size*CAPDAT.nchan;
	int ibuf;

	if (dma_cache_invalidate_limit != -1 &&
			setlen > dma_cache_invalidate_limit){
		setlen = dma_cache_invalidate_limit;
	}
	for (ibuf = 0; ibuf*bufferlen <= setlen; ++ibuf){
		struct HBM *hbm = adev->hb[ibuf];
		dev_dbg(DEVP(adev),
			"_a400fs_open_raw() dma_sync_single_for_cpu: pa:0x%08x len:%d dir:%d",
						hbm->pa, hbm->len, hbm->dir);
		dma_sync_single_for_cpu(DEVP(adev), hbm->pa, hbm->len, DMA_FROM_DEVICE);
	}

	return 0;
}
int a400fs_open_raw(struct inode *inode, struct file *file)
{
	int rc = a400fs_open(inode, file);
	unsigned acc_mode = file->f_flags & O_ACCMODE;

	if (rc == 0 && (acc_mode==O_RDONLY || acc_mode==O_RDWR)){
		return _a400fs_open_raw(inode, file);
	}else{
		return rc;
	}
}

#define TMPSIZE 80
#if 0
static ssize_t a400fs_read_file(struct file *file, char *buf,
		size_t count, loff_t *offset)
{
	struct A400_FS_PDESC* pd = FS_DESC(file);
	struct InodeMap *map = pd->map;
	char tmp[TMPSIZE];
	int len = snprintf(tmp, TMPSIZE, "%u %d.%02d word_off:%d buff_offset:%d\n",
			map->ino, map->site, map->channel,
			pd->word_offset, pd->buffer_offset);
	if (*offset > len)
		return 0;
	if (count > len - *offset)
		count = len - *offset;
/*
 * Copy it back, increment the offset, and we're done.
 */
	if (copy_to_user(buf, tmp + *offset, count))
		return -EFAULT;
	*offset += count;
	return count;
}
#endif
/*
 * offset: offset in bytes
 */

ssize_t _a400fs_read_raw_file(struct file *file, char *buf,
		size_t count, loff_t *offset)
{
	struct acq400_dev* adev0 = FSN.site0->adev;
	int foffset = (int)*offset;			/* we don't have 4GB */
	int bufferlen = adev0->bufferlen;
	int ibuf = foffset/bufferlen;
	int buff_offset = foffset - ibuf*bufferlen;
	int buf_headroom = bufferlen - buff_offset;
	int set_headroom = CAPDAT.nsamples*adev0->word_size*CAPDAT.nchan - foffset;

	int headroom = min(buf_headroom, set_headroom);

	dev_dbg(DEVP(adev0), "%s [%d] + %d head:%d count:%d", __FUNCTION__,
			ibuf, buff_offset, headroom, count);

	if (set_headroom <= 0){
		return 0;
	}else if (!(ibuf >= 0 || ibuf < adev0->nbuffers)){
		dev_err(DEVP(adev0), "bad ibuf %d", ibuf);
		return -1;
	}else{
		char *bp = (char*)(adev0->hb[ibuf]->va);
		if (count > headroom){
			count = headroom;
		}
		if (copy_to_user(buf, bp+buff_offset, count)){
			return -EFAULT;
		}
		*offset += count;
		return count;
	}
}

static ssize_t a400fs_read_raw_file(struct file *file, char *buf,
		size_t count, loff_t *offset)
{
	struct acq400_dev* adev0 = FSN.site0->adev;

	if (adev0->bufferlen){
		return _a400fs_read_raw_file(file, buf, count, offset);
	}else{
		dev_err(DEVP(adev0), "bufferlen not set");
		return -1;
	}
}

static ssize_t a400fs_read_chan_file(struct file *file, char *buf,
		size_t count, loff_t *offset)
{
	struct A400_FS_PDESC* pd = FS_DESC(file);
	struct acq400_dev* adev0 = FSN.site0->adev;
	int _foffset = (int)*offset;			/* we don't have 4GB */
	int foffset = _foffset == 0? pcomp: _foffset;
	int bufferlen = adev0->bufferlen/CAPDAT.nchan;
	int ibuf = foffset/bufferlen;
	int buff_offset = foffset - ibuf*bufferlen;	/* independent of stride */
	int buf_headroom = bufferlen - buff_offset;
	int set_headroom = CAPDAT.nsamples*adev0->word_size + pcomp - foffset;
	int cursor = buff_offset+pd->buffer_offset;

	int headroom = min(buf_headroom, set_headroom);

	if (_foffset == 0 && pcomp){
		*offset += pcomp;
	}
	if (pd->map->channel > pd->map->adev->nchan_enabled){
		return -1;
	}else if (set_headroom <= 0){
		return 0;
	}else if (!(ibuf >= 0 || ibuf < adev0->nbuffers)){
		dev_err(DEVP(adev0), "bad ibuf %d", ibuf);
		return -1;
	}else{
		char *bp = (char*)(adev0->hb[ibuf]->va);

		dev_dbg(DEVP(adev0), "[%d] %p %p + %d->%d ^%d head:%d count:%d",
					ibuf, bp, bp+cursor,
					pd->buffer_offset, buff_offset,
					cursor, headroom, count);

		if (count > headroom){
			count = headroom;
		}
		if (pd->stride == 1){
			if (copy_to_user(buf, bp+cursor, count)){
				return -EFAULT;
			}
			*offset += count;
		}else{
			int wsize = adev0->word_size;
			int wcount;
			int iw;
			if (!pd->lbuf){
				/* lazy allocation .. chances are all calls to
				 * read() in a single session will have same (or smaller) count
				 * free() on release.
				 */
				pd->lbuf = kmalloc(count, GFP_KERNEL);
				pd->lbuf_len = count;
			}else if (unlikely(count > pd->lbuf_len)){
				count = pd->lbuf_len;
			}
			count /= pd->stride;
			wcount = count/wsize;
			dev_dbg(DEVP(adev0), "stride:%d count:%d wcount:%d", pd->stride, count, wcount);

			for (iw = 0; iw < wcount; ++iw){
				memcpy(pd->lbuf+iw*wsize, bp+cursor+iw*pd->stride*wsize, wsize);
			}
			if (copy_to_user(buf, pd->lbuf, iw*wsize)){
				return -EFAULT;
			}
			*offset += count*pd->stride;
		}

		return count;
	}
}
/*
 * Write a file.
 */
static ssize_t a400fs_write_file(struct file *file, const char *buf,
		size_t count, loff_t *offset)
{
	return -EPERM;
}

int a400fs_release(struct inode *inode, struct file *file)
{
	struct A400_FS_PDESC* pd = FS_DESC(file);

	dev_dbg(DEVP(pd->map->adev), "a400fs_release() : %d.%d wo:%d bo:%d",
		pd->map->site, pd->map->channel, pd->word_offset, pd->buffer_offset);

	if (pd->lbuf) kfree(pd->lbuf);
	kfree(pd);
	return 0;
}


static long
a400fs_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct A400_FS_PDESC* pd = FS_DESC(file);
	switch(cmd){
	case ACQ400_FS_STRIDE:
		pd->stride = arg;
		return 0;
	default:
		return -ENODEV;
	}
}
/*
 * Now we can put together our file operations structure.
 */
static struct file_operations a400fs_chan_file_ops = {
	.open	= a400fs_open,
	.read 	= a400fs_read_chan_file,
	.write  = a400fs_write_file,
	.unlocked_ioctl = a400fs_unlocked_ioctl,
	.llseek = generic_file_llseek,
	.release= a400fs_release
};

static struct file_operations a400fs_raw_file_ops = {
	.open	= a400fs_open_raw,
	.read 	= a400fs_read_raw_file,
	.write  = a400fs_write_file,
	.release= a400fs_release
};
/*
 * Create a file mapping a name to a counter.
 */
static struct inode *a400fs_create_file (struct super_block *sb,
		struct dentry *dir, const char *name,
		struct file_operations* fops,
		struct InodeMap* inodeMap)
{
	static struct inode_operations inops = {
		.getattr = ai_getattr
	};
	struct dentry *dentry;
	struct inode *inode;
/*
 * Now we can create our dentry and the inode to go with it.
 */

	dentry = d_alloc_name(dir, name);
	if (! dentry)
		goto out;
	inode = a400fs_make_inode(sb, S_IFREG | 0644);
	if (! inode)
		goto out_dput;
	inode->i_fop = fops;
	inode->i_op = &inops;
	if (inodeMap) {
		inodeMap->ino = inode->i_ino;
	}
/*
 * Put it all into the dentry cache and we're done.
 */
	d_add(dentry, inode);
	dev_dbg(0, "%s 98 SUCCESS \"%s\"", __FUNCTION__, name);
	return inode;
/*
 * Then again, maybe it didn't work.
 */
  out_dput:
	dput(dentry);
  out:
  	dev_warn(0, "%s 99 FAILED to create \"%s\"", __FUNCTION__, name);
	return 0;
}



/*
 * Create a directory which can be used to hold files.  This code is
 * almost identical to the "create file" logic, except that we create
 * the inode with a different mode, and use the libfs "simple" operations.
 */
static struct dentry *a400fs_create_dir (struct super_block *sb,
		struct dentry *parent, const char *name)
{
	struct dentry *dentry;
	struct inode *inode;

	dentry = d_alloc_name(parent, name);
	if (! dentry){
		return NULL;
	}

	inode = a400fs_make_inode(sb, S_IFDIR | 0644);
	if (!inode) {
		dev_warn(0, "%s 99 FAILED to create \"%s\"", __FUNCTION__, name);
		dput(dentry);
		return NULL;
	}

	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	d_add(dentry, inode);
	dev_dbg(0, "%s 98 SUCCESS \"%s\"", __FUNCTION__, name);
	return dentry;

}

#define a400fs_create_raw_file a400fs_create_file
#define a400fs_create_channel_file a400fs_create_file



static const char* channels[] = {
	"00error",
	"01", "02", "03", "04", "05", "06", "07", "08",
	"09", "10", "11", "12", "13", "14", "15", "16",
	"17", "18", "19", "20", "21", "22", "23", "24",
	"25", "26", "27", "28", "29", "30", "31", "32"
};

struct InodeMap*
_build_mapping(struct FS_NODES *fsn, struct acq400_dev *adev,
		int site, int channel)
{
	struct InodeMap map;
	int ii = fsn->imap++;
	map.adev = adev;
	map.site = site;
	map.channel = channel;
	fsn->maps[ii] = map;	/* STRUCTURE COPY */
	if (map.site == 0){
		fsn->site0 = &fsn->maps[ii];
	}
	return &fsn->maps[ii];
}
struct InodeMap*
build_mapping(struct FS_NODES *fsn, struct acq400_dev *adev,
		int site, int channel)
{
	if (likely(fsn->imap < fsn->nmap)){
		return _build_mapping(fsn, adev, site, channel);
	}else{
		dev_err(DEVP(adev), "ERROR mapping count mismatch");
		return 0;
	}
}

void a400fs_add_site(int site, struct acq400_dev *adev, struct FS_NODES *fsn)
{
	int ic;
	struct inode *inode;

	dev_info(DEVP(adev), "a400fs_add_site() 01 site:%d sb:%p",
			site, fsn->sb);

	inode = a400fs_create_raw_file(
		fsn->sb, fsn->rawdir, adev->site_no,
		&a400fs_raw_file_ops, build_mapping(fsn, adev, site, XX));

	dev_dbg(DEVP(adev), "a400fs_add_site() 19 site:%d", site);

	fsn->chandata[site] =
		a400fs_create_dir(fsn->sb, fsn->root_dentry, adev->site_no);

	if(fsn->chandata[site] == 0){
		dev_warn(DEVP(adev), "site:%d failed to create_dir", site);
		return;
	}

	for (ic = 1; ic <= adev->nchan_enabled; ++ic){
		inode =	a400fs_create_channel_file(
				fsn->sb, fsn->chandata[site],
				channels[ic],
				&a400fs_chan_file_ops,
				build_mapping(fsn, adev, site, ic));
		if (inode == 0){
			dev_err(DEVP(adev), "ERROR failed to create channel file");
		}
	}
	dev_dbg(DEVP(adev), "a400fs_add_site() 99 site:%d", site);
}


static int a400fs_ctrl_open(struct inode *inode, struct file *file)
{
	return 0;
}

#define LINESIZE	80

static ssize_t a400fs_ctrl_read_file(struct file *file, char *buf,
		size_t count, loff_t *offset)
{
	char tmp[LINESIZE];
	int len = snprintf(tmp, LINESIZE, "COOKED=%d NSAMPLES=%d NCHAN=%d TYPE=%s\n",
			CAPDAT.is_cooked, CAPDAT.nsamples, CAPDAT.nchan, CAPDAT.typ);
	if (*offset > len)
		return 0;
	if (count > len - *offset)
		count = len - *offset;
/*
 * Copy it back, increment the offset, and we're done.
 */
	if (copy_to_user(buf, tmp + *offset, count))
		return -EFAULT;
	*offset += count;
	return count;
}

static ssize_t a400fs_ctrl_write_file(struct file *file, const char *buf,
		size_t count, loff_t *offset)
{
	char tmp[LINESIZE];
	struct CaptureData capdat;

	if (count > LINESIZE-1){
		count = LINESIZE-2;
	}
	if (copy_from_user(tmp, buf, count)){
		return -EFAULT;
	}
	tmp[count+1] = '\0';


	if (sscanf(tmp, "COOKED=%d NSAMPLES=%d NCHAN=%d TYPE=%7s",
			&capdat.is_cooked, &capdat.nsamples,
			&capdat.nchan, capdat.typ) == 4){
		CAPDAT = capdat;
		FSN.mtime = current_time(FSN.root);
		return count;
	}else{
		return -EINVAL;
	}
	return -EPERM;
}

static struct file_operations a400fs_control_ops = {
	.open	= a400fs_ctrl_open,
	.read 	= a400fs_ctrl_read_file,
	.write  = a400fs_ctrl_write_file,
};

static void a400fs_create_files (struct super_block *sb, struct dentry *root)
{
	struct acq400_dev *adev0 = acq400_devices[0];
	int dev;
	FSN.root_dentry = root;
	FSN.sb = sb;

	FSN.rawdir = a400fs_create_dir(sb, root, "raw");
	FSN.mtime = current_time(FSN.root);
	a400fs_create_file(sb, root, ".control", &a400fs_control_ops, 0);

	for (dev = 0; dev <= MAXDEVICES; ++dev){
		struct acq400_dev *adev = acq400_devices[dev];
		if (adev){
			FSN.nmap += adev->nchan_enabled + 1;
		}
	}

	FSN.maps = kmalloc(sizeof(struct InodeMap)*FSN.nmap, GFP_KERNEL);

	for (dev = 0; dev <= MAXDEVICES; ++dev){
		struct acq400_dev *adev = acq400_devices[dev];
		dev_dbg(DEVP(adev0), "dev:%d adev:%p", dev, adev);
		if (adev && (HAS_AI(adev) || IS_SC(adev) || IS_DIO432X(adev) || HAS_I(adev))){
			a400fs_add_site(adev->of_prams.site, adev, &FSN);
		}
	}
}


/*
 * Superblock stuff.  This is all boilerplate to give the vfs something
 * that looks like a filesystem to work with.
 */

/*
 * Our superblock operations, both of which are generic kernel ops
 * that we don't have to write ourselves.
 */
static struct super_operations a400fs_s_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
};

/*
 * "Fill" a superblock with mundane stuff.
 */
static int a400fs_fill_super (struct super_block *sb, void *data, int silent)
{
/*
 * Basic parameters.
 */
	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic = ACQ400_FS_MAGIC;
	sb->s_op = &a400fs_s_ops;
/*
 * We need to conjure up an inode to represent the root directory
 * of this filesystem.  Its operations all come from libfs, so we
 * don't have to mess with actually *doing* things inside this
 * directory.
 */
	FSN.root = a400fs_make_inode (sb, S_IFDIR | 0755);
	if (!FSN.root)
		goto out;
	FSN.root->i_op = &simple_dir_inode_operations;
	FSN.root->i_fop = &simple_dir_operations;
/*
 * Get a dentry to represent the directory in core.
 */
	FSN.root_dentry = d_make_root(FSN.root);
	if (! FSN.root_dentry)
		goto out_iput;
	sb->s_root = FSN.root_dentry;
/*
 * Make up the files which will be in this filesystem, and we're done.
 */
	a400fs_create_files (sb, FSN.root_dentry);

	return 0;

  out_iput:
	iput(FSN.root);
  out:
	return -ENOMEM;
}


/*
 * Stuff to pass in when registering the filesystem.
 */
static struct dentry *a400fs_get_super(struct file_system_type *fst,
		int flags, const char *devname, void *data)
{
	return mount_nodev(fst, flags, data, a400fs_fill_super);
}

static struct file_system_type a400fs_type = {
	.owner 		= THIS_MODULE,
	.name		= "acq400fs",
	.mount		= a400fs_get_super,
	.kill_sb	= kill_litter_super,
};


int a400fs_init(void)
{
	return register_filesystem(&a400fs_type);
}

void a400fs_exit(void)
{
	unregister_filesystem(&a400fs_type);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("peter.milne@d-tacq.com");
