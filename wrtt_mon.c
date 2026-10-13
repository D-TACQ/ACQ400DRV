/*
 * wrtt_mon.c
 *
 *  Created on: 29 Sep 2019
 *      Author: pgm
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void wrtt_mon(FILE *fp_wrtt, FILE *fp_trg)
{
	unsigned wrtt, trg;
	unsigned delta;

	fread(&wrtt, sizeof(unsigned), 1, fp_wrtt); /* blocks */
	fread(&trg, sizeof(unsigned), 1, fp_trg); /* noblock */
	trg &= 0x7fffffff;
	wrtt &= 0x7fffffff;
	delta = wrtt - trg;
	printf("%08x -> %08x delta %d\n", trg, wrtt, delta);
}

const char *fname(const char *ext, int id)
{
	static char fname_buf[80];
	sprintf(fname_buf, "/dev/acq400.0.%s%d", ext, id);
	return fname_buf;
}

int main(int argc, char *argv[])
{
	int id = argc > 1 ? atoi(argv[1]) : 0;

	FILE *fp_wrtt = fopen(fname("wr_tt", id), "r");
	FILE *fp_trg = fopen(fname("wr_trg", id), "r");

	assert(fp_wrtt);
	assert(fp_trg);
	while (1) {
		wrtt_mon(fp_wrtt, fp_trg);
	}
}
