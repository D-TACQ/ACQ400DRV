/* ------------------------------------------------------------------------- *
 * ScratchPad.h  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2013 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 5 Nov 2013  
 *    Author: pgm                                                         
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#ifndef SCRATCHPAD_H_
#define SCRATCHPAD_H_

#include <stdarg.h>

/** singleton .. */
class Scratchpad {
	char *root;

	Scratchpad(int site = 0)
	{
		root = new char[80];
		snprintf(root, 80, "/dev/acq400.%d.knobs", site);
	}
	virtual void set(int idx, const char *fmt, u32 value)
	{
		char knob[80];
		snprintf(knob, 80, "%s/spad%d", root, idx);
		File fc(knob, "w");
		fc.printf(fmt, value);
	}
	virtual int get(int idx, const char *fmt, u32 *value)
	{
		char knob[80];
		snprintf(knob, 80, "%s/spad%d", root, idx);

		File fc(knob, "r");

		return fscanf(fc(), fmt, value) == 1 ? 0 : -1;
	}

    public:
	enum SP_INDEX {
		SP_SAMPLE_COUNT,
		SP_MUX_STATUS,
		SP_MUX_CH01,
		SP_MUX_CH02,
		SP_AWG_G1,
		SP_AWG_G2,
		SP_AWG_G3,
		SP_AWG_G4,
	};
	enum SP_STATUS {
		SP_MUX_STATUS_BUSY = 0xb5b5b5b5,
		SP_MUX_STATUS_DONE = 0xd00e0000
	};
	static Scratchpad &instance()
	{
		static Scratchpad *_instance;

		if (!_instance) {
			_instance = new Scratchpad();
		}
		return *_instance;
	}

	virtual void set(int idx, u32 value)
	{
		u32 v2;
		int loopcount = 0;
		int errcount = 0;

		do {
			set(idx, "%08x", value);
			get(idx, &v2);
			if (value != v2) {
				if (errcount && ++errcount < 10) {
					fprintf(stderr,
						"SCRATCHPAD validate WARNING, retry %d idx %d %x != %x\n",
						++loopcount, idx, value, v2);
				}
			}
		} while (loopcount < 20 && value != v2);

		if (loopcount > 10) {
			fprintf(stderr,
				"SCRATCHPAD validate WARNING: we are working way too hard %d\n",
				loopcount);
		}
	}

	virtual int get(int idx, u32 *value)
	{
		return get(idx, "%x", value);
	}
	virtual u32 get(int idx)
	{
		u32 value;
		get(idx, &value);
		return value;
	}
	virtual void setBits(int idx, u32 bits)
	{
		set(idx, "+%08x", bits);
	}
	virtual void clrBits(int idx, u32 bits)
	{
		set(idx, "-%08x", bits);
	}
};

#endif /* SCRATCHPAD_H_ */
