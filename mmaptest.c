/* ------------------------------------------------------------------------- *
 * mmaptest.c  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 31 Mar 2014  
 *    Author: pgm                                                         
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "popt.h"

#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

char *fname = "/dev/mem";
unsigned offset = 0x40000000;
unsigned first = 0x80;
unsigned last = 0x9c;
unsigned length = 0x1000; /* one page, min */
int verbose;

unsigned *src;
unsigned *tgt;

int fd;
void *region;

void ui(int argc, const char *argv[])
{
	struct poptOption opt_table[] = {
		{ "device", 'f', POPT_ARG_STRING, &fname, 0 },
		{ "offset", 'o', POPT_ARG_INT, &offset, 'o' },
		{ "first", '1', POPT_ARG_INT, &first, 0 },
		{ "last", '2', POPT_ARG_INT, &last, 0 },
		{ "verbose", 'v', POPT_ARG_INT, &verbose, 0 },
		POPT_AUTOHELP POPT_TABLEEND
	};

	poptContext opt_context =
		poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;

	while ((rc = poptGetNextOpt(opt_context)) > 0) {
		;
	}

	if ((fd = open(fname, O_RDWR)) < 0) {
		perror(fname);
		exit(1);
	}
	region =
		mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (region == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	tgt = (unsigned *)((char *)region + first);
	src = calloc(last + sizeof(unsigned) - first, sizeof(unsigned));
}

#define ID 0xff
#define ID_SH 8
#define CARRY (1 << 31)

unsigned rot1(unsigned xx)
{
	unsigned id = xx & ID;
	int carry = xx & CARRY;
	xx &= ~ID;
	xx <<= 1;
	if (carry) {
		xx |= 1 << ID_SH;
	}
	xx |= id;
	return xx;
}

#define NWORDS ((last - first) / sizeof(unsigned) + 1)
void rot()
{
	int ii;
	for (ii = 0; ii < NWORDS; ++ii) {
		src[ii] = rot1(src[ii]);
	}
}
void init()
{
	int ii;
	for (ii = 0; ii < NWORDS; ++ii) {
		src[ii] = (ii << ID_SH) | (ii + '0');
		tgt[ii] = src[ii];
	}
}
void write_copy(int rix)
{
	src[rix] = rot1(src[rix]);
	tgt[rix] = src[rix];
}

void dump(const char *label, unsigned *stuff)
{
	int ii;
	printf("%s ", label);
	for (ii = 0; ii < NWORDS; ++ii) {
		printf("%08x ", stuff[ii]);
	}
	printf("\n");
}
int compare()
{
	int ii;
	int has_errors = 0;
	for (ii = 0; ii < NWORDS; ++ii) {
		if (verbose || src[ii] != tgt[ii]) {
			dump(">>", src);
			dump(src[ii] != tgt[ii] ? "EE" : "<<", tgt);
			has_errors = 1;
			break;
		}
	}
	return has_errors;
}
int mtest()
{
	unsigned prbs[4][8] = {
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
		{ 7, 6, 5, 4, 3, 2, 1, 0 },
		{ 4, 3, 2, 5, 6, 7, 0, 1 },
		{ 0, 1, 6, 7, 4, 5, 2, 3 },
	};
	int ii = 0;
	int jj;
	int has_errors = 0;
	init();
	for (;; ii++) {
		for (jj = 0; jj < 8; ++jj) {
			write_copy(prbs[ii & 3][jj]);
			if (compare()) {
				has_errors = 1;
			}
		}
	}
	return has_errors;
}

int main(int argc, const char *argv[])
{
	ui(argc, argv);
	return mtest();
}
