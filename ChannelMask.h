/*
 * ChannelMask.h : arbitrary length bitfield to specify a channel subset.
 * Specified using a string, choice of several formats:
 * ref 4GUGr50:
 * 30.2.8 /mnt/local/sysconfig/acq400_streamd.conf

CASE1: long-standing default
STREAM_OPTS=--subset=[start-channel,]length :
reduce output channel count to length,
starting from start-channel [1]

CASE2
alternate: 0xMASK where mask is a hex number showing a random set
of channels to include, eg:
STREAM_OPTS="--subset=0x355555555"
.. show ODD# channels out of 32 + 2 SPAD
the hex mask can be arbitrary length (eg 192 channels possible).
masks read right to left.

CASE3 :
0bMASK, where MASK is a binary number
STREAM_OPTS="--subset=0b1100001111"
ie channels 1,2,3,4,9,10

CASE4 *new*

--subset=CHn1,n2,n3....

Where n1,n2 are channel numbers starting at 1.
The key "CH" is necessary to disambiguate with CASE1

eg
--subset=CH1,2,3,4,33,34,35,36
--sum=[start-channel,]length :
sum over length channels, starting from start-channel [1]
sum output as stream of int32 from port 4270
STREAM_OPTS="--subset=8 –sum=4"


There are two static members that should be set before calling factory:

static int max_chan;			<< Client MUST set this value BEFORE calling factory().
static bool is_always_a_bitset;		<< Client MAY set this value BEFORE calling factory()/
 	 	 	 	 	 *.. the start,length contiguous mode was the original implementation in acq400_stream,
 	 	 	 	 	 *.. it's retained to ensure compatibility, also, it's thought to be faster
 	 	 	 	 	 *.. a new implementation can have a simpler bit-set only implementation by setting this flag.

 *
 *  Created on: 17 Jul 2026
 *      Author: pgm
 */

#ifndef CHANNELMASK_H_
#define CHANNELMASK_H_

#include <bitset>


/* MAXBIT: 256 : we have boxes with up to 192 channels,
 * so 256 gives headroom .. SPAD?
 */
static const unsigned MAXBIT = 256;
typedef std::bitset<MAXBIT> ChannelMask_t;

class ChannelMask: public ChannelMask_t {
	static ChannelMask_t intListToBitset(const std::string& input);
	static bool is_number_comma_string(const std::string& input, int& count);

	ChannelMask() : _start(0), _length(0), _is_bitset(false)
	{}

	ChannelMask(int start, int length);
	ChannelMask(unsigned long val);
	ChannelMask(std::string def);
	ChannelMask(ChannelMask_t cm);

	int _start;
	int _length;
	bool _is_bitset;

public:
	const int start() const { return _start; }
	const int length() const { return _length; }
	const bool is_bitset() const { return _is_bitset; }
	int get_channelcount() const;
	int get_topchan() const;
	int get_firstchan() const;

	static ChannelMask* factory(const char* def);

	std::string to_string() const;           /*<< return string repr. */

	static int max_chan;			/*<< Client MUST set this value BEFORE calling factory(). */
	static bool is_always_a_bitset;		/*<< Client MAY set this value BEFORE calling factory()/  */
};

#endif /* CHANNELMASK_H_ */
