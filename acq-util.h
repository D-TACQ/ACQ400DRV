/* ------------------------------------------------------------------------- */
/* acq-util.h -                                                              */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2003 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
 
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

#ifndef __ACQ_UTIL_H__
#define __ACQ_UTIL_H__

#if defined __cplusplus
extern "C" {
#endif

int acqMakeChannelRange(int channels[], int maxchan, const char* definition);
/** 
 *  @definition "-N  N-M  N,M,P  N-"
 *  returns number of selected channels
 */

	

int strsplit(char *str, char *argv[], int maxargs, const char* delim);
/** strsplit() splits str into args, returns #args. */

void goRealTime(int sched_fifo_priority);

char *getRoot(int devnum);
int getBufferId(int fc);



#if defined __cplusplus
};
int getenv_default(const char* key, int def = 0);
#endif

#endif /* __ACQ_UTIL_H__ */

