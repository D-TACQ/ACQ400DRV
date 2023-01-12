/*
 * bbq_send_ai.cpp : transmit ai buffer data from buffers once reported full by Buffer Q.
 *
 *  Created on: 27 Jan 2019
 *      Author: pgm
 */




/* ------------------------------------------------------------------------- */
/* bbq_send_ao.cpp  D-TACQ ACQ400 FMC  DRIVER    "big buffer" : data sender
 * Project: ACQ420_FMC
 * Created: 27 Jan 2019  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2019 Peter Milne, D-TACQ Solutions Ltd         *
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



#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"

#include "File.h"


using namespace std;
#include "acq-util.h"
#include "Buffer.h"

#include "Socket.h"

#define SAMPLE_FOREVER	0


namespace G {
	int samples_per_packet  = 1;
	int use_udp = 1;
	int verbose = 0;
	int devnum = 0;
	unsigned buffer_data_bytes;
	unsigned sample_size_bytes;
	unsigned max_samples = SAMPLE_FOREVER;
	const char* rhost = 0;
	const char* rport = 0;
	Socket *sender;
	int stdout;			// FIN FS NACC
	int packets_per_buffer;
	const char* spad;
	unsigned spadlen;
};
struct poptOption opt_table[] = {
	{ "samples_per_packet", 'S', POPT_ARG_INT, &G::samples_per_packet, 0,
			"samples per packet (message in UDP, write in TCP)"
	},
	{ "use_udp", 'u', POPT_ARG_INT, &G::use_udp, 0,
			"send udp messages"
	},
	{ "max_samples", 'm', POPT_ARG_INT, &G::max_samples, 0,
			"max samples to send (0: infinity)"
	},
	{
	  "packets_per_buffer", 'p', POPT_ARG_INT, &G::packets_per_buffer, 0,
	  	  	 "send evenly spaced packets per buffer"
	},
	{
	  "stdout",   's', POPT_ARG_INT, &G::stdout, 0,
	  	  	 "slowmon FIN FS NACC"
	},
	{
	  "spad",     'S', POPT_ARG_STRING, &G::spad, 0,
	  	  	  "current spad condition. replace SPAD[2] with SPAD[0]-SPAD[0]n1, SPAD[3] with b"
	},
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

#define MODPRAMS "/sys/module/acq420fmc/parameters/"
#define DFB	 MODPRAMS "distributor_first_buffer"
#define BUFLEN	 MODPRAMS "bufferlen"
#define NBUF	 MODPRAMS "nbuffers"
#define SSB	 "/etc/acq400/0/ssb"
void ui(int argc, const char** argv)
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	unsigned distributor_first_buffer = 0;


	getKnob(-1, NBUF,  &Buffer::nbuffers);
	getKnob(-1, BUFLEN, &Buffer::bufferlen);
	getKnob(-1, DFB, 	&distributor_first_buffer);
	if (distributor_first_buffer){
		Buffer::nbuffers -= distributor_first_buffer;
	}
	getKnob(G::devnum, "bufferlen", &G::buffer_data_bytes);
	getKnob(-1, SSB, &G::sample_size_bytes);

	int rc;

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		default:
			;
		}
	}

	G::rhost = poptGetArg(opt_context);
	G::rport = poptGetArg(opt_context);

	if (G::verbose){
		fprintf(stderr, "bbq_send_ai\n");
		if (G::max_samples == SAMPLE_FOREVER){
			fprintf(stderr, " send samples FOREVER\n");
		}else{
			fprintf(stderr, " send samples %u\n", G::max_samples? G::max_samples:0xffffffff);
		}
		fprintf(stderr, " sample size: %d, samples_per_packet:%d, message size: %d\n",
				G::sample_size_bytes, G::samples_per_packet,
				G::sample_size_bytes*G::samples_per_packet);
		fprintf(stderr, " bytes per buffer:%d, messages per buffer:%d\n",
				G::buffer_data_bytes,
				G::buffer_data_bytes/(G::sample_size_bytes*G::samples_per_packet));
		fprintf(stderr, " send using %s to %s:%s\n", G::use_udp? "UDP": "TCP", G::rhost, G::rport);
	}

	if (G::stdout){
		G::sender = Socket::createIpSocket("stdout", 0, 0);
	}else{
		if (G::rhost == 0 || G::rport == 0){
			fprintf(stderr, "usage bbq_send_ai [opts] HOST PORT\n");
			exit(1);
		}
		G::sender = Socket::createIpSocket(G::use_udp? "udp": "tcp", G::rhost, G::rport);
	}
	if (G::spad){
		sscanf(G::spad, "1,%u,%*d", &G::spadlen);
	}
}

#define NLSPAD		4
#define LSPADLEN 	(NLSPAD*sizeof(unsigned))

char* instrument_spad(int ib, char* cursor, unsigned* local_spad)
{
	unsigned spad0m1 = local_spad[0];
	unsigned spad0   = ((unsigned*)(cursor+G::sample_size_bytes-4*sizeof(unsigned)))[0];
	unsigned spad1   = ((unsigned*)(cursor+G::sample_size_bytes-4*sizeof(unsigned)))[1];
	local_spad[0] = spad0;
	local_spad[1] = spad1;
	local_spad[2] = spad0 - spad0m1;
	local_spad[3] = ib;
	return (char*)local_spad;
}

void send(int ib)
{
	char* bp = Buffer::the_buffers[ib]->getBase();
	char* cursor = bp;

	if (G::packets_per_buffer > 0){
		int len = G::sample_size_bytes;
		int stride = G::buffer_data_bytes/G::packets_per_buffer;
		static unsigned local_spad[NLSPAD];

		for (int pkt = 0; pkt < G::packets_per_buffer; ++pkt, cursor += stride){
			for (int ii = 0; ii < G::samples_per_packet; ++ii){
				if (G::spad == 0){
					G::sender->send(cursor+ii*len, len);
				}else{
					G::sender->send(cursor+ii*len, len-LSPADLEN);
					G::sender->send(instrument_spad(ib, cursor+ii*len, local_spad), LSPADLEN);
				}
			}
		}
	}else{
		int len = G::sample_size_bytes*G::samples_per_packet;
		int imax = G::buffer_data_bytes/(len);

		for (int ii = 0; ii < imax; ++ii, cursor += len){
			G::sender->send(cursor, len);
		}
	}
}
int run(void)
{
	File bq("/dev/acq400.0.bq", "r");
	char bufnum[32];

	while(fgets(bufnum, 32, bq())){
		unsigned ib = strtoul(bufnum, 0, 10);
		if (G::verbose){
			fprintf(stderr, "%u\n", ib);
		}
		send(ib);
	}
	return 0;
}
int main(int argc, const char** argv)
{
	ui(argc, argv);
	BufferManager bm(getRoot(G::devnum));
	return run();
}

