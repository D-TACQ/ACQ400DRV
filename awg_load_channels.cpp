/*
 * awg_load_channels.cpp
 *
 *  Created on: 2 Jan 2020
 *      Author: pgm
 */
/* awg_load_channels.cpp  D-TACQ ACQ400 mux and load AWG channels from channel files in /dev/shm/
 * Project: ACQ420_FMC
 * Created: 2 Jan 2020  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2020 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO
 * TODO
\* ------------------------------------------------------------------------- */



#include <stdio.h>
#include <vector>

#include "popt.h"


template <class T>
class Channel {
	T last_value;
	FILE *fp;
	enum State {
		S_FP_VALID,
		S_EOF
	} state;
public:
	Channel(int ic) : last_value(0) {
		char fname[80];
		snprintf(fname, 80, "/dev/shm/AWG.%02d", ic);
		fp = fopen(fname, "r");
		if (fp == NULL){
			state = S_EOF;
		}else{
			state = S_FP_VALID;
		}
	}
	~Channel() {
		if (fp){
			fclose(fp);
		}
	}

	T* next() {
		if (state == S_FP_VALID){
			if (fread(&last_value, sizeof(T), 1, fp) != 1){
				state = S_EOF;
			}
		}
		return &last_value;
	}
};




namespace G{
	int nchan = 8;
	int data32 = 1;
	int port = 54202;
	int minsam = 4;
};

struct poptOption opt_table[] = {
{ "nchan", 'n', POPT_ARG_INT, &G::nchan, 0, "number of AWG channels" },
{ "data32", 'd', POPT_ARG_INT, &G::data32, 0, "set 32 bit data"},
POPT_AUTOHELP
POPT_TABLEEND
};

void ui(int argc, const char** argv)
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		default:
			;
		}
	}
}


template <class T>
int load(void) {
	std::vector<Channel<T>*> channels;
	for (int ch = 1; ch <= G::nchan; ++ch){
		channels.push_back(new Channel<T>(ch));
	}

	for (int isam = 0; isam < G::minsam; ++isam){
		for (Channel<T>* ch: channels){
			fwrite(ch->next(), sizeof(T), 1, stdout);
		}
	}
	return 0;
}

int main(int argc, const char** argv)
{
	ui(argc, argv);
	if (G::data32){
		return load<int>();
	}else{
		return load<short>();
	}
}
