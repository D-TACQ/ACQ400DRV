/*
 * subset_mask_tester.cpp
 *
 *  Created on: 2 Dec 2024
 *      Author: pgm
 */

#include <stdio.h>
#include <string.h>

#include <bitset>
#include "hex_char_to_bin.h"
#include "Env"


const unsigned MAXBIT = 64;
typedef std::bitset<MAXBIT> ChannelMask;

int NCHAN = Env::getenv("")

int main(int argc, char* argv[]){
	ChannelMask *cm;
	for (int ii = 1; ii < argc; ++ii){
		const char* def = argv[ii];
		printf("subset_mask_tester %s\n", def);
		if (strncmp(def, "0x", 2) == 0){
			cm = new ChannelMask(hexStrToBin(def+2));
			printf("%s\n", cm->to_string().c_str());
		}
	}
	return 0;
}


