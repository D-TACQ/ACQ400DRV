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

#include "File.h"


using namespace std;

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
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
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

	G::rhost = poptGetArg(opt_context);
	G::rport = poptGetArg(opt_context);

}

int main(int argc, const char** argv)
{
	ui(argc, argv);
	
}

