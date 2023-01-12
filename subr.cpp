/*
 * subr.cpp: read the subrate device.
 *
 *  Created on: 3 Jan 2022
 *      Author: pgm
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "popt.h"
//#include "acq-util.h"

using namespace std;

namespace G {
	int site;
	int usleep;
	unsigned nchan = 32;
	int verbose = 0;
	int priority = 0;
};
struct poptOption opt_table[] = {
	{
	  "usleep", 'u', POPT_ARG_INT, &G::usleep, 0, "sleep usec (default: no sleep)"
	},
	{
	  "nchan", 0, POPT_ARG_INT, &G::nchan, 0, "number of channels @@todo: autodetect"
	},
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	{
	  "sched_fifo", 0, POPT_ARG_INT, &G::priority, 0, "sched_fifo priority"
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
	G::site = atoi(poptGetArg(opt_context));
	/*
	if (G::priority){
		goRealTime(G::priority);
	}
	*/
}

int main(int argc, const char** argv)
{
	ui(argc, argv);
	unsigned* buf = new unsigned[G::nchan];

	char fname[80];
	snprintf(fname, 80, "/dev/acq400.%d.subr", G::site);
	FILE* fp = fopen(fname, "r");
	if (!fp){
		perror(fname);
		exit(1);
	}
	setvbuf(fp, NULL, _IONBF, 0);
	while(fread(buf, sizeof(unsigned), G::nchan, fp) == G::nchan){
		fwrite(buf, sizeof(unsigned), G::nchan, stdout);
		fflush(stdout);
		if (G::usleep){
			usleep(G::usleep);
		}
	}
}
