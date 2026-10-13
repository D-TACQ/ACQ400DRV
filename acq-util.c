/* ------------------------------------------------------------------------- */
/* acq-util.c -                                                              */
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

#include <string.h>
#include <stdlib.h>

#include "acq-util.h"

static void _addRange(int channels[], int cmax, int ll, int rr)
{
	int ii;

	if (rr > cmax)
		rr = cmax;
	if (ll < 1)
		ll = 1;

	for (ii = ll; ii <= rr; ++ii) {
		channels[ii] = 1;
	}
}

static void addRange(int channels[], int cmax, char *range)
{
	char *ranger;

	if ((ranger = index(range, '-')) != 0 ||
	    (ranger = index(range, ':')) != 0) {
		int len = strlen(range);
		*ranger = '\0';

		if (ranger == range) {
			if (len == 1) {
				_addRange(channels, cmax, 1, cmax);
			} else {
				_addRange(channels, cmax, 1, atoi(range + 1));
			}
		} else if (ranger == range + len - 1) {
			_addRange(channels, cmax, atoi(range), cmax);
		} else {
			_addRange(channels, cmax, atoi(range),
				  atoi(ranger + 1));
		}
	} else {
		_addRange(channels, cmax, atoi(range), atoi(range));
	}
}

int acqMakeChannelRange(int channels[], int cmax, const char *definition)
/** 
 *  @definition "-N  N-M  N,M,P  N-"
 *  returns number of selected channels
 */
{
	char *dbuf = (char *)calloc(strlen(definition) + 1, 1);
	char *tbuf = (char *)calloc(strlen(definition) + 1, 1);
	char *rbuf = tbuf;
	char *ptok;

	strcpy(dbuf, definition);

	for (ptok = dbuf; (ptok = strtok_r(ptok, " ,", &rbuf)) != 0; ptok = 0) {
		addRange(channels, cmax, ptok);
	}

	int ichan;
	int selected = 0;

	for (ichan = 1; ichan <= cmax; ++ichan) {
		if (channels[ichan] != 0) {
			selected++;
		}
	}

	free(tbuf);
	free(dbuf);

	return selected;
}

int strsplit(char *str, char *argv[], int maxargs, const char *delim)
/** strsplit() splits str into args, returns #args. */
{
	int iarg;
	char *s1, *s2;

	for (iarg = 0, s1 = str; iarg < maxargs && (s2 = strstr(s1, delim));) {
		*s2 = '\0';
		argv[iarg++] = s1;
		s1 = s2 + strlen(delim);
	}
	if (s1 != 0) {
		argv[iarg++] = s1;
	}
	return iarg;
}
