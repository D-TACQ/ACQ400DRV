/*
 * wrtt_mon.c
 *
 *  Created on: 29 Sep 2019
 *      Author: pgm
 */


#include <stdio.h>
#include <assert.h>

void wrtt_mon(FILE* fp_wrtt, FILE* fp_trg)
{
	unsigned wrtt, trg;
	unsigned delta;

	fread(&wrtt, sizeof(unsigned), 1, fp_wrtt); 	/* blocks */
	fread(&trg,  sizeof(unsigned), 1, fp_trg);	/* noblock */
	trg &= 0x7fffffff;
	wrtt &= 0x7fffffff;
	delta = wrtt - trg;
	printf("%08x -> %08x delta %d\n", trg, wrtt, delta);
}



int main(int argc, char* argv[])
{
	FILE* fp_wrtt = fopen("/dev/acq400.0.wr_tt", "r");
	FILE* fp_trg =  fopen("/dev/acq400.0.wr_trg", "r");

	assert(fp_wrtt);
	assert(fp_trg);
	while(1){
		wrtt_mon(fp_wrtt, fp_trg);
	}
}
