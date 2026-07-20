/*
 * channel_mask_unit_test.cpp
 *
 *  Created on: 20 Jul 2026
 *      Author: pgm
 */

#include "ChannelMask.h"
#include "Env.h"


int main(int argc, char* argv[]){
	ChannelMask::max_chan = Env::getenv("ChannelMaskNCHAN", 64);
	ChannelMask::is_always_a_bitset = Env::getenv("ChannelMaskALWAYS_BITSET", 0);

	ChannelMask *cm;
	for (int ii = 1; ii < argc; ++ii){
		const char* def = argv[ii];
		printf("channel_mask_tester %s\n", def);
		cm = ChannelMask::factory(def);
		printf("%s\n", cm->to_string().c_str());
	}
	return 0;
}


