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

/** @file debugfs2.h DESCR
 * 
 *  Created on: Feb 10, 2012
 *      Author: pgm
 */

#ifndef DEBUGFS2_H_
#define DEBUGFS2_H_

/*
 * on read: read data from pread
 * on write: write data to pwrite
 * on commit: copy data from pwrite to pread
 */
struct DebugFs2NodeInfo {
	u32 *pwrite;
	u32 *pread;
	u32 *pcache;
	u32 mask;
	u32 mode;
	const char *read_fmt;
};
#define DBGFS2_NI_SZ sizeof(struct DebugFs2NodeInfo)

#define DFS2_WRITEONLY 0x1
#define DFS2_CACHE 0x2

/*
 * scenarios:


rw direct: pwrite == pread = reg; pcache=0 *pwrite |= wval
 w direct: pwrite = reg; pwrite != pread; pcache=0 *pwrite = *pread |= wval
rw cache:  pwrite != pread; pread = pcache == reg;
	on read:
		rval = *pread;
	on write:
		*pwrite |= wval
	on commit:
		*pcache = *pwrite;
 w cache: pwrite == pread; pcache == reg;


*/

/* create using ascii definition

offset mask mode

mode:: rwc

*/

struct dentry *debugfs2_create_file_def(struct dentry *parent,
					struct DebugFs2NodeInfo *nodeInfo,
					const char *def, int nline);
/* create node from definition string. nodeInfo should be init with base ptr */
/* def string:
name offset mask mode. if mask does not begin 0x, it's a bit position

RATE_AF		0x0000 0x00003fff rw
FAULT_RATE_AF   0x00d0 17 rw

*/

ssize_t debugfs2_write_line(struct file *file, const char __user *user_buf,
			    size_t count, loff_t *ppos, char *myline);

#endif /* DEBUGFS2_H_ */
