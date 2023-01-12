/*
 * ES.cpp
 *
 *  Created on: 13 Jan 2021
 *      Author: pgm
 */

#include <string.h>

#include "knobs.h"
#include "ES.h"

bool ISACQ480() {
	char mval[80];
	if (getKnob(0, "/etc/acq400/1/MODEL", mval) >= 0){
		return strstr(mval, "ACQ480") != NULL ||
				strstr(mval, "ACQ482") != NULL;
	}
	return false;
}

template <unsigned MASK, unsigned PAT, unsigned MATCHES>
class ES : public AbstractES {
	bool is_es_word(unsigned word) {
		return (word&MASK) == PAT;
	}
public:
	bool isES(unsigned *cursor){
		bool is_es = false;
		unsigned matches = MATCHES;
		for (int ic = 0; matches != 0; ++ic, matches >>= 1){
			if ((matches&0x1) != 0){
				if (is_es_word(cursor[ic])){
					is_es = true;
				}else{
					return false;
				}
			}
		}
		return is_es;
	}
};


#define EV0_MASK	0xffffffff
#define EV1_MASK	0xffffffff

AbstractES* AbstractES::evX_instance() {
	static AbstractES* _instance;
	if (!_instance){
		if (ISACQ480()){
			_instance = new ES<EVX_MASK, EVX_MAGIC, 0x0a>;
		}else{
			_instance = new ES<EVX_MASK, EVX_MAGIC, 0x0f>;
		}
	}
	return _instance;
}
AbstractES* AbstractES::ev0_instance() {
	static AbstractES* _instance;
	if (!_instance){
		if (ISACQ480()){
			_instance = new ES<EV0_MASK, EV0_MAGIC, 0x0a>;
		}else{
			_instance = new ES<EV0_MASK, EV0_MAGIC, 0x0f>;
		}
	}
	return _instance;
}
AbstractES* AbstractES::ev1_instance() {
	static AbstractES* _instance;
	if (!_instance){
		if (ISACQ480()){
			_instance = new ES<EV1_MASK, EV1_MAGIC, 0x0a>;
		}else{
			_instance = new ES<EV1_MASK, EV1_MAGIC, 0x0f>;
		}
	}
	return _instance;
}

