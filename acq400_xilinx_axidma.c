/* ------------------------------------------------------------------------- */
/* acq400_xilinx_axidma.c ACQ420_FMC					     */
/*
 * acq400_xilinx_axidma.c
 *
 *  Created on: 10 Nov 2015
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
#include "bolo.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"

#include "dmaengine.h"
#include "include/linux/amba/xilinx_dma.h"
#include "xilinx_axidma.h"

#include <linux/dmapool.h>

extern int AXI_BUFFER_COUNT;
extern int AXI_ONESHOT;
extern int bufferlen;

extern unsigned AXI_HEAD_DESCR_PA;
extern unsigned AXI_TAIL_DESCR_PA;

#define S2MM_DMACR_CYC 0x10

int AXI_use_eot_interrupts = 1;
module_param(AXI_use_eot_interrupts, int, 0644);
MODULE_PARM_DESC(AXI_use_eot_interrupts, "1: enable interrupts");

int DUAL_AXI_SWITCH_CHAN = 1;
module_param(DUAL_AXI_SWITCH_CHAN, int, 0444);

int AXI_TASKLET_CHANNEL_MASK = 0x1;
module_param(AXI_TASKLET_CHANNEL_MASK, int, 0644);
MODULE_PARM_DESC(AXI_TASKLET_CHANNEL_MASK, "channel enables downstream, 1 is enough");

struct AxiDescrWrapper {
	struct xilinx_dma_desc_hw* va;
	unsigned pa;
};
#define xilinx_dma_desc_sz 	sizeof(struct xilinx_dma_desc_hw)
#define AXI_DESCR_WRAPPER_SZ	sizeof(struct AxiDescrWrapper)

struct SEGMENT {
	enum { DUMP, STORE } mode;
	unsigned short nblocks;
};
typedef struct SEGMENT Segment;

#define ACQ400_AXIPOOL_SZ	4096

struct ACQ400_AXIPOOL {
	int ndescriptors;
	char pool_name[16];
	struct dma_pool *pool;
	struct AxiDescrWrapper* channelWrappers[2];
	unsigned dump_pa;
	int active_descriptors;
	int segment_cursor;
	Segment segments[1];		/* first segment */
};

struct AxiChannelWrapper {
	int ichan;
	struct acq400_dev *adev;
};

#define GET_ACQ400_AXIPOOL(adev) \
	((struct ACQ400_AXIPOOL*)(adev)->axi_private)
#define MAX_SEGMENTS \
	((ACQ400_AXIPOOL_SZ-sizeof(struct ACQ400_AXIPOOL))/sizeof(Segment))

#define SEGDEF_LEN	5	/* [+-]NNN\n */

static void *acq400axi_proc_seq_start(struct seq_file *s, loff_t *pos)
{
	struct AxiChannelWrapper *acw = s->private;
	struct acq400_dev *adev = acw->adev;
	struct ACQ400_AXIPOOL* apool = (struct ACQ400_AXIPOOL*)adev->axi_private;

	if (apool == 0){
		dev_info(DEVP(adev), "NO POOL, come back later");
		return NULL;
	}
	if (apool->channelWrappers[acw->ichan] == NULL){
		seq_printf(s, "# AXI DESCRIPTORS[%d] 0/0\n", acw->ichan);
		return NULL;
	}
        if (*pos == 0) {
        	seq_printf(s, "# AXI DESCRIPTORS[%d] %d/%d\n", acw->ichan,
        			apool->active_descriptors, apool->ndescriptors);
        }
        if (*pos < apool->active_descriptors){
        	return &apool->channelWrappers[acw->ichan][*pos];
        }

        return NULL;
}


static void *acq400axi_proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct AxiChannelWrapper *acw = s->private;
	struct acq400_dev *adev = acw->adev;
	struct ACQ400_AXIPOOL* apool = (struct ACQ400_AXIPOOL*)adev->axi_private;

	if (++(*pos) < apool->active_descriptors){
		return &apool->channelWrappers[acw->ichan][*pos];
	}else{
		return NULL;
	}
}

int pa2index(struct ACQ400_AXIPOOL* apool, int ichan, unsigned pa)
{
	int ii;
	for (ii = 0; ii < apool->active_descriptors; ++ii){
		struct AxiDescrWrapper * cursor =
				apool->channelWrappers[ichan]+ii;
		if (cursor->pa == pa){
			return ii;
		}
	}

	return -1;
}

int pa2buffer_index(struct acq400_dev *adev, unsigned pa)
{
	int ii;
	for (ii = 0; ii != adev->nbuffers; ++ii){
		struct HBM* pb = adev->hb[ii];
		if (pb->pa == pa){
			return pb->ix;
		}
	}

	return ~0;
}

static int acq400axi_proc_seq_show_descr(struct seq_file *s, void *v)
{
	struct AxiChannelWrapper *acw = s->private;
        struct acq400_dev *adev = acw->adev;
        struct ACQ400_AXIPOOL* apool = (struct ACQ400_AXIPOOL*)adev->axi_private;
        struct AxiDescrWrapper * cursor = v;


        seq_printf(s, "i:%08x,%03d, n:0x%08x,%03d b:%08x l:%08x g:%03d %c\n",
        		cursor->pa, pa2index(apool, acw->ichan, cursor->pa),
        		cursor->va->next_desc, pa2index(apool, acw->ichan, cursor->va->next_desc),
        		cursor->va->buf_addr, cursor->va->control,
			pa2buffer_index(adev, cursor->va->buf_addr),
			apool->dump_pa==0? ' ':
				cursor->va->buf_addr==apool->dump_pa? '-': '+');

        return 0;
}

static void acq400_proc_seq_stop(struct seq_file *s, void *v)
{
}



static int acq400_proc_open_axi_descr(struct inode *inode, struct file *file)
{
	static struct seq_operations acq400_proc_seq_ops_channel_mapping = {
	        .start = acq400axi_proc_seq_start,
	        .next = acq400axi_proc_seq_next,
	        .stop = acq400_proc_seq_stop,
	        .show = acq400axi_proc_seq_show_descr
	};

	seq_open(file, &acq400_proc_seq_ops_channel_mapping);
	((struct seq_file*)file->private_data)->private = PDE_DATA(inode);
	return 0;
}
static struct file_operations acq400_proc_ops_axi_descr = {
        .owner = THIS_MODULE,
        .open = acq400_proc_open_axi_descr,
	.read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};

static void *axi_seg_proc_seq_start(struct seq_file *s, loff_t *pos)
{
	struct AxiChannelWrapper *acw = s->private;
	struct acq400_dev *adev = acw->adev;
	struct ACQ400_AXIPOOL* apool = (struct ACQ400_AXIPOOL*)adev->axi_private;

        if (*pos == 0) {
        	seq_printf(s, "# DMA SEGMENTS\n");
        }
        if (*pos < apool->segment_cursor){
        	return &apool->segments[*pos];
        }
        return NULL;
}

static void *axi_seg_proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct AxiChannelWrapper *acw = s->private;
	struct acq400_dev *adev = acw->adev;
	struct ACQ400_AXIPOOL* apool = (struct ACQ400_AXIPOOL*)adev->axi_private;
	Segment *seg = (Segment*)v;

	if (++seg - apool->segments < apool->segment_cursor){
		++(*pos);
		return (void*)seg;
	}else{
		return NULL;
	}
}

static int axi_seg_proc_seq_show(struct seq_file *s, void *v)
{
        Segment *seg = (Segment*)v;

        seq_printf(s, "%c%03d\n", seg->mode==DUMP? '-':'+', seg->nblocks);
        return 0;
}


static int acq400_proc_open_axi_segments(struct inode *inode, struct file *file)
{
	static struct seq_operations acq400_proc_seq_seg = {
	        .start = axi_seg_proc_seq_start,
	        .next = axi_seg_proc_seq_next,
	        .stop = acq400_proc_seq_stop,
	        .show = axi_seg_proc_seq_show
	};

	printk("acq400_proc_open_axi_descr() 01 \n");
	((struct seq_file*)file->private_data)->private = PDE_DATA(inode);
		seq_open(file, &acq400_proc_seq_seg);
	return 0;
}

int addLoop(struct acq400_dev *adev, int iseg, int loopstart, int loopcount)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	int iloop;
	int loop_cursor;
	int seg = iseg;

	for (iloop = 0; iloop < loopcount; ++iloop){
		for (loop_cursor = loopstart; loop_cursor < iseg;
						++seg, ++loop_cursor){
			if (seg >= MAX_SEGMENTS){
				dev_err(DEVP(adev),
					"addLoop() MAX_SEGMENTS %d exceeded",
					seg);
				return seg;
			}
			apool->segments[seg] = apool->segments[loop_cursor];
		}
	}

	return seg;
}
int parse_user_segments(struct acq400_dev *adev, char* ubuf, int count)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	char *cursor = ubuf;
	char *eol;
	int iseg = 0;


	for (; cursor - ubuf < count; cursor = eol + 1){
		char skip_or_store;
		int blocks;
		int loopcount = 0;
		int loopstart = 0;

		eol = strchr(cursor, '\n');
		if (eol == NULL){
			eol = cursor+strlen(cursor);
		}else{
			*eol = '\0';
		}

		if (strlen(cursor) == 0) continue;
		if (cursor[0] == '#') continue;


		dev_dbg(DEVP(adev), "parse_user_segments() [%d] \"%s\"",
				iseg, cursor);

		if (sscanf(cursor, "loop to=%d count=%d", &loopstart, &loopcount) == 2){
			if (loopstart >= iseg){
				dev_err(DEVP(adev), "loop to %d not available", loopstart);
				return -EINVAL;
			}
			iseg = addLoop(adev, iseg, loopstart, loopcount);
			continue;
		}

		if (sscanf(cursor, "%c%d", &skip_or_store, &blocks) == 2){
			if (iseg < MAX_SEGMENTS){
				switch(skip_or_store){
				case '+':
					apool->segments[iseg].mode = STORE;
					break;
				case '-':
					apool->segments[iseg].mode = DUMP;
					break;
				default:
					dev_err(DEVP(adev), "[%d] bad mode %c\n",
							iseg, skip_or_store);
					return -EINVAL;
				}
				apool->segments[iseg++].nblocks = blocks;
			}else{
				dev_err(DEVP(adev), "[%d] no space", iseg);
				return -ENOSPC;
			}
		}else{
			dev_err(DEVP(adev),
					"[%d] bad spec \"%s\"\n", iseg, cursor);
			return -EINVAL;
		}
	}
	apool->segment_cursor = iseg;
	dev_dbg(DEVP(adev), "parse_user_segments() OK %d", iseg);
	return count;
}
/**
 * This function is called with the /proc file is written
 *
 */

ssize_t acq400_proc_segments_write(
	struct file *file, const char *buffer, size_t count, loff_t *data)
{
	const char* dname = file->f_path.dentry->d_parent->d_iname;
	struct acq400_dev *adev = acq400_lookupSite(dname[0] -'0');
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	char* lbuf;
	int rc;

	if (apool == 0){
		return -ENODEV;
	}
	if (count > MAX_SEGMENTS*SEGDEF_LEN) {
		return -EFBIG;
	}

	lbuf = kmalloc(count+1, GFP_KERNEL);
	/* write data to the buffer */
	if (copy_from_user(lbuf, buffer, count) ) {
		rc = -EFAULT;
	}else{
		lbuf[count] = '\0';
		rc = parse_user_segments(adev, lbuf, count);
	}

	kfree(lbuf);

	return rc;
}
static struct file_operations acq400_proc_ops_axi_segments = {
        .owner = THIS_MODULE,
        .open = acq400_proc_open_axi_segments,
        .read = seq_read,
	.write = acq400_proc_segments_write,
        .llseek = seq_lseek,
        .release = seq_release
};

extern int AXI_DEBUG_LOOPBACK_INDEX;

static void loop_to_self(struct AxiDescrWrapper *end)
{
	end->va->next_desc = end->pa;
}

static void _finalize_descriptor_chain(struct acq400_dev *adev, int ichan, int ndescriptors)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	struct AxiDescrWrapper* base = apool->channelWrappers[ichan];
	int ii;

	for (ii = 1; ii < ndescriptors; ++ii){
		base[ii-1].va->next_desc = base[ii].pa;
	}

	if (AXI_DEBUG_LOOPBACK_INDEX > 0){
		dev_info(DEVP(adev), "AXI_DEBUG_LOOPBACK_INDEX %d", AXI_DEBUG_LOOPBACK_INDEX);
		base[ndescriptors-1].va->next_desc = base[AXI_DEBUG_LOOPBACK_INDEX].pa;
	}else{
		base[ndescriptors-1].va->next_desc = base[0].pa;
	}
	apool->active_descriptors = ndescriptors;
}
static void init_descriptor_cache_nonseg(struct acq400_dev *adev, int ichan, int ndescriptors)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	int ii;
	struct AxiDescrWrapper * cursor = apool->channelWrappers[ichan];

	dev_dbg(DEVP(adev), "init_descriptor_cache_nonseg() %d %d", ichan, ndescriptors);
	for (ii = 0; ii < ndescriptors; ++ii, ++cursor){
		cursor->va = dma_pool_alloc(apool->pool, GFP_KERNEL, &cursor->pa);
		BUG_ON(cursor->va == 0);
		memset(cursor->va, 0, sizeof(struct xilinx_dma_desc_hw));
		cursor->va->buf_addr = adev->axi64[ichan].axi64_hb[ii]->pa;
		cursor->va->control = adev->bufferlen;
		dev_dbg(DEVP(adev), "init_descriptor_cache_nonseg() %d/%d", ii, ndescriptors);
	}
	dev_dbg(DEVP(adev), "init_descriptor_cache_nonseg ichan:%d ndesc:%d", ichan, ndescriptors);

	adev->axi64[ichan].ndesc = ndescriptors;
	_finalize_descriptor_chain(adev, ichan, ndescriptors);
}

static void init_descriptor_cache_segmented(struct acq400_dev *adev, int ichan, int ndescriptors)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	struct AxiDescrWrapper * base = apool->channelWrappers[ichan];
	struct AxiDescrWrapper * wrapper = base;
	int last_buffer = ndescriptors-1;
	int ihb = 0;
	int iseg;

	dev_dbg(DEVP(adev), "init_descriptor_cache_segmented() 01 %d", ndescriptors);
	apool->dump_pa = adev->axi64[ichan].axi64_hb[last_buffer]->pa;

	for (iseg = 0 ; iseg < apool->segment_cursor; ++iseg){
		Segment* segment = &apool->segments[iseg];
		int ib;
		for (ib = 0; ib < segment->nblocks; ++ib, ++wrapper){
			if (wrapper->va == 0){
				wrapper->va =
					dma_pool_alloc(apool->pool, GFP_KERNEL, &wrapper->pa);
			}
			memset(wrapper->va, 0, sizeof(struct xilinx_dma_desc_hw));

			if (segment->mode == STORE){
				if (ihb >= last_buffer){
					dev_info(DEVP(adev), "DONE: out of host buffers");
					break;
				}
				wrapper->va->buf_addr = adev->axi64[0].axi64_hb[ihb++]->pa;
			}else{
				wrapper->va->buf_addr = apool->dump_pa;
			}
			wrapper->va->control = adev->bufferlen;
		}
	}
	dev_dbg(DEVP(adev), "init_descriptor_cache_segmented() 88 %d", wrapper-base);
	_finalize_descriptor_chain(adev, ichan, wrapper-base);
}

typedef void (*init_cache_fun)(struct acq400_dev *adev, int ichan, int ndescriptors);

int get_segmented_descriptor_total(struct acq400_dev *adev)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	int ii;
	int ndesc = 0;

	for (ii = 0; ii < apool->segment_cursor; ++ii){
		ndesc += apool->segments[ii].nblocks;
	}
	return ndesc;
}

static void _delete_pool(struct ACQ400_AXIPOOL* apool, int ichan)
{
	struct AxiDescrWrapper * cursor = apool->channelWrappers[ichan];
	int iw = 0;
	for (; iw < apool->ndescriptors; ++iw, ++cursor){
		dma_pool_free(apool->pool, cursor->va, cursor->pa);
	}
	kfree(apool->channelWrappers[ichan]);
}
static void delete_pool(struct ACQ400_AXIPOOL* apool)
{
	_delete_pool(apool, 0);
	_delete_pool(apool, 1);			// @@todo what if no wrappers ichan==1 ?
	apool->ndescriptors = 0;
}
static void init_descriptor_cache(struct acq400_dev *adev, unsigned cmask)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	init_cache_fun init_cache = apool->segment_cursor?
			init_descriptor_cache_segmented:
			init_descriptor_cache_nonseg;

	int nchan = cmask == (CMASK0|CMASK1) ? 2: 1;
	int ndescriptors = AXI_BUFFER_COUNT/nchan;
	int ic;

	if ((cmask&(CMASK0|CMASK1)) == 0){
		dev_err(DEVP(adev), "ERROR CMASK not set, no DMAC available");
		return;
	}

	if (apool->segment_cursor){
		int ndrequest = get_segmented_descriptor_total(adev);
		if (ndrequest > ndescriptors){
			dev_warn(DEVP(adev),
			"WARNING: too many descriptors requested, trim from %d to %d",
			ndrequest, ndescriptors);
		}
	}

	if (apool->ndescriptors && apool->ndescriptors < ndescriptors){
		delete_pool(apool);
	}
	if (apool->ndescriptors == 0){
		// @@todo single ichan only
		for (ic = 0; ic < 2; ++ic){
			if ((1<<ic)&cmask){
				apool->channelWrappers[ic] =
					kzalloc(AXI_DESCR_WRAPPER_SZ*ndescriptors, GFP_KERNEL);
			}
		}

		apool->ndescriptors = ndescriptors;
	}

	for (ic = 0; ic < 2; ++ic){
		if ((1<<ic)&cmask){
			init_cache(adev, ic, ndescriptors);
		}
	}
}
void axi64_arm_dmac(struct xilinx_dma_chan *xchan, unsigned headpa, unsigned tailpa, unsigned oneshot)
{
	unsigned cr = dma_read(xchan, XILINX_DMA_CONTROL_OFFSET);
	unsigned halted, not_halted;
	unsigned rs_check;

	xilinx_dma_reset(xchan);
	halted = dma_read(xchan, XILINX_DMA_STATUS_OFFSET);
	dma_write(xchan, XILINX_DMA_CDESC_OFFSET, headpa);
	if (!oneshot){
		dma_write(xchan, XILINX_DMA_CONTROL_OFFSET, cr = S2MM_DMACR_CYC);
	}
	if (AXI_use_eot_interrupts){
		cr |= XILINX_DMA_XR_IRQ_ALL_MASK;
	}else{
		cr &= ~XILINX_DMA_XR_IRQ_ALL_MASK;
	}
	dma_write(xchan, XILINX_DMA_CONTROL_OFFSET, cr|XILINX_DMA_CR_RUNSTOP_MASK);
	rs_check = dma_read(xchan, XILINX_DMA_CONTROL_OFFSET);
	not_halted = dma_read(xchan, XILINX_DMA_STATUS_OFFSET);
	if ((rs_check&XILINX_DMA_CR_RUNSTOP_MASK) == 0){
		dev_err(xchan->dev, "NOT ENABLED CR=%08x", rs_check);
	}
	if ((not_halted&XILINX_DMA_SR_HALTED_MASK) != 0){
		dev_err(xchan->dev, "HALTED: but we wanted to GO! SR=%08x", not_halted);
	}
	dma_write(xchan, XILINX_DMA_TDESC_OFFSET, tailpa);
}

// put marker in reg to show we were there ...
#define SHOTID(adev)	(((adev->stats.shot&0x00ff)<<8)|0xcafe0000)

extern int wimp_out;

void acq400_dma_on_ioc(unsigned long arg)
{
	struct xilinx_dma_chan *xchan = (struct xilinx_dma_chan *)arg;
	struct acq400_dev *adev = (struct acq400_dev *)xchan->client_private;
	unsigned cursor_pa = dma_read(xchan, XILINX_DMA_CDESC_OFFSET);
	/* STOP at end of one-shot?
	struct dma_chan* dma_chan = &chan->common;
	dma_chan->device->device_control(dma_chan, DMA_TERMINATE_ALL, 0);
	*/
	adev->rt.axi64_ints++;
	wake_up_interruptible(&adev->DMA_READY);
	dev_dbg(DEVP(adev), "acq400_dma_on_ioc() done pa:%08x\n", cursor_pa);
}
void _start_transfer(struct xilinx_dma_chan *chan)
{

}


int _axi64_load_dmac(struct acq400_dev *adev, int ichan)
{
	struct xilinx_dma_chan *xchan = to_xilinx_chan(adev->dma_chan[ichan]);
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	struct AxiDescrWrapper* wrappers = apool->channelWrappers[ichan];

	int ihead = 0;
	int itail = -1;

	u32 tail_pa = AXI_ONESHOT? wrappers[itail = apool->ndescriptors-2].pa: SHOTID(adev);
	u32 head_pa = AXI_ONESHOT>=1? wrappers[ihead = apool->ndescriptors-2-AXI_ONESHOT].pa: wrappers[0].pa;

	if (AXI_ONESHOT >=1){
		dev_info(DEVP(adev), "AXI_ONESHOT %d head %d tail %d", AXI_ONESHOT, ihead, itail);
	}
	dev_dbg(DEVP(adev), "_axi64_load_dmac() 01 xchan:%p", xchan);
	xchan->client_private = adev;
	xchan->start_transfer = _start_transfer;
	/* DUAL-AXI: multiple wakeups for concurrent event not helpful */
	if (((1<<ichan)&AXI_TASKLET_CHANNEL_MASK) != 0){
		tasklet_init(&xchan->tasklet, acq400_dma_on_ioc, (unsigned long)xchan);
	}

	dev_dbg(DEVP(adev), "_axi64_load_dmac() 50");

	if (!wimp_out){
		acq400_set_AXI_DMA_len(adev, adev->bufferlen);
		axi64_arm_dmac(xchan, head_pa, tail_pa, AXI_ONESHOT);
	}
	dev_dbg(DEVP(adev), "_axi64_load_dmac() 99");
	return 0;
}

void __axi64_init_procfs(
	struct acq400_dev *adev, struct proc_dir_entry *root, int ic)
{
	const char* fn = ic==0? "AXI.0": "AXI.1";
	struct AxiChannelWrapper* acw =
		kzalloc(sizeof(struct AxiChannelWrapper), GFP_KERNEL);
	acw->adev = adev;
	acw->ichan = ic;
	dev_info(DEVP(adev), "__axi64_init_procfs() acw:%p", acw);
	proc_create_data(fn, 0, root, &acq400_proc_ops_axi_descr, acw);
}
void _axi64_init_procfs(struct acq400_dev *adev)
{
	__axi64_init_procfs(adev, adev->proc_entry, 0);
	__axi64_init_procfs(adev, adev->proc_entry, 1);
}
void axi64_init_procfs(struct acq400_dev *adev)
{
	proc_create_data(
		"SEGMENTS", 0, adev->proc_entry,
		&acq400_proc_ops_axi_segments, adev);
	_axi64_init_procfs(adev);
}

int axi64_init_dmac(struct acq400_dev *adev)
{
	struct ACQ400_AXIPOOL* apool = kzalloc(ACQ400_AXIPOOL_SZ, GFP_KERNEL);

	dev_dbg(DEVP(adev), "axi64_init_dmac() 01");
	snprintf(apool->pool_name, 16, "acq400_axi_pool");
	apool->pool = dma_pool_create(apool->pool_name, DEVP(adev),
		sizeof(struct xilinx_dma_desc_hw), 0x40, 0);

	BUG_ON(apool->pool == 0);
	adev->axi_private = apool;

	axi64_init_procfs(adev);
	dev_dbg(DEVP(adev), "axi64_init_dmac() 99");
	return 0;
}

int axi64_load_dmac(struct acq400_dev *adev, unsigned cmask)
{
	int rc;
	int ichan;

	dev_dbg(DEVP(adev), "axi64_load_dmac() 01");

	if (adev->axi_private == 0){
		axi64_init_dmac(adev);
	}
	dev_dbg(DEVP(adev), "axi64_load_dmac() 02");
	init_descriptor_cache(adev, cmask);

	dev_dbg(DEVP(adev), "axi64_load_dmac() 03");
	for (ichan = 0; cmask != 0; cmask >>=1, ++ichan){
		if ((cmask&1) != 0){
			rc = _axi64_load_dmac(adev, ichan);
		}
		if (rc != 0){
			dev_err(DEVP(adev), "_axi64_load_dmac %d returned %d", ichan, rc);
			return rc;
		}
	}
	dev_dbg(DEVP(adev), "axi64_load_dmac() 99");
	return 0;
}

int axi64_tie_off_dmac(struct acq400_dev *adev, int ichan, int nbuffers)
/* close off descriptor +nbuffers to prevent overrun */
/* find cursor in set, count ahead nbuffers, tie it off */
{
	struct xilinx_dma_chan *xchan = to_xilinx_chan(adev->dma_chan[ichan]);
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	struct AxiDescrWrapper* wrappers = apool->channelWrappers[ichan];


	int imax = apool->ndescriptors;

	unsigned cursor_pa = dma_read(xchan, XILINX_DMA_CDESC_OFFSET);
	int ii = (cursor_pa - wrappers[0].pa)/xilinx_dma_desc_sz;

	for (ii = 0; ii < imax; ++ii){
		if (wrappers[ii].pa == cursor_pa){
			int itie = ii + nbuffers;
			if (itie > imax){
				itie -= imax;
			}
			loop_to_self(&wrappers[itie]);
			dev_dbg(DEVP(adev),
				"axi64_tie_off_dmac() at %d call loop_to_self %d", ii, itie);
			return 0;
		}
	}

	dev_err(DEVP(adev),
		"FAILED to locate cursor_pa 0x%08x in range 0x%08x..0x%08x",
		cursor_pa, wrappers[0].pa, wrappers[0].pa+imax*xilinx_dma_desc_sz);
	return -1;
}


void axi64_terminate(struct dma_chan* dma_chan)
{
	dev_dbg(&dma_chan->dev->device, "axi64_terminate()");
	dma_cookie_init(dma_chan);
	dmaengine_terminate_all(dma_chan);
	xilinx_dma_reset(to_xilinx_chan(dma_chan));
}

int axi64_free_dmac(struct acq400_dev *adev)
{
	struct ACQ400_AXIPOOL* apool = GET_ACQ400_AXIPOOL(adev);
	adev->axi_private = 0;

	delete_pool(apool);
	dma_pool_destroy(apool->pool);
	kfree(apool);
	return 0;
}

bool filter_axi(struct dma_chan *chan, void *param)
{
	struct acq400_dev *adev = (struct acq400_dev *)param;
	const char* dname = chan->device->dev->driver->name;
	dev_info(DEVP(adev), "filter_axi: %s\n", chan->device->dev->driver->name);

	if (dname != 0 && strcmp(dname, "xilinx-acq400-dma") == 0){
		return true;
	}else{
		return false;
	}
}


int axi64_claim_dmac_channels(struct acq400_dev *adev)
/* MUST be called first .. */
{
	char c0, c1;
	dma_cap_mask_t mask;
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	adev->dma_chan[0] = dma_request_channel(mask, filter_axi, adev);
	adev->dma_chan[1] = dma_request_channel(mask, filter_axi, adev);
	if (adev->dma_chan[0] && adev->dma_chan[1]){
		if (DUAL_AXI_SWITCH_CHAN){
			swap(adev->dma_chan[0], adev->dma_chan[1]);
			dev_info(DEVP(adev), "axi64_claim_dmac_channels switch chan");
			c0 = 'B'; c1 = 'A';
		}else{
			c0 = 'A'; c1 = 'B';
		}
	}else if (adev->dma_chan[0]){
		c0 = 'A'; c1 = 'x';
	}else if (adev->dma_chan[1]){
		c0 = 'x'; c1 = 'A';
	}else{
		c0 = c1 = 'x';
		dev_err(DEVP(adev), "axi64_claim_dmac_channels no channels");
	}

	dev_info(DEVP(adev),
		"axi_dma not using standard driver using channels %c %c", c0, c1);

	return 0;
}

static int dma_reset(struct xilinx_dma_chan *chan)
{
	int loop = XILINX_DMA_RESET_LOOP;
	u32 tmp;

	dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
		 // dma_read(chan, XILINX_DMA_CONTROL_OFFSET) |
		  XILINX_DMA_CR_RESET_MASK);

	tmp = dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
	      XILINX_DMA_CR_RESET_MASK;

	/* Wait for the hardware to finish reset */
	while (loop && tmp) {
		tmp = dma_read(chan, XILINX_DMA_CONTROL_OFFSET) &
		      XILINX_DMA_CR_RESET_MASK;
		loop -= 1;
	}

	if (!loop) {
		dev_err(chan->dev, "reset timeout, cr %x, sr %x\n",
			dma_read(chan, XILINX_DMA_CONTROL_OFFSET),
			dma_read(chan, XILINX_DMA_STATUS_OFFSET));
		return -EBUSY;
	}

	return 0;
}

void dma_halt(struct xilinx_dma_chan *chan)
{
	dev_dbg(chan->dev, "%s", __FUNCTION__);
	dma_write(chan, XILINX_DMA_CONTROL_OFFSET,
		dma_read(chan, XILINX_DMA_CONTROL_OFFSET) & ~XILINX_DMA_CR_RUNSTOP_MASK);
	dma_write(chan, XILINX_DMA_TDESC_OFFSET, 0);
}
int xilinx_dma_reset(struct xilinx_dma_chan *chan)
{
	dev_dbg(chan->dev, "xilinx_dma_reset()");
	return dma_reset(chan);
}

int xilinx_dma_reset_dmachan(struct dma_chan *chan)
{
	return xilinx_dma_reset(to_xilinx_chan(chan));
}

void xilinx_dma_halt(struct dma_chan *chan)
{
	dma_halt(to_xilinx_chan(chan));
}



