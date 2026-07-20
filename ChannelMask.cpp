/*
 * ChannelMask.cpp
 *
 *  Created on: 17 Jul 2026
 *      Author: pgm
 *
 */

#include "ChannelMask.h"
#include "hex_char_to_bin.h"

#include <string.h>

#include <assert.h>
#include <sstream>
#include <vector>

int ChannelMask::max_chan = 0;
bool ChannelMask::is_always_a_bitset;


ChannelMask_t ChannelMask::intListToBitset(const std::string& input) {
	std::vector<unsigned> numbers;
	std::stringstream ss(input);
	int num;
	char comma;

	while (ss >> num) {
		numbers.push_back(num);
		ss >> comma;
	}

	ChannelMask_t result;
	for (unsigned n : numbers) {
		if (n >= 1 && n <= MAXBIT) {
			result.set(n-1);
		}
	}
	return result;
}

bool ChannelMask::is_number_comma_string(const std::string& input, int &count){
	bool want_comma = false;
	int ic = 0;
	count = 0;
	for (char c: input){
		if (want_comma){
			if (c == ','){
				want_comma = false;
			}else if (isdigit(c)){
				;                // >1 digit is OK
			}else{
				return false;    // ! (digit || comma)
			}
		}else{
			if (isdigit(c)){
				if (!want_comma){
					++count;
					want_comma = true;
				}
			}else{
				return false;	// ! (digit)  .. INCLUDING COMMA
			}
		}
		++ic;
	}
	return ic != 0;
}

int ChannelMask::get_channelcount() const {
	if (_is_bitset){
		const ChannelMask_t& cm = *this;
		int nset = 0;
		for (int ii = 0; ii < 256; ++ii){
			if (cm[ii]){
				++nset;
			}
		}
		return nset;
	}else{
		return _length;
	}
}

int ChannelMask::get_topchan() const{
	if (_is_bitset){
		const ChannelMask_t& cm = *this;
		int topchan = 0;
		for (int ii = 0; ii < 256; ++ii){
			if (cm[ii]){
				topchan = ii;
			}
		}
		return topchan;
	}else{
		return _start + _length;
	}
}
int ChannelMask::get_firstchan() const{
	if (_is_bitset){
		const ChannelMask_t& cm = *this;
		for (int ii = 0; ii < 256; ++ii){
			if (cm[ii]) return ii;
		}
		return 0;
	}else{
		return _start;
	}
}


ChannelMask::ChannelMask(int start, int length):
		_start(start), _length(length), _is_bitset(false)
{
	if (is_always_a_bitset){
		ChannelMask_t& cm = *this;

		for (int ii = 0; ii < max_chan; ++ii){
			cm[ii] = ii>=start && ii<=start+length? true: false;
		}
		_is_bitset = true;
	}
}

ChannelMask::ChannelMask(unsigned long val):
		ChannelMask_t(val), _start(0), _length(0), _is_bitset(true)
{}

ChannelMask::ChannelMask(ChannelMask_t cm):
		ChannelMask_t(cm), _is_bitset(true)
{}

//#define MARK fprintf(stderr, "%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
#define MARK


ChannelMask::ChannelMask(std::string def):
		ChannelMask_t(def), _start(0), _length(0), _is_bitset(true)
{
	MARK;
}

std::string ChannelMask::to_string() const {
	MARK;
	if (_is_bitset){
		MARK;
		const ChannelMask_t& cm = *this;
		MARK;
		std::string repr;
		MARK;
		int _top = get_topchan();

		for (int ii = 0; ii <= _top; ++ii){
			MARK;
			repr.append(1, cm[ii]? '1': '0');
		}
		MARK;
		return repr;
	}else{
		MARK;
		char repr[128];
		snprintf(repr, 128, "start:%d length:%d\n",
				_start, _length);
		return repr;
	}
}


ChannelMask* ChannelMask::factory(const char* def)
{
	ChannelMask *cm;
	int count;

	if (is_number_comma_string(def, count)){
		MARK;
		int start, length;
		switch(count){
		case 2:
			assert(sscanf(def, "%d,%d", &start, &length) == 2);
			break;
		case 1:
			assert(sscanf(def, "%d", &length) == 1);
			start = 0;
			break;
		case 0:
			start = 0;
			length = max_chan;
			break;
		default:
			return new ChannelMask(intListToBitset(def));
		}
		assert(max_chan >= 1);
		assert(length-start <= max_chan);
		return new ChannelMask(start, length);
	}else if (strncmp(def, "0x", 2) == 0){
		MARK;
		return new ChannelMask(hexStrToBin(def+2));  // arbitrary mask, hex definition
	}else if (strncmp(def, "0b", 2) == 0){
		MARK;
		return new ChannelMask(def+2);		   // arbitrary mask, binary definition
	}else if (strncmp(def, "CH", 2) == 0 &&
		  is_number_comma_string(def+2, count)){
		MARK;
		return new ChannelMask(intListToBitset(def+2));
	}else{
		MARK;
		return new ChannelMask(0, max_chan);
	}
	return cm;
}
