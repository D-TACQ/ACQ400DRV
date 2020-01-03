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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "popt.h"
#include "Knob.h"


template <class T>
class Channel {
	T last_value;
	FILE *fp;
	enum State {
		S_FP_VALID,
		S_EOF
	} state;
	int _nsam;
public:
	Channel(int ic) : last_value(0), _nsam(0) {
		char fname[80];
		snprintf(fname, 80, "/dev/shm/AWG.%02d", ic);
		fp = fopen(fname, "r");
		if (fp == NULL){
			state = S_EOF;
		}else{
			struct stat statbuf;
			if (fstat(fileno(fp), &statbuf) != 0){
				perror(fname);
				state = S_EOF;
			}else{
				_nsam = statbuf.st_size/sizeof(T);
				state = S_FP_VALID;
			}
		}
	}
	~Channel() {
		if (fp){
			fclose(fp);
		}
	}
	int nsam() {
		return _nsam;
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
	int mode = 2;
	int minsam = 4;
};

struct poptOption opt_table[] = {
{ "nchan", 'n', POPT_ARG_INT, &G::nchan, 0, "number of AWG channels" },
{ "data32", 'd', POPT_ARG_INT, &G::data32, 0, "set 32 bit data"},
{ "mode", 'm', POPT_ARG_INT, &G::mode, 0, "set mode 0:cont, 1:oneshot, 2:oneshot_repeat, -1: stdio" },
POPT_AUTOHELP
POPT_TABLEEND
};

#define PAGE	0x1000
#define MINPAGES 4

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

#define MINDMA 64

void set_playbuffer_len(unsigned play_len)
{
	Knob buffer_len("/sys/module/acq420fmc/parameters/bufferlen");
	unsigned bl;
	buffer_len.get(&bl);

	if (play_len > 4*bl){
		play_len = bl;		/* HUGE use full buffers */
	}else{
		play_len /= 4;		/* we're going to use 4 buffers */
	}
	if (play_len < PAGE*MINPAGES){
		play_len = PAGE*MINPAGES;
	}else{
		if (play_len%MINDMA){
			int residue = play_len%MINDMA;
			play_len += MINDMA - residue;
		}
	}
	Knob playbuffer_len("/etc/acq400/0/dist_bufferlen_play");
	playbuffer_len.set(play_len);
}

template <class T>
int load(FILE* out) {
	std::vector<Channel<T>*> channels;
	G::minsam = (MINPAGES*PAGE) / (sizeof(T) * G::nchan);
	for (int ch = 1; ch <= G::nchan; ++ch){
		Channel<T> *channel = new Channel<T>(ch);
		channels.push_back(channel);
		if (channel->nsam() > G::minsam){
			G::minsam = channel->nsam();
		}
	}
	set_playbuffer_len(G::minsam*sizeof(T) * G::nchan);

	for (int isam = 0; isam < G::minsam; ++isam){
		for (Channel<T>* ch: channels){
			fwrite(ch->next(), sizeof(T), 1, out);
		}
	}
	return 0;
}

int main(int argc, const char** argv)
{
	ui(argc, argv);
	FILE *fp = stdout;

	if (G::mode != -1){
		char cmd[80];
		snprintf(cmd, 80, "bb load --mode %d", G::mode);
		fp = popen(cmd, "w");
		assert(fp);
	}
	int rc;
	if (G::data32){
		rc = load<int>(fp);
	}else{
		rc = load<short>(fp);
	}
	if (fp != stdout){
		pclose(fp);
	}
	return rc;
}
