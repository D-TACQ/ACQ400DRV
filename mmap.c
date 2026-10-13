/* ------------------------------------------------------------------------- */
/* mmap.c  		                     	 			     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 pgm, D-TACQ Solutions Ltd                            *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Mar 24, 2013
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

/*****************************************************************************
 *
 * File: mmap.c
 *
 * $RCSfile: mmap.c,v $
 *
 * Copyright (C) 1999 D-TACQ Solutions Ltd
 * not to be used without owner's permission
 *
 * Description:
 *
 * $Id: mmap.c,v 1.4 2001/10/07 10:01:42 pgm Exp $
 * $Log: mmap.c,v $
 * Revision 1.4  2001/10/07 10:01:42  pgm
 * *** empty log message ***
 *
 * Revision 1.3  2001/03/23 19:44:46  pgm
 * map only opt, debugs added
 *
 * Revision 1.2  2000/10/05 21:29:55  pgm
 * mmap can read,write,fill
 *
 * Revision 1.1  2000/10/05 20:56:38  pgm
 * *** empty log message ***
 *
 * Revision 1.1  1999/10/22 16:26:49  pgm
 * first entry to cvs
 *
 *
\*****************************************************************************/

#undef __KERNEL__

#include "local.h"

#include <assert.h>
#include <errno.h>
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

#define FNAME "/dev/acq32/acq32.1.raw"

#define HELP                                                     \
	"mmap $Revision: 1.5 $\n"                                \
	"usage: mmap opts\n"                                     \
	"    -f device name\n"                                   \
	"    -r read [default]\n"                                \
	"    -w write\n"                                         \
	"    -W write longs\n"                                   \
	"    -n nop (just block, holding the mapping)\n"         \
	"    -o offset\n"                                        \
	"    -l length\n"                                        \
	"    -M mapping offset (pages)\n"                        \
	"maps device [ram] space and either\n"                   \
	"    reads from ram to stdout\n"                         \
	"-or-writes to ram from stdin\n"                         \
	"   -b [-v value] = block fill [default 0xdeadbeef]\n"   \
	"   -T [list of test regs] registers walking bit test\n" \
	""

int acq200_debug;

unsigned map_offset;

int doRegsTest(volatile unsigned *regs, unsigned *offsets, int nregs)
{
	int ir;
	int irr;
	int pass = 0;
	int fail = 0;

	dbg(2, "testing ..");

	for (ir = 0; ir != nregs; ++ir) {
		regs[offsets[ir]] = 0;
	}
	for (ir = 0; ir != nregs; ++ir) {
		int cursor;
		for (cursor = 0x1; cursor; cursor <<= 1) {
			unsigned wanted, got;

			dbg(3, "regs[%d] := %08x", offsets[ir], cursor);

			regs[offsets[ir]] = cursor;

			for (irr = 0; irr != nregs; ++irr) {
				got = regs[offsets[irr]];

				dbg(3, "regs[%d] = %08x", irr, got);
				if (irr == ir) {
					wanted = cursor;
				} else {
					wanted = 0;
				}
				if (wanted != got) {
					dbg(1,
					    "regs[%d] wanted 0x%08x got 0x%08x",
					    offsets[irr], wanted, got);
					++fail;
				} else {
					++pass;
				}
			}
			dbg(3, "\n");
		}
		regs[offsets[ir]] = cursor;
	}

	dbg(2, "complete %d fail %d pass", fail, pass);

	return fail != 0;
}

int G_fail;
int G_pass;
int please_quit;
int alarm_set;

int regsTest(void *pmem, const char *argv[])
{
	/* argv is a list of register offsets */
	int ii;
	int nargs;
	unsigned *offsets;
	int iter = 0;

	for (nargs = 0; argv[nargs] != NULL; ++nargs) {
		;
	}

	info("pmem %p nargs:%d argv[0] \"%s\"", pmem, nargs, argv[0]);

	offsets = calloc(nargs, sizeof(unsigned));

	for (ii = 0; ii != nargs; ++ii) {
		offsets[ii] = strtoul(argv[ii], 0, 0);
		offsets[ii] /= sizeof(unsigned);
		dbg(2, "offsets[%d] (%s) is %d", ii, argv[ii], offsets[ii]);
	}

	while (!please_quit) {
		if (doRegsTest((unsigned *)pmem, offsets, nargs)) {
			G_fail++;
		} else {
			G_pass++;
		}
		if ((++iter & 0x01ff) == 0 || alarm_set || please_quit) {
			dbg(2, "FAIL:%10d PASS:%10d %s", G_fail, G_pass,
			    G_fail == 0 ? "*** NO ERRORS ***" : ":-( :-( :-(");
			alarm_set = 0;
		}
	}

	return G_fail != 0;
}

static void alarm_handler(int signum)
{
	alarm_set = 1;
}

static void quit_handler(int signum)
{
	alarm_handler(signum);
	please_quit = 1;
}
static void install_handlers(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = alarm_handler;

	struct sigaction saq;
	memset(&saq, 0, sizeof(saq));
	sa.sa_handler = quit_handler;

	if (sigaction(SIGALRM, &sa, NULL))
		perror("sigaction");
	if (sigaction(SIGINT, &saq, NULL))
		perror("sigaction");
}

void readl(unsigned *region, unsigned offset32, int length32)
{
	unsigned *src = region + offset32;
	unsigned *lbuf = calloc(length32, sizeof(unsigned));
	unsigned *dst = lbuf;
	unsigned *dst99 = lbuf + length32;

	dbg(1, "readl region:%p offset32:%d src:%p length32:%d\n", region,
	    offset32, src, length32);
	for (; dst != dst99;) {
		*dst++ = *src++;
	}
	write(1, (char *)lbuf, length32 * sizeof(unsigned));
	free(lbuf);
}

int main(int argc, const char **argv)
{
	/* WORKTODO ... args handling */

	int fd;
	void *region;
	char *fname = FNAME;
	unsigned offset = 0;
	unsigned length = 0x100000;
	int rc;
	unsigned fill_value = 0xdeadbeef;
	int fill_incr = 0;
	enum MODE {
		M_READ,
		M_WRITE,
		M_FILL,
		M_TEST,
		M_NOP,
		M_WRITEL,
		M_READL
	} mode = M_READ;
	char *map_offset_str;
	int read_l_n32;
	int write_l_n32;
	int read_l_b = 0;

	struct poptOption opt_table[] = {
		{ "device", 'f', POPT_ARG_STRING, &fname, 0 },
		{ "read", 'r', POPT_ARG_INT, &read_l_b, 'r' },
		{ "read_b", 'y', POPT_ARG_INT, &read_l_b, 'r' },
		{ "read_l", 'R', POPT_ARG_INT, &read_l_n32, 'R', "read longs" },
		{ "write", 'w', POPT_ARG_NONE, 0, 'w' },
		{ "writel", 'W', POPT_ARG_NONE, 0, 'W',
		  "write longs default: same reg, to increment, set fill_incr" },
		{ "nop", 'n', POPT_ARG_NONE, 0, 'n' },
		{ "fill", 'b', POPT_ARG_INT, &write_l_n32, 'f' },
		{ "fill_incr", 'I', POPT_ARG_INT, &fill_incr, 'i' },
		{ "offset", 'o', POPT_ARG_INT, &offset, 'o' },
		{ "length", 'l', POPT_ARG_INT, &length, 'l' },
		{ "value", 'v', POPT_ARG_INT, &fill_value, 0 },
		{ "map_offset", 'M', POPT_ARG_STRING, &map_offset_str, 'M' },
		{ "regstest", 'T', POPT_ARG_NONE, 0, 'T' },
		{ "verbose", 'V', POPT_ARG_INT, &acq200_debug, 0 },
		POPT_AUTOHELP{}
	};

	poptContext opt_context;

	opt_context = poptGetContext(argv[0], argc, argv, opt_table, 0);

	int open_mode = O_RDONLY;
	int mmap_mode = PROT_READ;

	while ((rc = poptGetNextOpt(opt_context)) > 0) {
		//fprintf(stderr, "opt %c\n", rc);
		switch (rc) {
		case 'r':
			if (read_l_b == 0)
				read_l_b = length;
			mode = M_READ;
			break;
		case 'w':
			open_mode = O_RDWR;
			mmap_mode = PROT_READ | PROT_WRITE;
			mode = M_WRITE;
			break;
		case 'W':
			open_mode = O_RDWR;
			mmap_mode = PROT_READ | PROT_WRITE;
			mode = M_WRITEL;
			break;
		case 'n':
			mode = M_NOP;
			break;
		case 'f':
			open_mode = O_RDWR;
			mmap_mode = PROT_READ | PROT_WRITE;
			mode = M_FILL;
			break;
		case 'T':
			mode = M_TEST;
			break;
		case 'M':
			map_offset = strtoul(map_offset_str, 0, 0);
			break;
		case 'R':
			mode = M_READL;
			break;
		}
	} // processes all other opts via arg pointers

	if (map_offset) {
		dbg(1, "map_offset set 0x%08x\n", map_offset);
	}

	if ((fd = open(fname, open_mode)) < 0) {
		fprintf(stderr, "mmap: failed to open device \"%s\" - ", fname);
		perror("");
		return 1;
	}

	//	map_offset <<= 12;
	//	fprintf(stderr, "about to call mmap: length:%d map_offset:%d\n", length, map_offset);
	//	fprintf(stderr, "about to call mmap: length:%x map_offset:%x\n", length, map_offset);
	region = mmap(NULL, length, mmap_mode, MAP_SHARED, fd, map_offset);

	if (region == (caddr_t)-1) {
		perror("mmap");
		return 1;
	}

	switch (mode) {
	default:
	case M_READL:
		dbg(1, "M_READL %d lw", read_l_n32);
		readl((unsigned *)region, offset / sizeof(u32), read_l_n32);

		break;
	case M_READ:
		dbg(1, "M_READ %d bytes", read_l_b);
		// spew to stdout in one big blurt
		dbg(1, "read region:%p offsetb8:%d src:%p lengthb:%d\n", region,
		    offset, (char *)region + offset, read_l_b);
		write(1, (char *)region + offset, read_l_b);
		break;
	case M_FILL: {
		unsigned *praw = (unsigned *)&((char *)region)[offset];
		int iwrite;
		int imax = write_l_n32;

		for (iwrite = 0; iwrite != imax; ++iwrite) {
			praw[iwrite] = fill_value;
			fill_value += fill_incr;
		}
		break;
	}
	case M_WRITE: {
		// write to memory - read input until first of EOF or length
		// it's slow char at a time stuff but who cares?

		int iwrite;
		int cc;
		char *praw = &((char *)region)[offset];

		for (iwrite = 0; (cc = getchar()) != -1 && iwrite != length;
		     ++iwrite) {
			praw[iwrite] = cc;
		}
		break;
	}
	case M_WRITEL: {
		// write to memory - read input until first of EOF or length
		// it's slow char at a time stuff but who cares?

		int iwrite = 0;
		unsigned wl;
		char *praw = &((char *)region)[offset];
		unsigned *plong = (unsigned *)praw;

		while (fread(&wl, sizeof(unsigned), 1, stdin) == 1) {
			dbg(2, "plong %p := 0x%08x\n", plong + iwrite, wl);
			plong[iwrite += fill_incr] = wl;
		}
		break;
	}
	case M_NOP:
		fprintf(stderr,
			"mmap: blocking. Hit any key to continue, q to quit\n");

		while (getchar() != 'q') {
			char aline[128];
			FILE *fp = fopen("/proc/self/maps", "r");
			assert(fp);

			while (fgets(aline, sizeof(aline), fp) != NULL) {
				fprintf(stderr, aline);
			}
			fclose(fp);
		}
		break;
	case M_TEST:
		install_handlers();
		//		alarm(2);
		return regsTest(region, poptGetArgs(opt_context));
		break;
	}

	return 0;
}
