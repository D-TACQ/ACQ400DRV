/* ------------------------------------------------------------------------- */
/* acq400_ui.h  D-TACQ ACQ400 FMC  DRIVER                                   
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

#ifndef ACQ400_UI_H_
#define ACQ400_UI_H_

extern ssize_t acq400_read(struct file *file, char __user *buf, size_t count,
			   loff_t *f_pos);

extern ssize_t acq400_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *f_pos);
extern int acq400_mmap_bar(struct file *file, struct vm_area_struct *vma);
extern int acq400_release(struct inode *inode, struct file *file);

extern int acq400_open_ui(struct inode *inode, struct file *file);
#endif /* ACQ400_UI_H_ */
