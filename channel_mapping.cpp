/*
 * channel_mapping.cpp
 *
 *  Created on: 4 Sep 2021
 *      Author: pgm
 */


#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include <stdio.h>
#include "knobs.h"
#include "acq-util.h"

namespace G {
	unsigned int nchan;
	unsigned int naxi_dma;
	bool use_mgtdram;
	char model[32];
};

int print_the_map(void)
{
	if (G::use_mgtdram && strncmp(G::model, "ACQ48", 5) == 0 && G::nchan > 8){
		int blocks[] = { 0, 8, 4, 12, 16, 24, 20, 28, 32, 40, 36, 44 };
		for (unsigned int ic = 0; ic < G::nchan; ++ic){
			printf("%d%c", blocks[ic/4]+ic%4, ic+1==G::nchan? '\n': ',');
		}
		return 0;
	}

	for (unsigned int ic = 0; ic < G::nchan; ++ic){
		printf("%d%c", ic, ic+1==G::nchan? '\n': ',');
	}
	return 0;
}
int main(int argc, char** argv) {
	if (argc > 1){
		if (strncmp(argv[1], "mgtdram", 7) == 0){
			G::use_mgtdram = true;
		}
	}
	getKnob(0, "/etc/acq400/1/MODEL", G::model);
	getKnob(0, "/etc/acq400/0/NCHAN", &G::nchan);
	getKnob(0, "/dev/acq400.0.knobs/has_axi_dma", &G::naxi_dma);
	return print_the_map();
}
