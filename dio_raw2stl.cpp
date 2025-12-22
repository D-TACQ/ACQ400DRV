/* ------------------------------------------------------------------------- */
/* dio_raw2stl.cpp  D-TACQ ACQ400 FMC  DRIVER
 * Project: ACQ420_FMC
 * Created: 22 December 2025 			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2025 Peter Milne, D-TACQ Solutions Ltd         *
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


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>

#include "Knob.h"

#define NBITS	32

#define DFMT "/dev/acq400/data/%d/01"

#define FINISHED 0xffffffff

namespace G {
	FILE *fp_in;
	unsigned skip;
	unsigned finished = FINISHED;
	unsigned timeout = 0;			// timeout value : s
	unsigned seconds = 0;

	unsigned long long clocks[NBITS] = {};

	unsigned long long clk;

	unsigned fin = 0;              // mask shows inputs that have had a transition
	unsigned timed_out;
	bool heartbeat;		       // print 1s heartbeat if set
	bool before_first_last;        // sample before trigger, first sample from trigger, last sample
	unsigned before, first, last;  // values at before, first, last
	unsigned long samples;		// overflow at 85s @50Mhz.
};


struct OutputFormat {
	unsigned long samples;
	unsigned state;
};

#define PAGEW (4096/sizeof(unsigned))

int readw(unsigned *xx)
{
	static unsigned buf[PAGEW];
	static int cursor;
	static int len;

	if (cursor >= len){
		len = fread(buf, sizeof(xx), PAGEW, G::fp_in);
		if (len <= 0){
			perror("input terminated");
			exit(1);
		}
		cursor = 0;
	}
	*xx = buf[cursor++];
	return 1;
}


void alarm_handler(int sig)
{
	if (G::samples){
		G::seconds += 1;
		if (G::heartbeat){
			printf("HEARTBEAT=%u,%llu FIN:0x%08x != 0x%08x\n",
				G::seconds, G::clk, G::fin, G::finished);
			fflush(stdout);
		}
		if (G::seconds > G::timeout){
			G::timed_out = 1;
			return;
		}
	}

	alarm(1);
}

void print_clocks(void) {
	printf("FIRST_TRANSITION=");
	for (int ib = 0; ib < NBITS; ++ib){
		printf("%llu%c", G::clocks[ib], ib+1 == NBITS? '\n': ',');
	}
	if (G::before_first_last){
		printf("BEFORE_FIRST_LAST=0x%08x,0x%08x,0x%08x\n", G::before, G::first, G::last);
		printf("LAST_CLOCK=%llu\n", G::clk);
		printf("TIMED_OUT=%d\n", G::timed_out);
	}
}



void count_clocks(void)
{
	unsigned x0 = 0;
	unsigned x1 = 0;

	if (readw(&x0) != 1){
		perror("failed to read first word");
		exit(1);
	}
	G::samples++;
	unsigned clk = 1;

	for (; clk < G::skip; ++clk){
		if (readw(&x0) != 1){
			perror("run out of road with skip");
			exit(1);
		}
	}
	for (; !G::timed_out && G::fin != G::finished && readw(&x1) == 1; ++G::clk, x0 = x1){
		unsigned change = (x1 ^ x0);
		if (change & ~G::fin){
			for (unsigned ib = 0; ib < NBITS; ++ib){
				unsigned bit = 1<<ib;
				if ((G::fin&bit) == 0 && (change&bit) != 0){
					G::clocks[ib] = clk;
					G::fin |= bit;
				}
			}
		}
	}
	fclose(G::fp_in);

}


bool onChange(unsigned x0, unsigned x1)
{
	unsigned change = (x1 ^ x0);
	bool changes = change != 0;  // should never be zero
	struct OutputFormat of;

	of.samples = G::samples;
	of.state = x1;

	fwrite(&of, sizeof(struct OutputFormat), 1, stdout);
	fflush(stdout);
	return changes;
}



unsigned read_value_before_trigger(int site){
	char value[32];
	Knob knob(site, "di_snoop");
	assert(knob.get(value) == 1);

	return strtoul(value, 0, 16);
}

void count_clocks_live() {
	unsigned x0 = 0;
	unsigned x1 = 0;


	if (readw(&x0) != 1){
		perror("failed to read first word");
		exit(1);
	}
	G::first = x0;
	G::samples++;


	if (G::timeout){
		signal(SIGALRM, alarm_handler);
		alarm(1);
	}

	for (G::clk = 1; !G::timed_out && (G::fin&G::finished) != G::finished; ++G::clk, x0 = x1){
		readw(&x1);
		G::samples++;
		(x1 != x0) && onChange(x0, x1);
	}
	G::last = x1;
}

int ui(int argc, const char** argv)
{
	int site = 1;
	if (getenv("SITE") != 0){
		site = atoi(getenv("SITE"));
	}
	if (getenv("SKIP") != 0){
		G::skip = atoi(getenv("SKIP"));
	}
	if (getenv("TOP_FINISHED")){
		G::finished = strtoul(getenv("TOP_FINISHED"), 0, 16);
	}else if (getenv("FINISHED")){
		G::finished = strtoul(getenv("FINISHED"), 0, 16);
	}
	if (getenv("TOP_TIMEOUT")){
		G::timeout = atoi(getenv("TOP_TIMEOUT"));
	}else if (getenv("TIMEOUT")){
		G::timeout = atoi(getenv("TIMEOUT"));
	}
	if (getenv("TOP_HEARTBEAT")){
		G::heartbeat = atoi(getenv("TOP_HEARTBEAT"));
	}
	if (getenv("TOP_BEFORE_FIRST_LAST")){
		G::before_first_last = true;
		G::before = read_value_before_trigger(site);
	}
	if (argc > 0 && argv[1][0] == '-'){
		G::fp_in = stdin;
		return 1;
	}
	char fname[80];
	snprintf(fname, 80, DFMT, site);
	G::fp_in = fopen(fname, "r");
	if (!G::fp_in){
		perror(fname);
		exit(1);
	}


	return 0;
}

int main(int argc, const char** argv)
{
	if (ui(argc, argv)){
		count_clocks_live();
		sleep(2);
		system("kill -9 $(cat /var/run/acq400_stream_headImpl.0.pid)");
		sleep(2);
	}else{
		count_clocks();
	}

	return 0;
}

