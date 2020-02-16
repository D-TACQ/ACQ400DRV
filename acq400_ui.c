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

int hz = HZ;
module_param(hz, int, 0444);
MODULE_PARM_DESC(hz, "show kernel value for HZ");

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
	struct HBM *hb = adev->hb[ibuf];
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = hb->len;
	unsigned pfn = hb->pa >> PAGE_SHIFT;

	if (!IS_BUFFER(PD(file)->minor)){
		dev_warn(DEVP(adev), "ERROR: device node not a buffer");
		return -1;
	}
	dev_dbg(&adev->pdev->dev, "%c [%d] 0x%08x vsize %lu psize %lu %s",
		'D', hb->ix,hb->pa,
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
	struct HBM *hb = adev->hb[ibuf];
	unsigned cursor = *f_pos;	/* f_pos counts in bytes */
	int rc;

	dev_dbg(DEVP(adev), "acq400_hb_read() cursor:%u len:%u count:%u",
			cursor, hb->len, count);
	if (cursor >= hb->len){
		return 0;
	}else{
		int headroom = hb->len - cursor;
		if (count > headroom){
			count = headroom;
		}
	}

	dev_dbg(DEVP(adev), "acq400_hb_read() copy_to [%d] %p %d",
			ibuf, (char*)hb->va+cursor, count);
	rc = copy_to_user(buf, (char*)hb->va + cursor, count);
	if (rc){
		return -1;
	}

	*f_pos += count;

	dev_dbg(DEVP(adev), "acq400_hb_read() return count:%u", count);
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


ssize_t acq400_hb0_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	char HB0[16];

	int rc;
	/* force wait until next .. this is very conservative, we could
	 * stash the hb0 in DESCR for use between calls.
	 * But this way is self-regulating. This is for human monitor,
	 * not an attempt to handle ALL the data
	 */
	unsigned hb0_count = adev->rt.hb0_count;

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
	file->f_op = &acq400_fops_hb0;
	if (file->f_op->open){
		return file->f_op->open(inode, file);
	}else{
		return 0;
	}
}



int acq400_gpgmem_open(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	if (file->f_flags & O_WRONLY) {
		int iw;
		for (iw = 0; iw < sc_dev->gpg_cursor; ++iw){
			iowrite32(0, sc_dev->gpg_base+iw);
		}
		sc_dev->gpg_cursor = 0;
	}
	return 0;
}
ssize_t acq400_gpgmem_read(
	struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int len = sc_dev->gpg_cursor*sizeof(u32);
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
	rc = copy_to_user(buf, sc_dev->gpg_buffer+bcursor, count);
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
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
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
	rc = copy_from_user(sc_dev->gpg_buffer+bcursor, buf, count);
	if (rc){
		return -1;
	}
	*f_pos += count;
	sc_dev->gpg_cursor += count/sizeof(u32);
	return count;

}

int set_gpg_top(struct acq400_dev* adev, u32 gpg_count)
{
	if (gpg_count >= 2){
		u32 gpg_ctrl = acq400rd32(adev, GPG_CTRL);
		u32 gpg_top = gpg_count - 1		// was count, not address
					 -1;		// GPG_2ND_LAST_ADDR
		gpg_top <<= GPG_CTRL_TOPADDR_SHL;
		gpg_top &= GPG_CTRL_TOPADDR;
		gpg_ctrl &= ~GPG_CTRL_TOPADDR;
		gpg_ctrl |= gpg_top;
		acq400wr32(adev, GPG_CTRL, gpg_ctrl);
		return 0;
	}else{
		dev_err(DEVP(adev), "set_gpg_top() ERROR: must have 2 or more entries");
		return -1;
	}
}

int acq400_gpgmem_release(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	unsigned* src = (unsigned *)sc_dev->gpg_buffer;
	int iw;
	int rc;

	for (iw = 0; iw < sc_dev->gpg_cursor; ++iw){
		iowrite32(src[iw], sc_dev->gpg_base+iw);
	}
	dev_dbg(DEVP(adev), "acq400_gpgmem_release() %d\n", iw);
	rc = set_gpg_top(adev, sc_dev->gpg_cursor);
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
	int ix = PD(file)->minor - ACQ420_MINOR_SEW1_FIFO;

	int rc = acq400_sew_fifo_write_bytes(adev, ix, buf, count);

	if (rc > 0){
		*f_pos += rc;
	}
	return rc;
}
int acq420_sew_fifo_release(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	int ix = PD(file)->minor - ACQ420_MINOR_SEW1_FIFO;
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
	int ix = PD(file)->minor - ACQ420_MINOR_SEW1_FIFO;
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

void init_event_info(struct EventInfo *eventInfo)
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

	if (mutex_lock_interruptible(&adev->bq_clients_mutex)) {
		return -ERESTARTSYS;
	}else{
		++adev->event_client_count;
		mutex_unlock(&adev->bq_clients_mutex);
	}
	int_csr = x400_get_interrupt(adev);
	x400_set_interrupt(adev, int_csr|ADC_INT_CSR_COS_EN_ALL);

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
	char lbuf[40];
	int nbytes;
	int rc;
	struct EventInfo eventInfo = PD(file)->eventInfo;
	struct acq400_dev* adev0 = acq400_lookupSite(0);
	u32 old_sample = adev->rt.samples_at_event;
	int timeout = 0;
	int event_source = 0;

	if (eventInfo.pollin){
		PD(file)->eventInfo.pollin = 0;
	}

	dev_dbg(DEVP(adev), "acq400_event_read() 01 pollin %d old_sample %d",
			eventInfo.pollin, old_sample);

	if (eventInfo.pollin == 0){
		/* force caller to wait fresh event. This is an auto-rate-limit
		 * it's also re-entrant (supports multiple clients each at own rate)
		 * NB: NO ATTEMPT to guarantee that all events processed. caveat emptor
		 */
		dev_dbg(DEVP(adev), "acq400_event_read() 10 wait event");

		rc = wait_event_interruptible_timeout(
				adev->event_waitq,
				adev->rt.samples_at_event != old_sample,
				event_to);
		if (rc < 0){
			return -EINTR;
		}else if (rc == 0){
			return -EAGAIN;
		}
		init_event_info(&eventInfo);
	}

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
			xtd_dev->atd.event_source = 0;
		}
	}

	nbytes = snprintf(lbuf, sizeof(lbuf), "%u %d %d %s 0x%08x %d\n",
			adev->rt.samples_at_event,
			eventInfo.hbm0? eventInfo.hbm0->ix: -1,
			eventInfo.hbm1? eventInfo.hbm1->ix: -1, timeout? "TO": "OK",
			event_source,
			adev->rt.event_count);


	rc = copy_to_user(buf, lbuf, nbytes);
	if (rc != 0){
		rc = -1;
	}else{
		rc = nbytes;
	}
	dev_dbg(DEVP(adev), "acq400_event_read() 99 \"%s\" rc %d", lbuf, rc);
	return rc;
}


static unsigned int acq400_event_poll(
	struct file *file, struct poll_table_struct *poll_table)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	unsigned rc;

	if (adev->rt.samples_at_event){
		init_event_info(&PD(file)->eventInfo);
		rc = POLLIN;
	}else{
		poll_wait(file, &adev->event_waitq, poll_table);
		if (adev->rt.samples_at_event){
			init_event_info(&PD(file)->eventInfo);
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

	if (mutex_lock_interruptible(&adev->bq_clients_mutex)) {
		return -ERESTARTSYS;
	}else{
		if (--adev->event_client_count == 0){
			u32 int_csr = x400_get_interrupt(adev);

			int_csr &= ~(ADC_INT_CSR_EVENT1_EN|ADC_INT_CSR_EVENT0_EN);
			x400_set_interrupt(adev, int_csr);
		}
		mutex_unlock(&adev->bq_clients_mutex);
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
	int rc;

	dev_dbg(DEVP(adev), "wait_event_interruptible(%p)", &pdesc->waitq);
	if (wait_event_interruptible(
		pdesc->waitq,
		CIRC_CNT(bq->head, bq->tail, bq->bq_len))){
		return -EINTR;
	}

	bc = snprintf(lbuf, 32, "%03d\n", bq->buf[bq->tail]);
	smp_store_release(&bq->tail, (bq->tail+1)&(bq->bq_len-1));

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

	if (CIRC_SPACE(bq->head, bq->tail, bq->bq_len)){
		return POLLIN|POLLRDNORM;
	}else{
		poll_wait(file, &pdesc->waitq, poll_table);
		if (CIRC_SPACE(bq->head, bq->tail, bq->bq_len)){
			return POLLIN|POLLRDNORM;
		}else{
			return 0;
		}
	}
}

int acq400_bq_release(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;

	struct acq400_path_descriptor *cur;
	int nelems = 0;


        if (mutex_lock_interruptible(&adev->bq_clients_mutex)) {
	       return -ERESTARTSYS;
	}
        list_del_init(&pdesc->bq_list);
        /* diagnostic */
        list_for_each_entry(cur, &adev->bq_clients, bq_list){
               	++nelems;
        }
        mutex_unlock(&adev->bq_clients_mutex);

        if (nelems){
        	dev_dbg(DEVP(adev), "nelems:%d", nelems);
        }else{
        	dev_dbg(DEVP(adev), "nelems:%d", nelems);
        }

        kfree(pdesc->bq.buf);
        return acq400_release(inode, file);
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

	struct acq400_path_descriptor *cur;
	int nelems = 0;

        file->f_op = &acq400_fops_bq;

        INIT_LIST_HEAD(&pdesc->bq_list);
        pdesc->bq.bq_len = backlog;
        pdesc->bq.buf = kzalloc(pdesc->bq.bq_len*sizeof(unsigned), GFP_KERNEL);

        if (mutex_lock_interruptible(&adev->bq_clients_mutex)) {
	       return -ERESTARTSYS;
	}
        list_add_tail(&pdesc->bq_list, &adev->bq_clients);
        list_for_each_entry(cur, &adev->bq_clients, bq_list){
        	++nelems;
        }
        mutex_unlock(&adev->bq_clients_mutex);
        if (nelems > 1){
        	dev_dbg(DEVP(adev), "nelems:%d", nelems);
        }else{
        	dev_dbg(DEVP(adev), "nelems:%d", nelems);
        }
	return 0;
}




int acq400_open_ui(struct inode *inode, struct file *file)
{
        struct acq400_dev *adev;
        int minor;
        int rc;

        adev = container_of(inode->i_cdev, struct acq400_dev, cdev);
        minor = MINOR(inode->i_rdev);

        dev_dbg(DEVP(adev), "hello: minor:%d\n", minor);

        if (minor >= ACQ420_MINOR_BUF && minor <= ACQ420_MINOR_BUF2){
        	rc = acq400_open_hb(inode, file);
        } else if (minor >= ACQ420_MINOR_CHAN && minor <= ACQ420_MINOR_CHAN2){
        	rc = -ENODEV;  	// @@todo maybe later0
        } else {
        	switch(minor){
        	case ACQ420_MINOR_HISTO:
        		rc = acq400_open_histo(inode, file);
        		break;
        	case ACQ420_MINOR_HB0:
        		rc = acq420_open_hb0(inode, file);
        		break;
        	case ACQ420_MINOR_EVENT:
        		rc = acq400_open_event(inode, file);
        		break;
        	case ACQ400_MINOR_BQ_NOWAIT:
        		rc = acq400_bq_open(inode, file, BQ_MIN_BACKLOG);
        		break;
        	case ACQ400_MINOR_BQ_FULL:
        		rc = acq400_bq_open(inode, file, BQ_MAX_BACKLOG);
        		break;
        	case ACQ420_MINOR_GPGMEM:
        		rc = acq420_open_gpgmem(inode, file);
        		break;
        	case ACQ420_MINOR_BOLO_AWG:
        		rc = bolo_open_awg(inode, file);
        		break;
        	case AO420_MINOR_HB0_AWG_ONCE:
        	case AO420_MINOR_HB0_AWG_LOOP:
        	case AO420_MINOR_HB0_AWG_ONCE_RETRIG:
        		rc = xo400_open_awg(inode, file);
        		break;
        	case ACQ420_MINOR_RESERVE_BLOCKS:
        		rc = acq420_reserve_open(inode, file);
        		break;
        	case ACQ400_MINOR_RSV_DIST:
        		rc = acq420_reserve_dist_open(inode, file);
        		break;
        	case ACQ420_MINOR_SEW1_FIFO:
        	case ACQ420_MINOR_SEW2_FIFO:
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
        	case ACQ400_MINOR_WR_CUR_TRG:
        	case ACQ400_MINOR_WRTT:
        		rc = acq400_wr_open(inode, file);
        		break;
            	default:
        		if (minor >= ACQ400_MINOR_MAP_PAGE &&
        		    minor < ACQ400_MINOR_MAP_PAGE+16  ){
        			rc = acq400_map_page_open(inode, file);
        		}else{
        			rc = -ENODEV;
        		}
        	}
        }

        if (rc != 0){
        	dev_err(DEVP(adev), "acq400_open FAIL minor:%d rc:%d", minor, rc);
        	if (PD(file)) kfree(PD(file));
        	SETPD(file, 0);
        }
        return rc;
}




