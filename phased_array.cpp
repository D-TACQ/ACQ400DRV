/* ------------------------------------------------------------------------- */
/* phased_array.cpp  D-TACQ ACQ400 FMC  DRIVER    "big buffer" : read or write
 * Project: ACQ420_FMC
 * Created: 16 Jul 2016  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2016 Peter Milne, D-TACQ Solutions Ltd         *
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

/**
 * phased_array: load a sparse matrix with repeated waveforms

 * assume a 96 channel AWG, with 72 active antennae
 * SR=500kHz
 * Each channel pulse is 256msec
 * Total waveform 3000msec
 * MUST complete in 6000msec
 * Use BufferA, Buffer B for continous output
 * Output is a SPARSE matrix (1/12 full). Assume memory is pre-zeroed (hardware assist)
 * Output is in [T][C] order. So each channel sample is spaced at NCHAN interval, bad for caching.

1. OMNI: All  72 staves are driven with the same pulse waveform at the same time

2. RDT: This is a rotational mode. 3 beams are created with time delays and these beams are rotated clockwise.So that there will be 36 beams at the end.

3. MAS : Fixed time delays are applied to 72 channels.

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "popt.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <libgen.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>

#include <sched.h>

#define VERID	"B1000"

#include <semaphore.h>
#include <syslog.h>

#include <vector>

#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"

#include "File.h"


using namespace std;


#define PULSE_MS	256	// msec, length one pulse, one antenna
#define WF_MS		3000	// msec, total waveform
#define NCHAN		96
#define NSTAVES		72
#define SR		500	// kHz
#define WS		sizeof(short)

#define USMS		1000	// microseconds in a millisecond

/* copy from driver .. */
enum AO_playloop_oneshot { AO_continuous, AO_oneshot, AO_oneshot_rearm };

namespace G {
	unsigned sample_size = sizeof(unsigned);	// bytes per sample
	int play_site = 1;
	unsigned offset = 0;
	int devnum = 0;					// full system.
	FILE* fp_out = stdout;
	FILE* fp_in = stdin;

	int mode = AO_oneshot;
	int verbose;
	unsigned buffer0;				// index from here

};


class PhaseArray {
public:
	int pulse_ms;
	int wf_ms;
	int si;				// microseconds
	int nchan;
	short* base;

	int pulse_samples() {
		return pulse_ms/si * USMS;
	}
	int wf_samples() {
		return wf_ms/si * USMS;
	}
	int wf_bytes() {
		return wf_samples() * nchan * WS;
	}

	virtual void fill(short* pulse, int ps, int nstaves, int delay, int ic0) = 0;

	PhaseArray(short *_base): base(_base) {
		wf_ms = WF_MS;
		si = 1000/SR;				// microseconds
		nchan = NCHAN;
	}
	virtual ~PhaseArray() {}
};

class FillOverPhaseArray : public PhaseArray {
public:
	virtual void fill(short* pulse, int ps, int nstaves, int delay, int ic0){
	/**< write pulse to raw memory */
		for (int ic = 0; ic < nstaves; ++ic){
			short* pb = base + (ic+ic0)*delay*nchan + ic+ic0;
			for (int sam = 0; sam < ps; ++sam, pb += nchan){
				*pb = pulse[sam];
			}
		}
	}
	FillOverPhaseArray(short *_base): PhaseArray(_base) {}
};

class FillAddPhaseArray : public PhaseArray {
public:
	virtual void fill(short* pulse, int ps, int nstaves, int delay, int ic0){
	/**< for overlapping pulse, add to raw memory .. assumed slower .. */
		for (int ic = 0; ic < nstaves; ++ic){
			short* pb = base + (ic+ic0)*delay*nchan + ic+ic0;
			for (int sam = 0; sam < ps; ++sam, pb += nchan){
				*pb += pulse[sam];
			}
		}
	}
	FillAddPhaseArray(short *_base): PhaseArray(_base) {}
};




#include <sys/time.h>

class Timer {
	struct timeval t0;
public:
	Timer() {
		gettimeofday(&t0, 0);
	}
	virtual ~Timer() {
		struct timeval t1;
		struct timeval td;

		gettimeofday(&t1, 0);
		timersub(&t1, &t0, &td);

		fprintf(stderr, "Time:%ld.%06ld\n", td.tv_sec, td.tv_usec);
	}
};
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int get_pulse(char* pulse_file, short **pbuf)
{
	struct stat stat_buf;
	if (stat(pulse_file, &stat_buf) != 0){
		perror(pulse_file);
		exit(1);
	}
	unsigned nelems = stat_buf.st_size/sizeof(short);
	short *pb = new short[nelems];
	FILE *fp = fopen(pulse_file, "r");
	if (fp == 0){
		perror(pulse_file);
		exit(2);
	}
	if (fread(pb, sizeof(short), nelems, fp) != nelems){
		perror(pulse_file);
		exit(3);
	}
	*pbuf = pb;
	return nelems;
}

class PulseDef {

public:
	short *pulse;
	int len;
	int staves;
	int delay;

	PulseDef(char* pulse_file, int _staves, int _delay) :
		staves(_staves), delay(_delay)
	{
		len = get_pulse(pulse_file, &pulse);
	}
};


#include "Buffer.h"

struct poptOption opt_table[] = {
	{ "sample-size", 'S', POPT_ARG_INT, &G::sample_size, 0,
			"bytes per sample [deprecated]"
	},
	{ "play", 'P', POPT_ARG_INT, &G::play_site, 0,
			"AWG site [deprecated]"
	},
	{ "offset", 'o', POPT_ARG_INT, &G::offset, 0,
			"offset in buffer (for multi-site ops)"
	},
	{ "mode",  'm', POPT_ARG_INT, &G::mode, 0,
			"play mode 0: continuous, 1:oneshot 2:oneshot_rearm"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

enum RUN_MODE { M_FILL, M_LOAD, M_DUMP };

int G_nsamples;

void set_playloop_length(int nsamples)
{
	char cmd[128];
	sprintf(cmd, "set.site %d playloop_length %d %d",
			G::play_site, G_nsamples = nsamples, G::mode);
	system(cmd);
}

int pad(int nsamples, int pad_samples)
{
	char* base = Buffer::the_buffers[0]->getBase();
	char* end = base + nsamples*G::sample_size;
	char* last = end - G::sample_size;

	while(pad_samples--){
		memcpy(end, last, G::sample_size);
		end += G::sample_size;
		nsamples++;
	}
	return nsamples;
}

int _load() {
	int maxbuf = Buffer::nbuffers*Buffer::bufferlen/G::sample_size;

	unsigned nsamples = fread(Buffer::the_buffers[0]->getBase(),
			G::sample_size, maxbuf, G::fp_in);

	return nsamples;
}

int fill() {
	_load();
	return 0;
}
int load() {
	set_playloop_length(0);

	set_playloop_length(_load());

	return 0;
}
int dump() {
	unsigned nsamples;
	getKnob(G::play_site, "playloop_length", &nsamples);

	fwrite(Buffer::the_buffers[0]->getBase(),
			G::sample_size, nsamples, G::fp_out);
	return 0;
}


#define DFB	"/sys/module/acq420fmc/parameters/distributor_first_buffer"

typedef vector<PulseDef*> VPD;
typedef vector<PulseDef*>::iterator VPDI;

int G_threads;

VPD* ui(int argc, const char** argv)
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);

	if (getenv("VERBOSE")){
		G::verbose = atoi(getenv("VERBOSE"));
	}
	if (getenv("PA_THREADS")){
		G_threads = atoi(getenv("PA_THREADS")) >= 1;
		fprintf(stderr, "PA_THREADS set %d\n", G_threads);
	}
	getKnob(G::devnum, "nbuffers",  &Buffer::nbuffers);
	getKnob(G::devnum, "bufferlen", &Buffer::bufferlen);
	getKnob(G::devnum, DFB, &G::buffer0);
	unsigned dist_s1;
	getKnob(0, "dist_s1", &dist_s1);
	if (dist_s1){
		G::play_site = dist_s1;
		getKnob(0, "/etc/acq400/0/dssb", &G::sample_size);
		fprintf(stderr, "s1:%d size:%d\n", dist_s1, G::sample_size);
	}
	int rc;

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		default:
			;
		}
	}



	VPD *vpd = new VPD;

	while (const char* mode = poptGetArg(opt_context)){
		char pulse_file[80];
		int staves =66;
		int delay = 0;
		if (sscanf(mode, "load=%s %d %d", pulse_file, &staves, &delay) > 0){
			fprintf(stderr, "load pulse_file:\"%s\" staves=%d delay=%d\n",
					pulse_file, staves, delay);
		}else{
			fprintf(stderr, "error \"%s\" must be load=file staves delay\n", mode);
		}
		vpd->push_back(new PulseDef(pulse_file, staves, delay));
	}
	return vpd;
}

class BufferManager {
	void init_buffers()
	{
		const char* root = getRoot(G::devnum);
		Buffer::last_buf = G::buffer0;
		for (unsigned ii = 0; ii < Buffer::nbuffers-G::buffer0; ++ii){
			Buffer::create(root, Buffer::bufferlen);
		}
	}
	void delete_buffers()
	{
		/* this _should_ be automatic. But it's not! */
		for (unsigned ii = 0; ii < Buffer::the_buffers.size(); ++ii){
			delete Buffer::the_buffers[ii];
		}
	}
public:
	BufferManager() {
		init_buffers();
	}
	~BufferManager() {
		delete_buffers();
		printf("DONE %d\n", G_nsamples);
	}
};

#define MARK fprintf(stderr, "MARK %d\n", __LINE__)

#include <sys/types.h>
#include <sys/wait.h>



int main(int argc, const char** argv)
{
	VPD *pulses = ui(argc, argv);
	BufferManager bm;
	PhaseArray* pa0 = new FillOverPhaseArray((short*)Buffer::the_buffers[0]->getBase());
	PhaseArray* pa1 = new FillAddPhaseArray((short*)Buffer::the_buffers[0]->getBase());
	PhaseArray* pa = pa0;

	if (pulses->empty()){
		fwrite(Buffer::the_buffers[0]->getBase(), 1, pa->wf_bytes(), stdout);
		return 0;
	}

	Timer T;

	for (VPDI it = pulses->begin(); it != pulses->end(); ++it, pa = pa1){
		PulseDef *pd = *it;
		int status;
		int cpid;

		fprintf(stderr, "fill\n");


		if (G_threads > 0){
			if ((cpid = fork()) == 0){
//				fprintf(stderr, "%d fill %d from %d\n", getpid(), pd->staves/2, 0);
				pa->fill(pd->pulse, pd->len, pd->staves/2, pd->delay, 0);
				exit(0);
			}else{
				if (cpid == -1){
					perror("fork() failed");
					exit(1);
				}
//				fprintf(stderr, "%d fill %d from %d\n", getpid(), pd->staves/2, pd->staves/2);
				pa->fill(pd->pulse, pd->len, pd->staves/2, pd->delay, pd->staves/2);
			}
			wait(&status);
		}else{
//			fprintf(stderr, "%d fill %d from %d\n", getpid(), pd->staves, 0);
			pa->fill(pd->pulse, pd->len, pd->staves, pd->delay, 0);
		}
	}
	return 0;
}


