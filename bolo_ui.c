/* bolo_ui.c  D-TACQ ACQ400 FMC  DRIVER                                   
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

void bolo_awg_commit(struct acq400_dev *adev)
{
	struct acq400_bolo_dev *b8_dev =
		container_of(adev, struct acq400_bolo_dev, adev);
	int nwords = b8_dev->bolo8.awg_buffer_cursor / sizeof(u32);
	u32 *src = (u32 *)b8_dev->bolo8.awg_buffer;
	int ii;
	for (ii = 0; ii < nwords; ++ii) {
		acq400wr32(adev, B8_AWG_MEM + sizeof(u32) * ii, src[ii]);
	}
	/* wavetop in shorts, starting from zero */
	acq400wr32(adev, B8_DAC_WAVE_TOP,
		   b8_dev->bolo8.awg_buffer_cursor / sizeof(u16) - 1);
	acq400wr32(adev, B8_DAC_CON,
		   acq400rd32(adev, B8_DAC_CON) | B8_DAC_CON_ENA);
}
int bolo_awg_open(struct inode *inode, struct file *file)
/* if write mode, reset length */
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	struct acq400_bolo_dev *b8_dev =
		container_of(adev, struct acq400_bolo_dev, adev);
	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		b8_dev->bolo8.awg_buffer_cursor = 0;
	}
	return 0;
}
int bolo_awg_release(struct inode *inode, struct file *file)
/* if it was a write, commit to memory and set length */
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		bolo_awg_commit(adev);
	}
	return 0;
}

ssize_t bolo_awg_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	struct acq400_bolo_dev *b8_dev =
		container_of(adev, struct acq400_bolo_dev, adev);
	int len = b8_dev->bolo8.awg_buffer_cursor;
	unsigned bcursor = *f_pos; /* f_pos counts in bytes */
	int rc;

	if (bcursor >= len) {
		return 0;
	} else {
		int headroom = (len - bcursor);
		if (count > headroom) {
			count = headroom;
		}
	}
	rc = copy_to_user(buf, b8_dev->bolo8.awg_buffer + bcursor, count);
	if (rc) {
		return -1;
	}

	*f_pos += count;
	return count;
}

ssize_t bolo_awg_write(struct file *file, const char __user *buf, size_t count,
		       loff_t *f_pos)
{
	struct acq400_dev *adev = ACQ400_DEV(file);
	struct acq400_bolo_dev *b8_dev =
		container_of(adev, struct acq400_bolo_dev, adev);
	int len = b8_dev->bolo8.awg_buffer_max;
	unsigned bcursor = b8_dev->bolo8.awg_buffer_cursor;
	int rc;

	if (bcursor >= len) {
		return 0;
	} else {
		int headroom = (len - bcursor);
		if (count > headroom) {
			count = headroom;
		}
	}
	rc = copy_from_user(b8_dev->bolo8.awg_buffer + bcursor, buf, count);
	if (rc) {
		return -1;
	}
	*f_pos += count;
	b8_dev->bolo8.awg_buffer_cursor += count;
	return count;
}

int bolo_open_awg(struct inode *inode, struct file *file)
{
	static struct file_operations fops = {
		.open = bolo_awg_open,
		.write = bolo_awg_write,
		.read = bolo_awg_read,
		.release = bolo_awg_release,
	};
	file->f_op = &fops;
	if (file->f_op->open) {
		return file->f_op->open(inode, file);
	} else {
		return 0;
	}
}
