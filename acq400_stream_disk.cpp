/*
 * acq400_stream_disk.cpp
 *
 *  Created on: 10 Feb 2015
 *      Author: pgm
 *
 *     usage: scq400_stream_disk NBUFFERS dest1 [dest2]
 *     Read full rate data on stdin
 *     farm to one or more file trees  destX/%03d/%02d[extension]
 *     we try to avoid too many files in one directory (max 100)
 *     eg
 *     /diskA/000/01.dat ..
 *     /diskA/000/99.dat
 *     /diskA/001/01.dat ..
 *     /diskA/001/99.dat ..
 *     /diskA/099/99.dat        : 100 x 100 x 1MB = 10GB
 *     .. run to the limit, now we have 1000 subdirs, maybe another level is
 *     .. needed, but most of the time we won't go this _large_ ?
 *     /diskA/999/99.dat       : 1000 x 100 x 1MB = 100GB
 *     Write data in 1MB files
 *     STOP after NBUFFERS x 1MB files
 *
 *     options:
 *     EXTENSION=ext   eg EXTENSION=.dat	default: none
 *     BUFFERLEN=len   eg BUFFERLEN=1024 	file length in decimal bytes
 *     FILESDIR=n      eg FILESDIR=100		number files per dir (max 100)
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>


#define BUFFERLEN	0x100000
#define FILESDIR	100

unsigned bufferlen = BUFFERLEN;
int verbose = 0;
const char* extension = "";
int filesdir = FILESDIR;
int concat;

int icat;
FILE* fp;

static void processBuffer(const char* outroot, int ibuf, short* buf, int nbuf){
	if (verbose){
		fprintf(stderr, "%02d\n", ibuf);
	}

	char fname[80];
	int cycle = ibuf/filesdir;
	int ibm = ibuf%filesdir;

	if (ibm == 0){
		sprintf(fname, "%s/%03d/", outroot, cycle);
		mkdir(fname, 0777);
	}
	sprintf(fname, "%s/%03d/%06d%s", outroot, cycle, ibuf, extension);

	if (icat == 0){
		fp = fopen(fname, "w");
		if (fp == 0){
			perror(fname);
			_exit(errno);
		}
	}


	fwrite(buf, sizeof(short), nbuf, fp);
	if (++icat >= concat){
		fclose(fp);
		icat = 0;
	}
}

void process(int nbuffers, int ndest, const char* dests[])
{
	unsigned nshorts = bufferlen/sizeof(short);
	short* buf = new short[nshorts];
	long ibuf = 0;

	while(fread(buf, sizeof(short), nshorts, stdin) == nshorts){
		for (int id = 0; id < ndest; ++id){
			processBuffer(dests[id], ibuf, buf, nshorts);
		}
		if (++ibuf > nbuffers){
			return;
		}
	}
	perror("fread fail");
	return;
}

int main(int argc, const char* argv[])
{
	int nbuffers;

	if (getenv("BUFFERLEN")) bufferlen = atoi(getenv("BUFFERLEN"));
	if (getenv("VERBOSE"))   verbose   = atoi(getenv("VERBOSE"));
	if (getenv("EXTENSION")) extension = getenv("EXTENSION");
	if (getenv("FILESDIR"))  filesdir  = atoi(getenv("FILESDIR"));
	if (getenv("CONCATFILES")) concat = atoi(getenv("CONCATFILES"));

	if (filesdir > FILESDIR) filesdir  = FILESDIR;
	if (argc < 3){
		fprintf(stderr, "USAGE: acq400_stream_disk NBUFFERS dest1 [dest2]\n");
		return -1;
	}else{
		nbuffers = atoi(argv[1]);
	}

	process(nbuffers, argc-2, &argv[2]);
}

