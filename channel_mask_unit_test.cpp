/*
 * channel_mask_unit_test.cpp
 *
 *  Created on: 20 Jul 2026
 *      Author: pgm
 */

#include "ChannelMask.h"
#include "Env.h"

const char* unit_tests[] = {
"my name",    // argv0 is the program name, not evaluated..
"#CASE1 (see ChannelMask.h)\n",
"# blank.. return full house",
"",
"# single number, return 0..len",
"8",
" two numbers, return start,len",
"9,16",
"#CASE2\n",
"0x355555555",
"0x3aaaaaaaa",
"0x3f00000ff",
"#  ChannelMaskNCHAN=128 for best result",
"0xFFFF00000000FFFF",
"#CASE3\n",
"0b1111000011",
"#CASE4\n",
"CH9,10,11,12,13,14,15,16",
"# .. now try again ChannelMaskALWAYS_BITSET and compare with 9,16",
"# .. correct replacement for CASE#1 9,16:",
"CH9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25",
"CH1,2,3,4,5,6,7,8,33,34,35,36",
"# some garbage input, results in full house",
"a.b.c",
"x1",
"!",
"@123",
"# if the CASE1 contiguous list has > 2 elements, assume channel numbers like CASE4\n",
"1,3,9"
};
const int ntests = sizeof(unit_tests)/sizeof(char*);

int main(int argc, const char* argv[]){
	ChannelMask::max_chan = Env::getenv("ChannelMaskNCHAN", 64);
	ChannelMask::is_always_a_bitset = Env::getenv("ChannelMaskALWAYS_BITSET", 0);

	if (argc == 1){
		argc = ntests;
		argv = unit_tests;
	}

	ChannelMask *cm;
	for (int ii = 1; ii < argc; ++ii){
		const char* def = argv[ii];
		if (def[0] == '#'){
			printf("%s\n", def);
		}else{
			printf("channel_mask_tester %s\n", def);
			cm = ChannelMask::factory(def);
			printf("%s\n", cm->to_string().c_str());
		}
	}
	return 0;
}


