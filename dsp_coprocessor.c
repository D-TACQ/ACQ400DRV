/* ------------------------------------------------------------------------- */
/* dsp_coprocessor.c  D-TACQ ACQ400 FMC  test app	                     */
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

/*
 * dsp-coprocessor SRCFILE DSTFILE
 *
 * Fill SRC buf from SRCFILE
 * run a process
 * Write DST buf to DSTFILE
 *
 * arm-xilinx-linux-gnueabi-gcc -o dsp_coprocessor dsp_coprocessor.c -lrt -lm
 */

#define BUG_MAXCOUNT 1 /* BUG: count max at 7fff, not 8000 */
#define BUG_STOPSHORT 1 /* BUG: stops ons sample short	    */

#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define SRC_OFFSET 0x80000000U
#define SRC_LEN 0x00020000U
#define DST_OFFSET 0x80040000U
#define DST_LEN 0x00020000U
#define REGS_OFFSET 0x80020000U
#define REGS_LEN 0x1000U

#define BYTE_COUNT SRC_LEN
#define WORD_COUNT (SRC_LEN / sizeof(unsigned))

#define CTRL 1
#define STAT 2

#define MAXPOLL 1000000

const char *src_file = "-";
const char *dst_file = "-";
unsigned *psrc;
unsigned *pdst;
volatile unsigned *pregs;

FILE *fin;
FILE *fout;

int verbose;
int stopshort = 8; /* #bytes to omit from check */

void cli(int argc, char *argv[])
{
	if (argc > 2) {
		src_file = argv[1];
		if (argc >= 3) {
			dst_file = argv[2];
		}
	}
	if (strcmp(src_file, "-") == 0) {
		fin = stdin;
	} else {
		fin = fopen(src_file, "r");
		if (fin == 0) {
			perror(src_file);
			exit(1);
		}
	}
	if (strcmp(dst_file, "-") == 0) {
		fout = stdout;
	} else {
		fout = fopen(dst_file, "w");
		if (fout == 0) {
			perror(dst_file);
			exit(1);
		}
	}

	if (getenv("VERBOSE")) {
		verbose = atoi(getenv("VERBOSE"));
	}
	if (getenv("STOPSHORT")) {
		stopshort = atoi(getenv("STOPSHORT"));
	}
}

#define MAP_FLAGS (MAP_SHARED | MAP_LOCKED)
void make_mappings()
{
	int fm = open("/dev/mem", O_RDWR);

	psrc = mmap((void *)SRC_OFFSET, SRC_LEN, PROT_WRITE, MAP_FLAGS, fm,
		    SRC_OFFSET);
	if (psrc == MAP_FAILED) {
		perror("src map");
		exit(1);
	}
	pdst = mmap((void *)DST_OFFSET, DST_LEN, PROT_READ, MAP_FLAGS, fm,
		    DST_OFFSET);
	if (pdst == MAP_FAILED) {
		perror("dst map");
		exit(1);
	}
	pregs = mmap((void *)REGS_OFFSET, REGS_LEN, PROT_READ | PROT_WRITE,
		     MAP_FLAGS, fm, REGS_OFFSET);
	if (pdst == MAP_FAILED) {
		perror("regs map");
		exit(1);
	}
}

void blt32(unsigned *dst, unsigned *src, unsigned wc)
{
	fprintf(stderr, "blt32 %p %p %d\n", dst, src, wc);
	while (wc--) {
		if (verbose && wc < 5) {
			fprintf(stderr, "blt32 %p = %p (%08x)\n", dst, src,
				*src);
		}
		*dst++ = *src++;
	}
}

void checksum(void *buf, int len)
{
	FILE *fp = popen("md5sum", "w");
	fwrite(buf, 1, len, fp);
	pclose(fp);
}
void fill_src()
{
	unsigned *buf = calloc(WORD_COUNT, sizeof(unsigned));

	fprintf(stderr, "calloc(%d, %d) ret %p\n", WORD_COUNT, sizeof(unsigned),
		buf);
	if (fread(buf, sizeof(unsigned), WORD_COUNT, fin) < 0) {
		perror("fread fail");
		exit(1);
	}
	checksum(buf, BYTE_COUNT);
	blt32(psrc, buf, WORD_COUNT);

	blt32(buf, psrc, WORD_COUNT);
	checksum(buf, BYTE_COUNT);
#ifdef BUG_STOPSHORT
	checksum(buf, BYTE_COUNT - stopshort);
#endif
	free(buf);
}

int diffMS(struct timespec *t0, struct timespec *t1)
{
	double dns = t1->tv_nsec - t0->tv_nsec;
	double dms = (t1->tv_sec - t0->tv_sec) * 1000 - dns / 1000000;

	return (int)dms;
}
void process()
{
	struct timespec t0, t1;
#define STATPRINT                                                           \
	if (verbose)                                                        \
		fprintf(stderr, "CTRL: 0x%08x STAT: 0x%08x\n", pregs[CTRL], \
			pregs[STAT]);
	int pollcat = 0;
	pregs[CTRL] = 0;
	STATPRINT;

	clock_gettime(CLOCK_REALTIME, &t0);

#ifdef BUG_MAXCOUNT
	pregs[CTRL] = ((WORD_COUNT - 1) << 16) | 1;
#else
	pregs[CTRL] = ((WORD_COUNT) << 16) | 1;
#endif
	STATPRINT;
	while ((pregs[STAT] & 1) == 0) {
		sched_yield();

		if (++pollcat > MAXPOLL) {
			fprintf(stderr, "quit on MAXPOLL\n");
			break;
		}
		STATPRINT;
	}

	clock_gettime(CLOCK_REALTIME, &t1);
	STATPRINT;

	fprintf(stderr, "DONE: pollcat %d  %d msec\n", pollcat,
		diffMS(&t0, &t1));
	pregs[CTRL] = 0;
}

void copy_dst()
{
	unsigned *buf = calloc(WORD_COUNT, sizeof(unsigned));

	blt32(buf, pdst, WORD_COUNT);
#ifdef BUG_STOPSHORT
	checksum(buf, BYTE_COUNT - stopshort);
#else
	checksum(buf, BYTE_COUNT);
#endif

	if (fwrite(buf, sizeof(unsigned), WORD_COUNT, fout) < 0) {
		perror("fwrite fail");
		exit(1);
	}
	free(buf);
}
int main(int argc, char *argv[])
{
	cli(argc, argv);
	make_mappings();
	fill_src();
	process();
	copy_dst();
	return 0;
}
