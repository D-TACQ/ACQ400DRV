/* ------------------------------------------------------------------------- */
/* acq400_ui.c  D-TACQ ACQ400 FMC  DRIVER                                   
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

#include "acq400.h"
#include "bolo.h"
#include "hbm.h"

#include "acq400_lists.h"
#include "acq400_ui.h"

//int event_to = HZ/2;
int event_to = 0x7fffffff;		// ie infinity
module_param(event_to, int, 0644);
MODULE_PARM_DESC(event_to, "backstop event time out should be one TBLOCK");

int acq400_event_count_limit = 1;
module_param(acq400_event_count_limit, int, 0644);
MODULE_PARM_DESC(acq400_event_count_limit, "limit number of events per shot 0: no limit");


int defer_stream_dac_task_init = 1;
module_param(defer_stream_dac_task_init, int, 0644);
MODULE_PARM_DESC(defer_stream_dac_task_init, "delay starting stream_dac task until we have data in buffer");


int subrate_wr_timing = 0;
module_param(subrate_wr_timing, int, 0644);
MODULE_PARM_DESC(subrate_wr_timing, "set true for WR_TAI timing, else use ADC SAMPLE COUNT");

int subrate_verbose = 0;
module_param(subrate_verbose, int, 0644);
MODULE_PARM_DESC(subrate_verbose, "view subrate gather pattern");

int xo400_awg_open(struct inode *inode, struct file *file)
/* if write mode, reset length */
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	if ( (file->f_flags & O_ACCMODE) == O_WRONLY) {
		xo400_reset_playloop(adev, xo_dev->AO_playloop.length = 0);
	}
	return 0;
}
int xo400_awg_release(struct inode *inode, struct file *file)
/* if it was a write, commit to memory and set length */
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);

	if ( (file->f_flags & O_ACCMODE) != O_RDONLY) {
		xo_dev->AO_playloop.oneshot =
			iminor(inode) == AO420_MINOR_HB0_AWG_ONCE? AO_oneshot:
			iminor(inode) == AO420_MINOR_HB0_AWG_ONCE_RETRIG? AO_oneshot_rearm:
					AO_continuous;
		xo400_reset_playloop(adev, xo_dev->AO_playloop.length);
	}
	return 0;
}

ssize_t xo400_awg_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	int pll = AOSAMPLES2BYTES(adev, xo_dev->AO_playloop.length);
	unsigned len = adev->cursor.hb[0]->len;

	unsigned bcursor = *f_pos;	/* f_pos counts in bytes */
	unsigned ib = bcursor/len;
	unsigned offset = bcursor - ib*len;
	int rc;

	if (bcursor >= pll){
		return 0;
	}else{
		int headroom = min(len-offset, pll-bcursor);
		if (count > headroom){
			count = headroom;
		}
	}
	rc = copy_to_user(buf, (char*)adev->cursor.hb[ib]->va + offset, count);
	if (rc){
		return -1;
	}


	*f_pos += count;
	return count;
}

void flip_x16_sign_bit(unsigned short* bp, int nwords)
{
	const unsigned short sbit = 1<<15;

	while(nwords--){
		*bp++ ^= sbit;
	}
}
ssize_t xo400_awg_write(
	struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	unsigned len = adev->cursor.hb[0]->len;

	unsigned bcursor = *f_pos;	/* f_pos counts in bytes */
	unsigned ib = bcursor/len;
	/** @@todo check ib within bounds .. what bounds? */
	unsigned offset = bcursor - ib*len;
	int headroom = (len - offset);
	char* dst = (char*)adev->cursor.hb[ib]->va + offset;
	int rc;

	if (count > headroom){
		count = headroom;
	}

	rc = copy_from_user(dst, buf, count);

	if (rc){
		return -1;
	}
	if (IS_AO424(adev) && SPAN_IS_BIPOLAR(xo_dev)){
		flip_x16_sign_bit((unsigned short*)dst, count/sizeof(unsigned short));
	}

	dev_dbg(DEVP(adev),
		"dma_sync_single_for_device() %08x %d len:%d demand:%d %d (DMA_TO_DEVICE=%d)",
		adev->cursor.hb[ib]->pa + offset, count,
		len, bcursor+count,
		adev->cursor.hb[ib]->dir, DMA_TO_DEVICE);

	dma_sync_single_for_device(DEVP(adev),
			adev->cursor.hb[ib]->pa + offset, count, adev->cursor.hb[ib]->dir);

	*f_pos += count;
	xo_dev->AO_playloop.length += AOBYTES2SAMPLES(adev, count);
	return count;
}

int xo400_open_awg(struct inode *inode, struct file *file)
{
	static struct file_operations fops = {
			.open = xo400_awg_open,
			.write = xo400_awg_write,
			.read = xo400_awg_read,
			.release = xo400_awg_release,
	};
	file->f_op = &fops;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}

ssize_t acq400_histo_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	unsigned *the_histo = ACQ400_DEV(file)->fifo_histo;
	int maxentries = FIFO_HISTO_SZ;
	unsigned cursor = *f_pos;	/* f_pos counts in entries */
	int rc;

	if (cursor >= maxentries){
		return 0;
	}else{
		int headroom = (maxentries - cursor) * sizeof(unsigned);
		if (count > headroom){
			count = headroom;
		}
	}
	rc = copy_to_user(buf, the_histo+cursor, count);
	if (rc){
		return -1;
	}

	*f_pos += count/sizeof(unsigned);
	return count;
}



int acq400_open_histo(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_histo = {
			.read = acq400_histo_read,
	};
	acq400_fops_histo.release = file->f_op->release;
	file->f_op = &acq400_fops_histo;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}

static int acq400_hb_open(struct inode *inode, struct file *file)
{
        struct acq400_dev *adev = ACQ400_DEV(file);
        int ibuf = BUFFER(PD(file)->minor);
        struct HBM *hbm = adev->hb[ibuf];

	unsigned acc_mode = file->f_flags & O_ACCMODE;
	switch (acc_mode) {
	case O_RDONLY:
	case O_RDWR:
		dev_dbg(DEVP(adev),
			"acq400_hb_open() mode %x, dma_sync_single_for_cpu: pa:0x%08x len:%d dir:%d",
				acc_mode, hbm->pa, hbm->len, hbm->dir);
		dma_sync_single_for_cpu(DEVP(adev), hbm->pa, hbm->len, DMA_FROM_DEVICE);
	default:
		;
	}

	return 0;
}

int acq400_dma_mmap_host(struct file* file, struct vm_area_struct* vma)
/**
 * mmap the host buffer.
 */
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int ibuf = BUFFER(PD(file)->minor);
	struct HBM *hbm = adev->hb[ibuf];
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = hbm->len;
	unsigned pfn = hbm->pa >> PAGE_SHIFT;

	if (!IS_BUFFER(PD(file)->minor)){
		dev_warn(DEVP(adev), "ERROR: device node not a buffer");
		return -1;
	}
	dev_dbg(&adev->pdev->dev, "%c [%d] 0x%08x vsize %lu psize %lu %s",
		'D', hbm->ix,hbm->pa,
		vsize, psize, vsize>psize? "EINVAL": "OK");

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		return -EAGAIN;
	}else{
		return 0;
	}
}

ssize_t acq400_hb_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int ibuf = BUFFER(PD(file)->minor);
	struct HBM *hbm = adev->hb[ibuf];
	unsigned cursor = *f_pos;	/* f_pos counts in bytes */
	int rc;
	size_t copy_count;

/*
	dev_dbg(DEVP(adev), "acq400_hb_read() cursor:%u len:%u count:%u",
			cursor, hbm->len, count);
*/
	if (cursor >= hbm->len){
		return 0;
	}else{
		int headroom = hbm->len - cursor;
		if (count > headroom){
			count = headroom;
		}
		if (count == headroom){
			copy_count = count - POISON_SZ;
		}else{
			copy_count = count;
		}
	}
/*
	dev_dbg(DEVP(adev), "acq400_hb_read() copy_to [%d] %p %d",
			ibuf, (char*)hbm->va+cursor, count);
*/
	rc = copy_to_user(buf, (char*)hbm->va + cursor, copy_count);
	if (rc){
		return -1;
	}
	if (copy_count == count-POISON_SZ){
		unsigned po_bytes = poison_offset(adev);
		unsigned first_word = FIRST_POISON_WORD(po_bytes);
		unsigned px[2];
		px[0] = hbm->va[first_word+0];
		if (px[0] == POISON0){
			dev_dbg(DEVP(adev),
				"acq400_hb_read() poison replaced %08x:%08x",
							px[0], hbm->poison_data[0]);
			px[0] = hbm->poison_data[0];
		}
		px[1] = hbm->va[first_word+1];
		if (px[1] == POISON1){
			dev_dbg(DEVP(adev),
				"acq400_hb_read() poison replaced %08x:%08x",
							px[1], hbm->poison_data[1]);
			px[1] = hbm->poison_data[1];
		}

		rc = copy_to_user(buf+copy_count, (char*)px, POISON_SZ);
		if (rc){
			return -1;
		}
	}

	*f_pos += count;
/*
	dev_dbg(DEVP(adev), "acq400_hb_read() return count:%u", count);
*/
	return count;
}

ssize_t acq400_hb_write(
	struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int ibuf = BUFFER(PD(file)->minor);
	struct HBM *hb = adev->hb[ibuf];
	unsigned cursor = *f_pos;	/* f_pos counts in bytes */
	int rc;

	dev_dbg(DEVP(adev), "acq400_hb_write() cursor:%u len:%u count:%u",
			cursor, hb->len, count);

	if (cursor >= hb->len){
		return -ENOSPC;
	}else{
		int headroom = hb->len - cursor;
		if (count > headroom){
			count = headroom;
		}
	}
	dev_dbg(DEVP(adev), "acq400_hb_write() copy_from [%d] %p %d",
				ibuf, (char*)hb->va+cursor, count);
	rc = copy_from_user((char*)hb->va+cursor, buf, count);

	if (rc){
		return -1;
	}

	*f_pos += count;
	dev_dbg(DEVP(adev), "acq400_hb_write() return count:%u", count);

	return count;
}


int acq400_hb_release(struct inode *inode, struct file *file)
{
        struct acq400_dev *adev = ACQ400_DEV(file);
        int ibuf = BUFFER(PD(file)->minor);
        struct HBM *hbm = adev->hb[ibuf];

        unsigned acc_mode = file->f_flags & O_ACCMODE;

        switch (acc_mode) {
        case O_WRONLY:
        case O_RDWR:
        	dev_dbg(DEVP(adev),
        	"acq400_hb_release() mode %x, dma_sync_single_for_device: pa:0x%08x len:%d dir:%d",
		acc_mode, hbm->pa, hbm->len, hbm->dir);
        	dma_sync_single_for_device(DEVP(adev), hbm->pa, hbm->len, DMA_TO_DEVICE);
        default:
        	;
        }

        return acq400_release(inode, file);
}



int acq400_open_hb(struct inode *inode, struct file *file)
{
	static struct file_operations acq420_fops_dma = {
		.open = acq400_hb_open,
		.mmap = acq400_dma_mmap_host,
		.read = acq400_hb_read,
		.write = acq400_hb_write,
		.release = acq400_hb_release,
		// sendfile method is no more.. it's probably not quite this easy ..
		// sure enough, it's not !
		// most likely the HB's have to be on a block device ..
		.splice_read	= generic_file_splice_read
	};
	file->f_op = &acq420_fops_dma;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}


extern int hb0_no_ratelimit;

ssize_t acq400_hb0_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	char HB0[16];

	int rc;

	unsigned hb0_count;

	if (hb0_no_ratelimit){
		/* have we moved on from last time? */
		hb0_count = HB0_COUNT(PD(file));
	}else{
		/* force wait until next .. this is very conservative */
		hb0_count = adev->rt.hb0_count;
	}

	if (wait_event_interruptible(
			adev->hb0_marker,
			adev->rt.hb0_count != hb0_count ||
			adev->rt.refill_error ||
			adev->rt.please_stop)){
		return -EINTR;
	} else if (adev->rt.please_stop){
		return -GET_FULL_DONE;
	} else if (adev->rt.refill_error){
		return -GET_FULL_REFILL_ERR;
	}
	HB0_COUNT(PD(file)) = adev->rt.hb0_count;
	sprintf(HB0, "%d %d\n", adev->rt.hb0_ix[0], adev->rt.hb0_ix[1]);
	count = min(count, strlen(HB0)+1);
	rc = copy_to_user(buf, HB0, count);
	if (rc){
		return -1;
	}

	*f_pos += count;
	return count;
}

int acq420_open_hb0(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_hb0 = {
			.read = acq400_hb0_read,
			.release = acq400_release
	};

	HB0_COUNT(PD(file)) = -1;
	file->f_op = &acq400_fops_hb0;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}

struct GPG_buffer* get_gpg(struct acq400_dev* adev, int gpg32)
{
	if (IS_DIO482_PG(adev)){
		struct PG_dev* pg_dev = container_of(adev, struct PG_dev, adev);
		dev_dbg(DEVP(adev), "is:%s gpg_buffer:%p gpg_base:%p gpg_cursor:%u",
				pg_dev->id, pg_dev->gpg.gpg_buffer, pg_dev->gpg.gpg_base, pg_dev->gpg.gpg_cursor);
		if (gpg32){
			return &pg_dev->gpg32;
		}else{
			return &pg_dev->gpg;
		}
	}else{
		struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
		return &sc_dev->gpg;
	}
}

#define GPG32(file) (PD(file)->minor == ACQ400_MINOR_GPGMEM32)

int acq400_gpgmem_open(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct GPG_buffer* gpg = get_gpg(adev, GPG32(file));
	if (file->f_flags & O_WRONLY) {
		int iw;
		for (iw = 0; iw < gpg->gpg_cursor; ++iw){
			iowrite32(0, gpg->gpg_base+iw);
		}
		gpg->gpg_cursor = 0;
	}
	return 0;
}
ssize_t acq400_gpgmem_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct GPG_buffer* gpg = get_gpg(adev, GPG32(file));
	int len = gpg->gpg_cursor*sizeof(u32);
	unsigned bcursor = *f_pos;	/* f_pos counts in bytes */
	int rc;

	if (bcursor >= len){
		return 0;
	}else{
		int headroom = (len - bcursor);
		if (count > headroom){
			count = headroom;
		}
	}
	rc = copy_to_user(buf, gpg->gpg_buffer+bcursor, count);
	if (rc){
		return -1;
	}

	*f_pos += count;
	return count;
}

ssize_t acq400_gpgmem_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct GPG_buffer* gpg = get_gpg(adev, GPG32(file));
	int len = GPG_MEM_ACTUAL;
	unsigned bcursor = *f_pos;	/* f_pos counts in bytes */
	int rc;

	if (bcursor >= len){
		return 0;
	}else{
		int headroom = (len - bcursor);
		if (count > headroom){
			count = headroom;
		}
	}
	rc = copy_from_user(gpg->gpg_buffer+bcursor, buf, count);

	if (rc){
		return -1;
	}
	*f_pos += count;
	gpg->gpg_cursor += count/sizeof(u32);
	return count;

}

int set_gpg_top(struct acq400_dev* adev, u32 gpg_count)
{
	if (gpg_count >= 2){
		unsigned GPG_CR = IS_DIO482_PG(adev)? DIO482_PG_GPGCR: GPG_CONTROL;
		u32 gpg_ctrl = acq400rd32(adev, GPG_CR);
		u32 gpg_top = gpg_count - 1		// was count, not address
					 -1;		// GPG_2ND_LAST_ADDR
		gpg_top <<= GPG_CTRL_TOPADDR_SHL;
		gpg_top &= GPG_CTRL_TOPADDR;
		gpg_ctrl &= ~GPG_CTRL_TOPADDR;
		gpg_ctrl |= gpg_top;
		acq400wr32(adev, GPG_CR, gpg_ctrl);
		return 0;
	}else{
		dev_err(DEVP(adev), "set_gpg_top() ERROR: must have 2 or more entries");
		return -1;
	}
}

int acq400_gpgmem_release(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct GPG_buffer* gpg = get_gpg(adev, GPG32(file));
	int iw;
	int rc;

	if (GPG32(file)){
		for (iw = 0; iw < gpg->gpg_cursor; ++iw){
			iowrite32(gpg->gpg_buffer[iw], gpg->gpg_base+iw);
		}
	}else{
		gpg->gpg_used_bits = 0;

		for (iw = 0; iw < gpg->gpg_cursor; ++iw){
			unsigned stl_entry = gpg->gpg_buffer[iw];
			unsigned stl_state = stl_entry &0x00000ff;

			iowrite32(stl_entry, gpg->gpg_base+iw);

			gpg->gpg_final_state = stl_state;
			gpg->gpg_used_bits  |= stl_state;
		}
		dev_dbg(DEVP(adev), "acq400_gpgmem_release() %d used:%08x fin:%08x\n", iw, gpg->gpg_used_bits, gpg->gpg_final_state);
		rc = set_gpg_top(adev, gpg->gpg_cursor);
	}
	acq400_release(inode, file);
	return rc;
}

int acq420_open_gpgmem(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_gpgmem = {
			.open = acq400_gpgmem_open,
			.read = acq400_gpgmem_read,
			.write = acq400_gpgmem_write,
			.release = acq400_gpgmem_release
	};
	file->f_op = &acq400_fops_gpgmem;
	return file->f_op->open(inode, file);
}


ssize_t acq400_aofifo_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_path_descriptor* pd = PD(file);
	int rc;

	if (count > MAXLBUF){
		count = MAXLBUF;
	}
	rc = copy_from_user(pd->lbuf, buf, count);

	if (rc){
		return rc;
	}

	write32(pd->dev->dev_virtaddr+AXI_FIFO, (volatile u32*)pd->lbuf, count/sizeof(unsigned));

	return count;

}

int acq400_aofifo_mmap(struct file* file, struct vm_area_struct* vma)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = 0x1000;
	unsigned long pa = adev->dev_physaddr + AXI_FIFO;
	unsigned pfn = pa >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	dev_dbg(DEVP(adev), "acq400_aofifo_mmap pa:0x%08lx vsize %lu psize %lu %s",
		pa, vsize, psize, vsize>psize? "EINVAL": "OK");

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (io_remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		return -EAGAIN;
	}else{
		return 0;
	}
}

int acq400_minor_aofifo_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_aofifo = {
			.write = acq400_aofifo_write,
			.mmap = acq400_aofifo_mmap
	};
	file->f_op = &acq400_fops_aofifo;
	return 0;
}

ssize_t acq400_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
{
	return -EINVAL;
}

ssize_t acq400_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
        return -EINVAL;
}

int acq400_mmap_bar(struct file* file, struct vm_area_struct* vma)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = adev->dev_addrsize;
	unsigned pfn = adev->dev_physaddr >> PAGE_SHIFT;

	dev_dbg(DEVP(adev), "%c vsize %lu psize %lu %s",
		'D', vsize, psize, vsize>psize? "EINVAL": "OK");

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (io_remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		return -EAGAIN;
	}else{
		return 0;
	}
}


int acq400_mmap_bar_page(struct file* file, struct vm_area_struct* vma)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = 0x1000;
	unsigned long pa = adev->dev_physaddr + ACQ400_MINOR_MAP_PAGE_OFFSET(PD(file)->minor);
	unsigned pfn = pa>> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	dev_dbg(DEVP(adev), "acq400_mmap_bar_page pa:0x%08lx vsize %lu psize %lu %s",
		pa, vsize, psize, vsize>psize? "EINVAL": "OK");

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (io_remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		return -EAGAIN;
	}else{
		return 0;
	}
}

int acq400_map_page_open(struct inode* inode, struct file* file)
{
	static struct file_operations acq400_map_page_fops = {
	        .owner = THIS_MODULE,
	        .release = acq400_release,
	        .mmap = acq400_mmap_bar_page
	};
	file->f_op = &acq400_map_page_fops;
	return 0;
}

int acq400_mmap_bar_atd(struct file* file, struct vm_area_struct* vma)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = AXI_ATD_LEN;
	unsigned pfn = (adev->dev_physaddr + AXI_ATD_RAM)>> PAGE_SHIFT;

	dev_dbg(DEVP(adev), "%c vsize %lu psize %lu %s",
		'D', vsize, psize, vsize>psize? "EINVAL": "OK");

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (io_remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		return -EAGAIN;
	}else{
		return 0;
	}
}

int acq400_atd_open(struct inode* inode, struct file* file)
{
	static struct file_operations acq400_atd_fops = {
	        .owner = THIS_MODULE,
	        .release = acq400_release,
	        .mmap = acq400_mmap_bar_atd
	};
	file->f_op = &acq400_atd_fops;
	return 0;
}

ssize_t acq420_sew_fifo_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int ix = PD(file)->minor - ACQ400_MINOR_SEW1_FIFO;

	int rc = acq400_sew_fifo_write_bytes(adev, ix, buf, count);

	if (rc > 0){
		*f_pos += rc;
	}
	return rc;
}
int acq420_sew_fifo_release(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int ix = PD(file)->minor - ACQ400_MINOR_SEW1_FIFO;
	return acq400_sew_fifo_destroy(adev, ix);
}

int acq420_sew_fifo_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq420_sew_fifo = {
			.write = acq420_sew_fifo_write,
			.release = acq420_sew_fifo_release,
	};
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int ix = PD(file)->minor - ACQ400_MINOR_SEW1_FIFO;
	int busy;

	if (mutex_lock_interruptible(&adev->awg_mutex)) {
		return -EINTR;
	}
	busy = sc_dev->sewFifo[ix].sf_task != 0;
	if (!busy){
		acq400_sew_fifo_init(adev, ix);
	}
	mutex_unlock(&adev->awg_mutex);
	if (busy){
		return -EBUSY;
	}

	file->f_op = &acq420_sew_fifo;
	return 0;
}




int acq420_reserve_release(struct inode *inode, struct file *file)
/* if it was a write, commit to memory and set length */
{
	replace_all(PD(file));
	return acq400_release(inode, file);
}

ssize_t acq420_reserve_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	wait_queue_head_t waitq;	/* no one gonna wake this sucker */

	init_waitqueue_head(&waitq);

	wait_event_interruptible(waitq, 0);  /* but it will accept a signal .. */
	return -EINTR;
}

int acq420_reserve_dist_open(struct inode *inode, struct file *file)
/* simply reserve block 0 on open. Can get clever later */
{
	static struct file_operations fops = {
		.open = acq420_reserve_dist_open,
		.read = acq420_reserve_read,
		.release = acq420_reserve_release,
	};

	file->f_op = &fops;
	return acq400_reserve_dist_buffers(PD(file));
}
int acq420_reserve_open(struct inode *inode, struct file *file)
/* simply reserve block 0 on open. Can get clever later */
{
	static struct file_operations fops = {
		.open = acq420_reserve_open,
		.read = acq420_reserve_read,
		.release = acq420_reserve_release,
	};
	file->f_op = &fops;
	return reserve(PD(file), 0);
}


#define AXIMAXLINE	80

#include <linux/ctype.h>


ssize_t acq400_axi_once_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
/* write a list of binary unsigned char ibufs ..
 * if there's NO write, then the default is to use the single ibuf=0
 */
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	char labuf[AXIMAXLINE+1];
	const char *ps;
	int ii = 0;
	int ibuf;
	size_t rc;
	int cursor = 0;

	dev_dbg(DEVP(adev), "%s 01", __FUNCTION__ );


	if (count > AXIMAXLINE){
		count = AXIMAXLINE;
	}
	rc = copy_from_user(labuf, buf, count);
	labuf[count] = '\0';



	if (rc != 0){
		return -1;
	}
	for (ps = labuf; sscanf(ps, "%d%n", &ibuf, &cursor) > 0; ps += cursor){
		dev_dbg(DEVP(adev), "%s [%d] %d", __FUNCTION__, ii, ibuf);
		pdesc->lbuf[ii++] = ibuf;
	}

	dev_dbg(DEVP(adev), "%s write %d nbuf:%d %02x,%02x,%02x,%02x",
			__FUNCTION__, count, ii,
			pdesc->lbuf[0], pdesc->lbuf[1],
			pdesc->lbuf[2], pdesc->lbuf[3]);

	if (rc == 0){
		*f_pos = ii;
		axi64_data_once(adev, (unsigned char*)PD(file)->lbuf, ii);
		rc = count;
	}
	return count;
}
int acq400_axi_once_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	char lbuf[32];
	int bc, rc;

	dev_dbg(DEVP(adev), "%s 01 pos:%u", __FUNCTION__, *(unsigned*)f_pos);

	axi64_data_once(adev, (unsigned char*)PD(file)->lbuf, 1);
	bc = snprintf(lbuf, min(sizeof(lbuf), count), "%u\n", *(unsigned*)f_pos);
	rc = copy_to_user(buf, lbuf, bc);

	if (rc){
		return -rc;
	}else{
		*f_pos += bc;
		dev_dbg(DEVP(adev), "%s 99 pos:%u ret:%d", __FUNCTION__, *(unsigned*)f_pos, bc);
		return bc;
	}
}

int acq400_axi_once_release(struct inode *inode, struct file *file)
{
	struct acq400_dev *adev = ACQ400_DEV(file);

	dev_dbg(DEVP(adev), "%s 01", __FUNCTION__);

	sc_data_engine_disable(DATA_ENGINE_0);
	acq2106_distributor_reset_enable(adev);
	acq2106_aggregator_reset(adev);
	return acq400_release(inode, file);
}

int acq400_axi_dma_once_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_axi_once = {
		.write = acq400_axi_once_write,
		.read = acq400_axi_once_read,
		.release = acq400_axi_once_release
	};
	struct acq400_dev *adev = ACQ400_DEV(file);

	dev_dbg(DEVP(adev), "%s 01", __FUNCTION__);


	acq2106_distributor_reset_enable(adev);
	acq2106_aggregator_reset(adev);
	sc_data_engine_reset_enable(DATA_ENGINE_0);
	acq2006_aggregator_enable(adev);

	file->f_op = &acq400_fops_axi_once;
	return 0;
}

void acq400_init_event_info(struct EventInfo *eventInfo)
{
	struct acq400_dev* adev0 = acq400_lookupSite(0);
	memset(eventInfo, 0, sizeof(struct EventInfo));

	mutex_lock(&adev0->list_mutex);
	/* event is somewhere between these two blocks */
	if (!list_empty(&adev0->REFILLS)){
		eventInfo->hbm0 = list_last_entry(&adev0->REFILLS, struct HBM, list);
	}else if (!list_empty(&adev0->OPENS)){
		eventInfo->hbm0 = list_last_entry(&adev0->OPENS, struct HBM, list);
	}
	if (!list_empty(&adev0->INFLIGHT)){
		eventInfo->hbm1 = list_first_entry(&adev0->INFLIGHT, struct HBM, list);
	}
	mutex_unlock(&adev0->list_mutex);
	eventInfo->pollin = 1;
}

int acq400_event_open(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	u32 int_csr;
	u32 enable = IS_DIO482FMC(adev)? (DIO_INT_CSR_COS|DIO_INT_CSR_COS_EN) :
			ADC_INT_CSR_COS_EN_ALL;

	PD(file)->samples_at_event = adev->rt.samples_at_event;
	if (mutex_lock_interruptible(&adev->mutex)) {
		return -ERESTARTSYS;
	}else{
		++adev->event_client_count;
		mutex_unlock(&adev->mutex);
	}
	int_csr = x400_get_interrupt(adev);

	dev_dbg(DEVP(adev), "acq400_event_open() intcsr |= %x", enable);
	x400_set_interrupt(adev, int_csr|enable);

	/* good luck using this in a 64-bit system ... */
	/*
	setup_timer( &adev->event_timer, event_isr, (unsigned)adev);
	mod_timer( &adev->event_timer, jiffies + msecs_to_jiffies(event_isr_msec));
	*/
	return 0;
}



ssize_t acq400_event_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int nbytes;
	int rc;
	struct EventInfo eventInfo = PD(file)->eventInfo;
	struct acq400_dev* adev0 = acq400_lookupSite(0);
	u32 old_sample = PD(file)->samples_at_event;
	u32 new_sample;
	int timeout = 0;
	int event_source = 0;

	dev_dbg(DEVP(adev), "acq400_event_read() 01 pollin %d old_sample %d",
			eventInfo.pollin, old_sample);

	rc = wait_event_interruptible_timeout(
				adev->event_waitq,
				(new_sample = adev->rt.samples_at_event) != old_sample,
				event_to);
	if (rc < 0){
		return -EINTR;
	}else if (rc == 0){
		return -EAGAIN;
	}
	acq400_init_event_info(&eventInfo);

	dev_dbg(DEVP(adev), "acq400_event_read() hbm0 %p hbm1 %p %s",
			eventInfo.hbm0, eventInfo.hbm1,
			eventInfo.hbm1? "must wait for hbm1 to complete": "ready");

	if (eventInfo.hbm0){
		dma_sync_single_for_cpu(DEVP(adev), eventInfo.hbm0->pa, eventInfo.hbm0->len, eventInfo.hbm0->dir);
	}
	if (eventInfo.hbm1){
		int rc = wait_event_interruptible_timeout(
			adev0->refill_ready,
			eventInfo.hbm1->bstate != BS_FILLING ||
				adev0->rt.refill_error ||
				adev0->rt.please_stop,
			event_to);
		if (rc < 0){
			return -EINTR;
		}else if (rc == 0){
			timeout = 1;
		}
		dma_sync_single_for_cpu(DEVP(adev), eventInfo.hbm1->pa, eventInfo.hbm1->len, eventInfo.hbm1->dir);
	}

	if (HAS_DTD(adev)){
		struct XTD_dev *xtd_dev = container_of(adev, struct XTD_dev, adev);
		if (xtd_dev->atd.event_source){
			event_source = xtd_dev->atd.event_source;
			/* RACE: a fast second interrupt may already have been! */
			xtd_dev->atd.event_source = 0;
		}
	}

	nbytes = snprintf(PD(file)->lbuf, MAXLBUF, "%d %d %d %s 0x%08x %u %u %u\n",
			adev->rt.event_count,
			eventInfo.hbm0? eventInfo.hbm0->ix: -1,
			eventInfo.hbm1? eventInfo.hbm1->ix: -1, timeout? "TO": "OK",
			event_source,
			new_sample,
			adev->rt.samples_at_event_latch,
			adev->rt.samples_at_event-adev->rt.samples_at_event_latch);

	PD(file)->samples_at_event = new_sample;

	rc = copy_to_user(buf, PD(file)->lbuf, nbytes);
	if (rc != 0){
		rc = -1;
	}else{
		rc = nbytes;
	}
	dev_dbg(DEVP(adev), "acq400_event_read() 99 \"%s\" rc %d", PD(file)->lbuf, rc);
	return rc;
}


static unsigned int acq400_event_poll(
	struct file *file, struct poll_table_struct *poll_table)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned rc;

	if (adev->rt.samples_at_event != PD(file)->samples_at_event){
		acq400_init_event_info(&PD(file)->eventInfo);
		rc = POLLIN;
	}else{
		poll_wait(file, &adev->event_waitq, poll_table);
		if (adev->rt.samples_at_event != PD(file)->samples_at_event){
			acq400_init_event_info(&PD(file)->eventInfo);
			rc = POLLIN;
		}else{
			rc = 0;
		}
	}
	dev_dbg(DEVP(adev), "acq400_event_poll() return %u", rc);
	return rc;
}
int acq400_event_release(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	u32 enable = IS_DIO482FMC(adev)? (DIO_INT_CSR_COS|DIO_INT_CSR_COS_EN) :
			ADC_INT_CSR_COS_EN_ALL;

	if (mutex_lock_interruptible(&adev->mutex)) {
		return -ERESTARTSYS;
	}else{
		if (--adev->event_client_count == 0){
			u32 int_csr = x400_get_interrupt(adev);

			int_csr &= ~enable;
			x400_set_interrupt(adev, int_csr);
		}
		mutex_unlock(&adev->mutex);
	}
	return acq400_release(inode, file);
}

int acq400_open_event(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_event = {
			.open = acq400_event_open,
			.read = acq400_event_read,
			.poll = acq400_event_poll,
			.release = acq400_event_release
	};
	file->f_op = &acq400_fops_event;

	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}


ssize_t acq400_bq_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct BQ* bq = &pdesc->bq;
	char lbuf[32];
	int bc;
	int rc = -99;
	unsigned bn = 0;
	int retry;

	for (retry = 0; rc != 0; ++retry){
		dev_dbg(DEVP(adev), "wait_event_interruptible(%p)", &pdesc->waitq);
		if (wait_event_interruptible(
				pdesc->waitq,
				CIRC_CNT(bq->head, bq->tail, bq->bq_len))){
			return -EINTR;
		}
		rc = BQ_get_st(DEVP(adev), bq, &bn);
		if (rc != 0){
			dev_info(DEVP(adev), "BQ_get EMPTY retries %d", retry);
		}
	}

	bc = snprintf(lbuf, 32, "%03u\n", bn);

	if (bc > count){
		return -ENOSPC;
	}
	rc = copy_to_user(buf, lbuf, bc);
	if (rc){
		return -rc;
	}else{
		return bc;
	}
}

static unsigned int acq400_bq_poll(
	struct file *file, struct poll_table_struct *poll_table)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct BQ* bq = &pdesc->bq;

	if (BQ_space(bq)){
		return POLLIN|POLLRDNORM;
	}else{
		poll_wait(file, &pdesc->waitq, poll_table);
		if (BQ_space(bq)){
			return POLLIN|POLLRDNORM;
		}else{
			return 0;
		}
	}
}

static int bqw_init(struct acq400_path_descriptor* pdesc, struct BQ_Wrapper* bqw, int backlog)
{
        INIT_LIST_HEAD(&pdesc->bq_list);
        BQ_init(&pdesc->bq, backlog);

        if (mutex_lock_interruptible(&bqw->bq_clients_mutex)) {
	       return -ERESTARTSYS;
	}
        list_add_tail(&pdesc->bq_list, &bqw->bq_clients);

        mutex_unlock(&bqw->bq_clients_mutex);
        return 0;
}

static int bqw_release(struct acq400_path_descriptor* pdesc, struct BQ_Wrapper* bqw)
{
	if (mutex_lock_interruptible(&bqw->bq_clients_mutex)) {
	       return -ERESTARTSYS;
	}
	list_del_init(&pdesc->bq_list);
	mutex_unlock(&bqw->bq_clients_mutex);
	kfree(pdesc->bq.buf);
	return 0;
}
int acq400_bq_release(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int rc = 0;

	if ((rc = bqw_release(pdesc, &sc_dev->bqw)) != 0){
		return rc;
	}else{
		return acq400_release(inode, file);
	}
}


int acq400_bq_open(struct inode *inode, struct file *file, int backlog)
{
	static struct file_operations acq400_fops_bq = {
		.read = acq400_bq_read,
		.release = acq400_bq_release,
		.poll = acq400_bq_poll
	};

	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

        file->f_op = &acq400_fops_bq;

        bqw_init(pdesc, &sc_dev->bqw, backlog);
	return 0;
}

int acq400_nacc_service(struct acq400_dev* adev, struct Subrate* subrate)
{
	int imax = adev->nchan_enabled;
	int ii;

	if (!adev->data32){
		imax >>= 1;
	}
	adev->RW32_debug = 1;
	for (ii = 0; ii < imax; ++ii){
		subrate->raw[ii] = acq400rd32(adev, ADC_NACC_SAMPLES+ii);
	}
	adev->RW32_debug = 0;
	return 0;
}

/** keep it simple: read one sample, no block */
ssize_t acq400_nacc_subrate_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct ADC_dev *adc_dev = container_of(adev, struct ADC_dev, adev);
	int ss = adev->nchan_enabled*(adev->data32? sizeof(int): sizeof(short));
	struct Subrate* subrate = &adc_dev->subrate;
	int rc;

	if (count != ss){
		if (count < ss){
			dev_err(DEVP(adev), "ERROR: count %d less than ss %d", count, ss);
			return -1;
		}
		if (count > ss){
			if (*f_pos == 0){
				dev_warn(DEVP(adev), "ERROR: count %d not a multiple of ss %d", count, ss);
			}
			count = ss;
		}
	}
	acq400_nacc_service(adev, subrate);
	rc = copy_to_user(buf, subrate->raw, count);

	if (rc){
		return -1;
	}
	*f_pos += count;
	return count;
}

#define SUBRATE_TO_MS 10

#define MAX_DESC	(6+4)                         // 6 sites, 4 element "spad"
#define PD_GATHER_DESC(pdesc) (pdesc->client_private)


void acq400_sc_nacc_service(unsigned *lbuf, struct GatherDesc* gd0, int imax)
{
	struct GatherDesc *gd = gd0;

	for (gd = gd0; gd-gd0 < imax; ++gd){
		unsigned *ubuf = lbuf + gd->dst_idx;
		struct acq400_dev *sdev = gd->adev;
		unsigned imax = gd->n32;
		unsigned ii;
		for (ii = 0; ii < imax; ++ii){
			ubuf[ii] = acq400rd32(sdev, gd->src_off+ii*sizeof(unsigned));
		}
	}
}

ssize_t acq400_sc_nacc_subrate_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct GatherDesc *gd = (struct GatherDesc *)PD_GATHER_DESC(pdesc);
	struct GatherDesc *gdp = gd+MAX_DESC-1;
	int idesc;
	int n32 = 0;
	size_t ss;
	ssize_t rc;

	dev_dbg(DEVP(adev), "%s 01", __FUNCTION__);

	for (idesc = MAX_DESC-1; idesc >= 0; --idesc, --gdp){
		if (gdp->adev){
			n32 = gdp->dst_idx + gdp->n32;
			dev_dbg(DEVP(adev), "idesc:%d setting %d + %d = %d", idesc, gdp->dst_idx, gdp->n32, n32);
			break;
		}
	}
	ss = n32 * sizeof(unsigned);

	if (count != ss){
		if (count < ss){
			dev_err(DEVP(adev), "ERROR: count %d less than ss %d", count, ss);
			return -1;
		}
		if (count > ss){
			if (*f_pos == 0){
				dev_warn(DEVP(adev), "ERROR: count %d not a multiple of ss %d", count, ss);
			}
			count = ss;
		}
	}
	acq400_sc_nacc_service((unsigned*)pdesc->lbuf, gd, idesc+1);

	rc = copy_to_user(buf, pdesc->lbuf, count);

	if (rc){
		dev_dbg(DEVP(adev), "%s 88", __FUNCTION__);
		return -1;
	}
	*f_pos += count;
	dev_dbg(DEVP(adev), "%s 99", __FUNCTION__);
	return count;
}

int acq400_subrate_release(struct inode *inode, struct file *file)
{
	return acq400_release(inode, file);
}

int acq400_sc_nacc_subrate_release(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	kfree((void*)PD_GATHER_DESC(pdesc));
	return acq400_release(inode, file);
}

int _acq400_nacc_subrate_open(struct inode *inode, struct file *file)
{
	return 0;
}


int acq400_sc_nacc_subrate_open(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct GatherDesc* gd = kzalloc(MAX_DESC*sizeof(struct GatherDesc), GFP_KERNEL);
	struct GatherDesc* gd0 = gd;
	int idev = 0;
	unsigned dst_idx = 0;

	dev_dbg(DEVP(adev), "%s 01", __FUNCTION__);
	gd++;  				// skip first descriptor

	for (idev = 0; idev < MAXDEVICES; ++idev){
		struct acq400_dev* slave = sc_dev->aggregator_set[idev];
		dev_dbg(DEVP(adev), "%s idev:%d slave:%s dst_idx %d", __FUNCTION__, idev, slave? slave->site_no: "x", dst_idx);
		if (slave){
			unsigned n32 = slave->nchan_enabled >> (slave->data32? 0: 1);
			struct GatherDesc tmp = {
				.adev = slave,
				.src_off = ADC_NACC_SAMPLES,
				.n32 = n32,
				.dst_idx = dst_idx
			};
			dst_idx += n32;
			*gd++ = tmp;
		}else{
			break;
		}
	}
	/* now make a fake "SPAD". First descriptor is first reading */

	{
		struct acq400_dev* slave = sc_dev->aggregator_set[0];
		struct GatherDesc tmp0 = {						// SPAD[0] : ADC_SAMPLE_CTR
			.adev = slave,
			.src_off = ADC_SAMPLE_CTR,
			.n32 = 1,
			.dst_idx = dst_idx
		};
		*gd0 = tmp0; dst_idx += 1;

		if (subrate_wr_timing){						/* IF WR */
			struct GatherDesc tmp1 = {					// SPAD[1] WR_TAI_CUR_L
				.adev = adev,
				.src_off = WR_TAI_CUR_L,
				.n32 = 1,
				.dst_idx = dst_idx
			};
			struct GatherDesc tmp2 = {					// SPAD[2] WR_CUR_VERNR
				.adev = adev,
				.src_off = WR_CUR_VERNR,
				.n32 = 1,
				.dst_idx = dst_idx+1
			};
			*gd++ = tmp1; dst_idx += 1;
			*gd++ = tmp2; dst_idx += 1;
		}else{								/* OR */
			struct GatherDesc tmp1 = {					// SPAD[1] : ADC_CLOCK_CTR
				.adev = adev,
				.src_off = SPADN(1),
				.n32 = 1,
				.dst_idx = dst_idx
			};
			struct GatherDesc tmp2 = {					// SPAD[2] : SPAD[2] (USER)
				.adev = adev,
				.src_off = SPADN(2),
				.n32 = 1,
				.dst_idx = dst_idx+1
			};
			*gd++ = tmp1; dst_idx += 1;
			*gd++ = tmp2; dst_idx += 1;
		}
		tmp0.dst_idx = dst_idx;
		*gd++ = tmp0; dst_idx += 1;					// SPAD[3] ADC_SAMPLE_CTR again: measure cost of collection
	}
	if (subrate_verbose){
		dev_dbg(DEVP(adev), "%s subrate_verbose", __FUNCTION__);
		for (gd = gd0; gd-gd0 < MAX_DESC; ++gd){
			if (gd->adev){
				dev_info(DEVP(adev), "[%2u] %s 0x%04x dst:%d len:%d", gd-gd0, gd->adev->dev_name, gd->src_off, gd->dst_idx, gd->n32);
			}else{
				dev_info(DEVP(adev), "[%2u]", gd-gd0);
			}
		}
	}

	PD_GATHER_DESC(pdesc) = (unsigned)gd0;
	dev_dbg(DEVP(adev), "%s 99", __FUNCTION__);
	return 0;
}
int acq400_nacc_subrate_open(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	static struct file_operations acq400_fops_subrate = {
		.open = _acq400_nacc_subrate_open,
		.read = acq400_nacc_subrate_read,
		.release = acq400_subrate_release
	};
	static struct file_operations acq400_sc_fops_subrate = {
		.open = acq400_sc_nacc_subrate_open,
		.read = acq400_sc_nacc_subrate_read,
		.release = acq400_subrate_release
	};

	if (IS_SC(adev)){
		file->f_op = &acq400_sc_fops_subrate;
	}else{
		file->f_op = &acq400_fops_subrate;
	}
	return file->f_op->open(inode, file);
}



int streamdac_data_loop_dummy(void *data)
{
	struct acq400_path_descriptor* pdesc = (struct acq400_path_descriptor*)(data);
	struct acq400_dev *adev = pdesc->dev;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct BQ* bq_in = &sc_dev->stream_dac.refills;
	struct BQ* bq_bk = &pdesc->bq;
	int rc = 0;
	int ab = 0;
	int id;

	dev_info(DEVP(adev), "streamdac_data_loop 01");
	go_rt(MAX_RT_PRIO-4);

	// @@todo .. don't dequeue in two's, dequeue in ones to feed DMAC in ones. Except init ..
	for (ab = 0; !kthread_should_stop(); ab = !ab){
		dev_dbg(DEVP(adev), "streamdac_data_loop() wait_event");
		if (wait_event_interruptible(
				sc_dev->stream_dac.sd_waitq,
				BQ_count(bq_in)>1 || kthread_should_stop())){
			dev_err(DEVP(adev), "streamdac_data_loop %d", __LINE__);
			rc = -EINTR;
			goto quit;
		}
		if (!kthread_should_stop()){
			id = BQ_get(DEVP(adev), bq_in);

			msleep(100);   /* @@todo pass a, b to DMAC */

			BQ_put(DEVP(adev), bq_bk, id);
			wake_up_interruptible(&pdesc->waitq);
		}
	}
quit:
	dev_info(DEVP(adev), "streamdac_data_loop 99");
	return rc;
}


/**
 * streamdac:
 * out: sc_dev->stream_dac.sd_bqw   : fresh data : belongs to driver
 * ret: pdesc->bq                   : return buffers : belongs to app
 *
 * app gets a full set on return buffers to begin with.
 */


#define TWO_INT	(2*sizeof(int))


ssize_t acq400_streamdac_write(struct file *file, const char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;

	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct BQ* refills = &sc_dev->stream_dac.refills;
	struct XO_dev* xo_dev = container_of(sc_dev->distributor_set[0], struct XO_dev, adev);
	unsigned id;
	int ii;
	unsigned cursor = 0;
	ssize_t rc;

	dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);

	if (count < TWO_INT){
		return -EINVAL;
	}

	while ((count-cursor) >= TWO_INT){
		if (wait_event_interruptible(
			pdesc->waitq,
			BQ_space(refills) > 2)){
			dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
			return -EINTR;
		}
		for (ii = 0; ii < 2; ++ii, cursor += sizeof(int)){
			rc = copy_from_user(&id, buf+cursor, sizeof(int));
			if (likely(rc == 0)){
				dev_dbg(DEVP(adev), "%s %d store id:%d", __FUNCTION__, __LINE__, id);
				BQ_put(DEVP(adev), refills, id);
				xo_dev->AO_playloop.push_buf = id;
			}else{
				dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
				return -rc;
			}
			wake_up_interruptible(&sc_dev->stream_dac.sd_waitq);
		}
	}
	dev_dbg(DEVP(adev), "%s %d rc:%d", __FUNCTION__, __LINE__, cursor);

	if (*f_pos == 0 && defer_stream_dac_task_init){
		sc_dev->stream_dac.sd_task = kthread_run(
			pdesc->minor == ACQ400_MINOR_STREAMDAC?
				streamdac_data_loop: streamdac_data_loop_dummy,
				pdesc, "%s.dac", adev->dev_name);
	}
	*f_pos += cursor;
	return cursor;
}


ssize_t acq400_streamdac_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct BQ* empties = &pdesc->bq;

	unsigned cursor = 0;



	dev_dbg(DEVP(adev), "%s %d available:%d count:%d", __FUNCTION__, __LINE__, BQ_count(empties), count);

	if (count < TWO_INT){
		dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	while((count-cursor) >=  TWO_INT){
		int ii;
		dev_dbg(DEVP(adev), "wait_event_interruptible(%p) cursor:%d count:%d",
				&pdesc->waitq, cursor, count);

		if (cursor > 0 && BQ_count(empties) < 2){
			break;
		}else if (wait_event_interruptible(
			pdesc->waitq, BQ_count(empties) >= 2)){
			dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
			return -EINTR;
		}

		dev_dbg(DEVP(adev), "%s %d %d 0x%08x", __FUNCTION__, __LINE__, empties->tail, empties->buf[empties->tail]);

		for (ii = 0; ii < 2; ++ii, cursor += sizeof(int)){
			unsigned tmp = BQ_get(DEVP(adev), empties);
			int rc = copy_to_user(buf+cursor, &tmp, sizeof(int));

			if (rc){
				dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
				return -rc;
			}
		}
	}
	dev_dbg(DEVP(adev), "%s %d cursor:%d", __FUNCTION__, __LINE__, cursor);
	return cursor;
}


int acq400_streamdac_open(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct XO_dev* xo_dev = container_of(sc_dev->distributor_set[0], struct XO_dev, adev);
	struct BQ* empties = &pdesc->bq;
	struct BQ* refills = &sc_dev->stream_dac.refills;
	struct BQ_Wrapper* bqw = &sc_dev->stream_dac.sd_bqw;
	int ib = 0;		// @@todo probably not zero ..
	int limit = min(lastDistributorBuffer(), firstDistributorBuffer()+AWG_BACKLOG-1);

	dev_dbg(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);

	if (!list_empty(&bqw->bq_clients)){
		dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
		return -EBUSY;
	}

	bqw_init(pdesc, bqw, AWG_BACKLOG);
	for (ib = firstDistributorBuffer(); ib < limit; ++ib){
		BQ_put(DEVP(adev), empties, ib);

		dev_dbg(DEVP(adev), "%s %d ib:%d bq:%d", __FUNCTION__, __LINE__,
					ib, CIRC_CNT(empties->head, empties->tail, empties->bq_len));
	}
	xo_dev->AO_playloop.push_buf =
	xo_dev->AO_playloop.pull_buf =
	xo_dev->AO_playloop.first_buf = firstDistributorBuffer();
	xo_dev->AO_playloop.last_buf = limit;
	BQ_clear(refills);

	dev_dbg(DEVP(adev), "%s %d available:%d", __FUNCTION__, __LINE__, BQ_count(empties));

	if (!defer_stream_dac_task_init){
		sc_dev->stream_dac.sd_task = kthread_run(
			pdesc->minor == ACQ400_MINOR_STREAMDAC?
				streamdac_data_loop: streamdac_data_loop_dummy,
				pdesc, "%s.dac", adev->dev_name);
	}

	dev_dbg(DEVP(adev), "%s %d bq:%d task:%p", __FUNCTION__, __LINE__,
			CIRC_CNT(empties->head, empties->tail, empties->bq_len), sc_dev->stream_dac.sd_task);
	return 0;
}




int acq400_streamdac_release(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int rc;

	if (!WORKER_DONE(pdesc) && sc_dev->stream_dac.sd_task){
		dev_dbg(DEVP(adev), "acq400_streamdac_release() %p", sc_dev->stream_dac.sd_task);
		kthread_stop(sc_dev->stream_dac.sd_task);
		wake_up_interruptible(&sc_dev->stream_dac.sd_waitq);
	}

	dev_dbg(DEVP(adev), "acq400_streamdac_release() wait WORKER_DONE");
	if (wait_event_interruptible(pdesc->waitq, WORKER_DONE(pdesc)) > 0){
		dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
	}
	sc_dev->stream_dac.sd_task = 0;

	dev_dbg(DEVP(adev), "acq400_streamdac_release() WORKER_DONE");

	if ((rc = bqw_release(pdesc, &sc_dev->stream_dac.sd_bqw)) != 0){
		return rc;
	}else{
		return acq400_release(inode, file);
	}
}



int acq400_open_streamdac(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_streamdac = {
			.open = acq400_streamdac_open,
			.read = acq400_streamdac_read,
			.poll = acq400_bq_poll,
			.write = acq400_streamdac_write,
			.release = acq400_streamdac_release,
	};
	file->f_op = &acq400_fops_streamdac;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}



extern char awg_seg[];

ssize_t acq400_awg_abcde_read(struct file *file, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct AWG_ABCDE* abcde = &xo_dev->awg_abcde;

	if (wait_event_interruptible(abcde->ret_waitq, buf_count(&xo_dev->awg_abcde.ret_queue, AWG_ABCDE_LEN))){
		dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
		return -EINTR;
	}else{
		char bx = buf_get(&xo_dev->awg_abcde.ret_queue, AWG_ABCDE_LEN);
		int rc = copy_to_user(buf, &bx, sizeof(char));

		if (rc){
			dev_err(DEVP(adev), "%s %d", __FUNCTION__, __LINE__);
			return -rc;
		}
	}

	*f_pos += 1;
	return 1;
}



ssize_t acq400_awg_abcde_write(struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	struct AWG_ABCDE* abcde = &xo_dev->awg_abcde;
	int cursor = 0;
	int rc = 0;
	u8 lbuf;

	while(count > 0){
		int headroom = buf_space(&abcde->new_queue, AWG_ABCDE_LEN);
		int c0 = cursor;
		if (count < headroom){
			headroom = count;
		}
		for ( ; cursor < c0+headroom; ++cursor){
			rc = copy_from_user(&lbuf, buf+cursor, sizeof(char));
			if (VALID_ABCDE(lbuf)){
				dev_dbg(DEVP(adev), "acq400_awg_abcde_write count:%d cursor:%d cc:%c", count, cursor, lbuf);
				if (likely(rc == 0)){
					buf_put(&abcde->new_queue, lbuf, AWG_ABCDE_LEN);
				}else{
					dev_err(DEVP(adev), "%s %d rc:%d", __FUNCTION__, __LINE__, rc);
					return -rc;
				}
			}else{
				dev_dbg(DEVP(adev), "%s rejected d:%d c:%c", __FUNCTION__, lbuf, lbuf);
			}
		}
		count -= headroom;
		if (count){
			if (wait_event_interruptible(abcde->new_waitq, buf_space(&abcde->new_queue, AWG_ABCDE_LEN)) != 0){
				dev_err(DEVP(adev), "%s %d rc:%d", __FUNCTION__, __LINE__, rc);
				init_cb_empty(&xo_dev->awg_abcde.new_queue, AWG_ABCDE_LEN);
				return -EINTR;
			}
		}
	}

	if (rc > 0){
		*f_pos += cursor;
	}
	return cursor;
}

int acq400_awg_abcde_release(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);
	xo_dev->awg_abcde.pid = 0;
	init_cb_empty(&xo_dev->awg_abcde.ret_queue, AWG_ABCDE_LEN);
	return acq400_release(inode, file);
}
int acq400_awg_abcde_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_awg_abcde = {
			.read = acq400_awg_abcde_read,
			.write = acq400_awg_abcde_write,
			.release = acq400_awg_abcde_release,
	};
	struct acq400_path_descriptor* pdesc = PD(file);

	if (pdesc->pid != task_pid_nr(current)){
		return -EBUSY;
	}else{
		struct acq400_dev* adev = pdesc->dev;
		struct XO_dev* xo_dev = container_of(adev, struct XO_dev, adev);

		xo_dev->awg_abcde.pid = PD(file)->pid;
		file->f_op = &acq400_fops_awg_abcde;
		if (file->f_op->open){
			return file->f_op->open(inode, file);
		}else{
			return 0;
		}
	}
}

int acq400_open_ui(struct inode *inode, struct file *file)
{
        struct acq400_dev *adev;
        int minor;
        int rc;

        adev = container_of(inode->i_cdev, struct acq400_dev, cdev);
        minor = MINOR(inode->i_rdev);

        dev_dbg(DEVP(adev), "hello: minor:%d\n", minor);

        if (minor >= ACQ400_MINOR_BUF && minor <= ACQ400_MINOR_BUF2){
        	rc = acq400_open_hb(inode, file);
        } else if (minor >= ACQ420_MINOR_CHAN && minor <= ACQ420_MINOR_CHAN2){
        	rc = -ENODEV;  	// @@todo maybe later0
        } else {
        	switch(minor){
        	case ACQ420_MINOR_HISTO:
        		rc = acq400_open_histo(inode, file);
        		break;
        	case ACQ400_MINOR_HB0:
        		rc = acq420_open_hb0(inode, file);
        		break;
        	case ACQ400_MINOR_EVENT:
        		rc = acq400_open_event(inode, file);
        		break;
        	case ACQ400_MINOR_STREAMDAC:
        	case ACQ400_MINOR_STREAMDAC_DUMMY:
        		rc = acq400_open_streamdac(inode, file);
        		break;
        	case ACQ400_MINOR_BQ_NOWAIT:
        		rc = acq400_bq_open(inode, file, BQ_MIN_BACKLOG);
        		break;
        	case ACQ400_MINOR_BQ_FULL:
        		rc = acq400_bq_open(inode, file, BQ_MAX_BACKLOG);
        		break;
        	case ACQ400_MINOR_GPGMEM:
        	case ACQ400_MINOR_GPGMEM32:
        		rc = acq420_open_gpgmem(inode, file);
        		break;
        	case ACQ400_MINOR_BOLO_AWG:
        		rc = bolo_open_awg(inode, file);
        		break;
        	case AO420_MINOR_HB0_AWG_ONCE:
        	case AO420_MINOR_HB0_AWG_LOOP:
        	case AO420_MINOR_HB0_AWG_ONCE_RETRIG:
        		rc = xo400_open_awg(inode, file);
        		break;
        	case ACQ400_MINOR_RESERVE_BLOCKS:
        		rc = acq420_reserve_open(inode, file);
        		break;
        	case ACQ400_MINOR_RSV_DIST:
        		rc = acq420_reserve_dist_open(inode, file);
        		break;
        	case ACQ400_MINOR_SEW1_FIFO:
        	case ACQ400_MINOR_SEW2_FIFO:
        		rc = acq420_sew_fifo_open(inode, file);
        		break;
        	case ACQ400_MINOR_ATD:
        		rc = acq400_atd_open(inode, file);
        		break;
        	case ACQ400_MINOR_AXI_DMA_ONCE:
        		rc = acq400_axi_dma_once_open(inode, file);
        		break;
        	case ACQ400_MINOR_WR_TS:
        	case ACQ400_MINOR_WR_PPS:
        	case ACQ400_MINOR_WR_CUR:
        	case ACQ400_MINOR_WR_CUR_TAI:
        	case ACQ400_MINOR_WR_CUR_TRG0:
        	case ACQ400_MINOR_WR_CUR_TRG1:
        	case ACQ400_MINOR_WRTT:
        	case ACQ400_MINOR_WRTT1:
        	case ACQ400_MINOR_ADC_SAMPLE_COUNT:           // NOT WR, but similar function
        		rc = acq400_wr_open(inode, file);
        		break;
        	case ACQ400_MINOR_AOFIFO:
        		rc = acq400_minor_aofifo_open(inode, file);
        		break;
        	case ACQ400_MINOR_ADC_NACC_SUBRATE:
        		rc = acq400_nacc_subrate_open(inode, file);
        		break;
        	case ACQ400_MINOR_AWG_ABCDE:
        		rc = acq400_awg_abcde_open(inode, file);
        		break;
            	default:
        		if (minor >= ACQ400_MINOR_MAP_PAGE &&
        		    minor < ACQ400_MINOR_MAP_PAGE+16  ){
        			rc = acq400_map_page_open(inode, file);
        		}else if (minor >= ACQ420_MINOR_TIGA_TS_1 &&
        			  minor < ACQ420_MINOR_TIGA_99){
        			rc = acq400_tiga_open(inode, file);
        		}else{
        			rc = -ENODEV;
        		}
        	}
        }

        if (rc != 0){
        	dev_err(DEVP(adev), "acq400_open_ui FAIL minor:%d rc:%d", minor, rc);
        	if (PD(file)) kfree(PD(file));
        	SETPD(file, 0);
        }
        return rc;
}

EXPORT_SYMBOL_GPL(acq400_init_event_info);


