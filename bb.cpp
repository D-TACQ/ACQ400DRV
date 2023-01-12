/* ------------------------------------------------------------------------- */
/* bb.cpp  D-TACQ ACQ400 FMC  DRIVER    "big buffer" : read or write
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

#include <syslog.h>

#define VERID	"B1007"

#define NCHAN	4

#include <semaphore.h>
#include <syslog.h>


#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"

#include "Env.h"
#include "File.h"

#include "tcp_server.h"


using namespace std;

/* copy from driver .. */
enum AO_playloop_oneshot { AO_continuous, AO_oneshot, AO_oneshot_rearm };

#define G_LOAD_THRESHOLD_DEFAULT 2
#define G_PAD_NONE 0
#define G_PAD_LAST 1
#define G_PAD_ZERO 2

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
	int concurrent;
	int minbufs = 4;
	int max_samples;
	int TO = 1;					// Timeout, seconds
	int load_threshold = G_LOAD_THRESHOLD_DEFAULT;
	unsigned play_bufferlen;			// Change bufferlen on play
	unsigned initval = 0;				// M_INIT, set all mem this value

	int pad = 1;					// 0: no pad, 1: pad last, 2: pad 0

	char* port = 0;				// 0 no server (inetd), else make a server
	char* host = 0;
};

using namespace std;
#include "acq-util.h"
#include "Buffer.h"

struct poptOption opt_table[] = {
	{ "sample-size", 'S', POPT_ARG_INT, &G::sample_size, 0,
			"bytes per sample [deprecated]"
	},
	{ "concurrent", 'c', POPT_ARG_INT, &G::concurrent, 0,
			"allow concurrent update"
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
	{ "minbufs", 'b', POPT_ARG_INT, &G::minbufs, 0,
			"minimum buffers : 4 is safe with large buffers, 2 possible for small shots"
	},
	{
	  "pad", 'n',  POPT_ARG_INT, &G::pad, 0,
	  	  	 "when set, pad to end of buffer"
	},
	{
	  "port", 'p', POPT_ARG_STRING, &G::port, 0, "server port 0: no tcp server (using inetd)"
	},
	{
	  "host", 'H', POPT_ARG_STRING, &G::host, 0, "server host 0: allow any host"
	},
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

enum RUN_MODE { M_NONE, M_FILL, M_LOAD, M_DUMP, M_INIT };

void set_playloop_length(int nsamples)
{
	char cmd[128];
	setKnob(-1, "/dev/acq400.0.knobs/dist_bufferlen", G::play_bufferlen? G::play_bufferlen: Buffer::bufferlen);
	sprintf(cmd, "set.site %d playloop_length %d %d", G::play_site, nsamples, G::mode);
	system(cmd);

	if (nsamples == 0){
		unsigned task_active = 1;
		unsigned timeout = 20;
		while (task_active){
			if (getKnob(G::play_site, "task_active", &task_active) != 1){
				fprintf(stderr, "ERROR: failed to read knob task_active");
				exit(1);
			}else if (task_active){
				if (--timeout == 0){
					fprintf(stderr, "ERROR: timeout waiting task_active to go idle");
					exit(1);
				}
				usleep(50000);
			}
		}
	}
}

int pad(int nsamples, int pad_samples)
{
	char* base = Buffer::the_buffers[0]->getBase();
	char* end = base + nsamples*G::sample_size;
	char* last = end - G::sample_size;

	nsamples += pad_samples;

	if (G::pad){
		if (G::pad == G_PAD_ZERO){
			last = new char[G::sample_size];
			memset(last, 0, G::sample_size);
		}
		pad_samples += G::play_bufferlen/G::sample_size;	// fill an extra buffer
		while(pad_samples--){
			memcpy(end, last, G::sample_size);
			end += G::sample_size;
		}
		if (G::pad == G_PAD_ZERO){
			delete [] last;
		}
	}
	return nsamples;
}

void do_soft_trigger() {
	setKnob(0, "soft_trig", "0");
	setKnob(0, "soft_trig", "1");
	setKnob(0, "soft_trig", "0");
}


int _load_pad(int nsamples)
{
#define MARK \
	if (G::verbose){\
		fprintf(stderr, "%d playbuffs %d residue %d padsam %d\n", __LINE__, playbuffs, residue, padsam);\
	}
	int playbuffs = (nsamples*G::sample_size)/G::play_bufferlen;
	int residue = (nsamples*G::sample_size)%G::play_bufferlen;
	int padsam = 0;

	if (G::verbose){
		fprintf(stderr, "nsamples:%d G::sample_size:%d BL:%d\n",
				nsamples, G::sample_size, G::play_bufferlen);
		fprintf(stderr, "nsamples:0x%x G::sample_size:0x%x BL:0x%x\n",
						nsamples, G::sample_size, G::play_bufferlen);
	}
	MARK;
	if (residue){
		padsam = (G::play_bufferlen - residue)/G::sample_size;
		playbuffs += 1;		/* partly into a buffer, round up */
		MARK;
	}
	if (playbuffs&1){
		/* PRI DMA MUST ping+pong, expand to even # buffers */
		playbuffs += 1;
		padsam += G::play_bufferlen/G::sample_size;
		MARK;
	}
	if (G::minbufs == 4 && playbuffs == 2){
		playbuffs += 2;
		padsam += 2 * G::play_bufferlen/G::sample_size;
	}

	if (padsam){

		MARK;
		nsamples = pad(nsamples, padsam);
	}
	if (G::verbose) fprintf(stderr, "return nsamples %d\n", nsamples);

	return nsamples;
}
void _load_concurrent() {
	const int bls = Buffer::bufferlen/G::sample_size;
	const int gss = G::sample_size;
	char* bp = Buffer::the_buffers[0]->getBase();



	int playloop_length = 0;
	int totsamples = 0;
	unsigned nsamples;
	enum TRIGGER_REQ {
		TR_first_time,
		TR_requested,
		TR_done,
		TR_done_update_length_pending
	} tr = TR_first_time;
	int play_load_blocks = G::load_threshold;

	while((nsamples = fread(bp, gss, play_load_blocks*bls, G::fp_in)) > 0){
		totsamples += nsamples;
		if (tr == TR_done){
			tr = TR_done_update_length_pending;
		}
		if (totsamples >= playloop_length + play_load_blocks*bls){
			set_playloop_length(playloop_length = totsamples);
			switch(tr){
			case TR_first_time:
				play_load_blocks = 1;
				tr = TR_requested;
				break;
			case TR_requested:
				do_soft_trigger(); // fall thru
			default:
				tr = TR_done;
			}
		}
		bp += nsamples*gss;
	}
	if (nsamples <= 0){
		syslog(LOG_DEBUG, "bb fread returned %d at totsamples:%d feof:%d ferror:%d", nsamples, totsamples, feof(G::fp_in), ferror(G::fp_in));
	}

	if (tr != TR_done){
		if (tr == TR_first_time){
			set_playloop_length(_load_pad(totsamples));
			usleep(100000);
		}
		if (tr < TR_done){
			do_soft_trigger();
		}
	}
}
#include <sys/select.h>

int _fread(void* buffer, size_t size, size_t nelems, FILE *fp)
{
	char* bp0 = (char*)buffer;
	char* bp = bp0;
	int fd = fileno(fp);
	int fd1 = fd+1;
	struct timespec pto;
	int nread = 0;
	int rc;
	int maxbytes = nelems*size;
	sigset_t  emptyset;
	fd_set exceptfds;
	fd_set readfds;

#define INIT_SEL do { 		\
	sigemptyset(&emptyset); \
	pto.tv_sec = G::TO; 	\
	pto.tv_nsec = 0;	\
	FD_ZERO(&exceptfds);	\
	FD_SET(fd, &exceptfds);	\
	FD_ZERO(&readfds);	\
	FD_SET(fd, &readfds);	\
	} while(0)

	for( ; bp - bp0 < maxbytes; bp += nread){
		INIT_SEL;
		rc = pselect(fd1, &readfds, NULL, &exceptfds, &pto, &emptyset);

		if (rc < 0){
			syslog(LOG_ERR, "ERROR: pselect() fail %d\n", errno);
			exit(1);
		}
		if (FD_ISSET(fd, &readfds)){
			nread = read(fd, bp, maxbytes - (bp-bp0));
			if (nread < 0){
				syslog(LOG_WARNING, "ERROR: read() fail %d at %u\n", errno, bp-bp0);
				exit(1);
			}else if (nread == 0){
				break;
			}
		}
		if (FD_ISSET(fd, &exceptfds)){
			syslog(LOG_WARNING, "WARNING: exception on fd");
		}
		if (rc == 0){
			if (bp - bp0 == 0){
				continue;
			}else{
				syslog(LOG_WARNING, "TIMEOUT");
				break;
			}
		}
	}
	nelems = (bp - bp0)/size;
	syslog(LOG_DEBUG, "_fread bp0:%p returns %d * %d = %08x\n", bp0, nelems, size, bp-bp0);
	return nelems;
}
int _load() {
	unsigned nsamples = _fread(Buffer::the_buffers[0]->getBase(),
			G::sample_size, G::max_samples, G::fp_in);

	syslog(LOG_DEBUG, "bb fread returned %d feof:%d ferror:%d errno:%d",
			nsamples, feof(G::fp_in), ferror(G::fp_in), ferror(G::fp_in)? errno: 0);
	if (ferror(G::fp_in)){
		syslog(LOG_DEBUG, "bb fread ERROR exit");
		exit(1);
	}
	return _load_pad(nsamples);
}

int _load_by_buffer() {
	int spb = G::play_bufferlen/G::sample_size;
	unsigned nsamples = 0;
	unsigned buf;
	for (buf = 0; buf < Buffer::nbuffers; ++buf){
		unsigned nread = _fread(Buffer::the_buffers[buf]->getBase(),
						G::sample_size, spb, G::fp_in);
		if (nread > 0){
			nsamples += nread;
		}else{
			if (ferror(G::fp_in)){
				syslog(LOG_DEBUG, "bb fread ERROR exit");
				exit(1);
			}
			fprintf(stderr, "load_pad buffers:%u nsamples:%u\n", buf+1, nsamples);
			return _load_pad(nsamples);
		}
	}
	syslog(LOG_DEBUG, "bb hit the buffers, going with buffers=%u samples=%u", buf, nsamples);
	return _load_pad(nsamples);
}


int fill() {
	int nsamples;
	if (G::play_bufferlen != Buffer::bufferlen){
		fprintf(stderr, "LOAD_BY_BUFFER: NEW\n");
		syslog(LOG_DEBUG, "LOAD_BY_BUFFER: NEW\n");
		nsamples = _load_by_buffer();
	}else{
		nsamples = _load();
	}
	printf("DONE %d\n", nsamples);
	return nsamples;
}
int load() {
	fprintf(stderr, "LOAD NEW\n");
	openlog("bb", LOG_PID, LOG_USER);
	set_playloop_length(0);



	if (G::concurrent){
		_load_concurrent();
	}else{
		set_playloop_length(fill());
	}

	return 0;
}

int init() {
	for (Buffer* buffer : Buffer::the_buffers){
		fprintf(stderr, "Buffer %d len:%d\n", buffer->ib(), buffer->bufferlen);
		unsigned* cursor = (unsigned*)buffer->getBase();
		unsigned* end = (unsigned*)buffer->getEnd();
		unsigned value = G::initval;

		while(cursor != end){
			*cursor++ = value;
		}
	}
	return 0;
}

int dump() {
	unsigned nsamples;
	getKnob(G::play_site, "playloop_length", &nsamples);

	fwrite(Buffer::the_buffers[0]->getBase(),
			G::sample_size, nsamples, G::fp_out);
	return 0;
}

/*
acq2106_112> grep ^050, /proc/driver/acq400/0/buffers
050,e1800000,0x21800000,0x400000,0
*/
unsigned getSpecificBufferlen(int ibuf)
{
	char cmd[128];
	sprintf(cmd, "grep ^%03d, /proc/driver/acq400/0/buffers", ibuf);
	unsigned bl = 0;
	FILE * pp = popen(cmd, "r");
	if (fscanf(pp, "%*d,%*x,%*x,%x,%*d", &bl) == 1){
		fprintf(stderr, "success bl:0x%08x\n", bl);
	}else{
		fprintf(stderr, "FAIL: %s\n", cmd);
	}
	pclose(pp);
	return bl;
}
#define MODPRAMS "/sys/module/acq420fmc/parameters/"
#define DFB	 "/dev/acq400.0.knobs/first_distributor_buffer"
#define BUFLEN	 MODPRAMS "bufferlen"
#define NBUF	 MODPRAMS "nbuffers"

#define PAGESZ	 4096
#define PAGEM    (PAGESZ-1)

void set_dist_awg(unsigned dist_s1)
{
	char cmd[80];
	snprintf(cmd, 80, "/etc/acq400/%u/AWG:DIST AWG", dist_s1);
	system(cmd);
}

RUN_MODE ui(int argc, const char** argv)
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	G::verbose 		= Env::getenv("VERBOSE", 0);
	G::load_threshold 	= Env::getenv("BB_LOAD_THRESHOLD", G_LOAD_THRESHOLD_DEFAULT);
	G::pad			= Env::getenv("BB_PAD",  G_PAD_LAST);

	getKnob(-1, NBUF,  &Buffer::nbuffers);
	getKnob(-1, DFB, 	&G::buffer0);
	getKnob(-1, BUFLEN, &Buffer::bufferlen);
	getKnob(-1, "/etc/acq400/0/dist_bufferlen_play", &G::play_bufferlen);

	int rc;

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		default:
			;
		}
	}

	const char* mode = poptGetArg(opt_context);
	RUN_MODE rm = M_NONE;

	fprintf(stderr, "%s: bl:%d\n", __FUNCTION__, Buffer::bufferlen);

	if (G::play_bufferlen > Buffer::bufferlen){
		fprintf(stderr, "ERROR play %d > buffer %d\n", G::play_bufferlen, Buffer::bufferlen);
		exit(1);
	}else if (G::play_bufferlen == 0){
		G::play_bufferlen = Buffer::bufferlen;
	}

	if (mode != 0){
		if (strcmp(mode, "load") == 0){
			rm = M_LOAD;
		}else if (strcmp(mode, "fill") == 0){
			rm = M_FILL;
		}else if (strcmp(mode, "init") == 0){
			const char* initval = poptGetArg(opt_context);
			G::initval = initval? strtoul(initval, 0, 0): 0;
			rm = M_INIT;
		}else if (strcmp(mode, "dump") == 0){
			Buffer::bufferlen = G::play_bufferlen;
			return M_DUMP;
		}else{
			fprintf(stderr, "ERROR bad pram \%s\"\n", mode);
			return rm;
		}
	}else{
		return rm;
	}

	setKnob(-1, "/dev/acq400.0.knobs/dist_bufferlen", Buffer::bufferlen);

	fprintf(stderr, "%s: bl:%d play:%d\n", __FUNCTION__, Buffer::bufferlen, G::play_bufferlen);

	Buffer::nbuffers -= G::buffer0;

	unsigned dist_s1 = 0;
	getKnob(0, "/etc/acq400/0/play0_ready", &dist_s1);

	if (dist_s1){
		set_dist_awg(dist_s1);
		unsigned playloop_maxlen;
		G::play_site = dist_s1;

		getKnob(0, "/etc/acq400/0/dssb", &G::sample_size);
		//fprintf(stderr, "s1:%d size:%d\n", dist_s1, G::sample_size);
		getKnob(dist_s1, "playloop_maxlen", &playloop_maxlen);


		if (playloop_maxlen){
			unsigned playloop_maxbytes = playloop_maxlen*G::sample_size;
			if (playloop_maxbytes < Buffer::nbuffers*Buffer::bufferlen){
				G::max_samples = playloop_maxlen;
			}
		}
	}
	if (G::max_samples == 0){
		G::max_samples = Buffer::nbuffers*Buffer::bufferlen/G::sample_size;
	}
	return rm;
}


int load_interpreter(FILE* fin, FILE* fout)
{
	close(0); dup(fileno(fin));
	close(1); dup(fileno(fout));
	close(2); dup(fileno(fout));
	return load();
}

int main(int argc, const char** argv)
{
	RUN_MODE rm = ui(argc, argv);
	BufferManager bm(getRoot(G::devnum), G::buffer0);
	switch(rm){
	case M_FILL:
		return fill() > 0? 0: -1;
	case M_LOAD:
		if (G::port){
			return tcp_server(G::host, G::port, load_interpreter);
		}else{
			return load();
		}
	case M_INIT:
		init();
	case M_NONE:
		fprintf(stderr, "bb --help for info\n");
		return 0;
	default:
		return dump();
	}
}


