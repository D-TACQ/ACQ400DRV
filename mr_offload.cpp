/*
 * mr_offload.cpp
 *
 *  Created on: 4 May 2020
 *      Author: pgm
 *
 *      offload to nfs mount:
 *      acq2106_182> /mnt/local/mr_offload | pv > /remote/shot4
 185MiB 0:00:07 [26.4MiB/s] [                          <=>                                                                                                                                          ]
acq2106_182> time /mnt/local/mr_offload > /remote/shot4
real	0m 6.28s
user	0m 0.00s
sys	0m 2.22s
 *
 *Faster:
 acq2106_182> nc -l -p 6677 -e /mnt/local/mr_offload
 [root@brotto ~]# nc -i 1 acq2106_182 6677 | pv > /data/shot2
 185MiB 0:00:04 [44.2MiB/s] [       <=>

[root@brotto ~]# nc -i 0.1 acq2106_182 6677 | pv > /data/shot2
 185MiB 0:00:03 [  48MiB/s] [      <=>


 Demux time:
 [peter@andros EPICS]$ camonitor acq2106_182:MODE:TRANS_ACT:STATE acq2106_182:MODE:TRANS_ACT:POST
acq2106_182:MODE:TRANS_ACT:STATE 2020-05-04 22:00:55.889770 IDLE
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:00:55.891951 2000000
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:38.355773 0
acq2106_182:MODE:TRANS_ACT:STATE 2020-05-04 22:01:38.355918 ARM
acq2106_182:MODE:TRANS_ACT:STATE 2020-05-04 22:01:39.455395 RUN_POST
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:39.459934 43648
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:39.496949 218240
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:40.598544 349184
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:40.600690 480128
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:41.700206 1134848
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:42.802288 1789568
acq2106_182:MODE:TRANS_ACT:STATE 2020-05-04 22:01:42.802395 POST_PROCESS
acq2106_182:MODE:TRANS_ACT:POST 2020-05-04 22:01:42.804388 2000000
acq2106_182:MODE:TRANS_ACT:STATE 2020-05-04 22:01:48.394864 CLEANUP
acq2106_182:MODE:TRANS_ACT:STATE 2020-05-04 22:01:50.493408 IDLE

=> 7s max.

ie 10s until data.. for 2M samples, 192MB.
 */

#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"
#include "Env.h"
#include "popt.h"
#include <stdio.h>

#include <assert.h>

#define NCHAN 48

#define BUFLEN 0x400000
#define FOUT	stdout

/** mr_offload: offload ALL data in one BLOB, comprising 3 sections:
 * mr_offload header;
 * char decims[nsamples];     // array with decimation factor in force at each sample.
 * short chdata[NCHAN][NSAM]; // 2D array (or, concatenation of 1D arrays, raw channel data)
 */
struct mr_offload {
	unsigned int shot;	// shot number, set automatically by the box
	unsigned long long TAI;	// time in nsec at start trigger
	float DT;		// sample clock period, nsec
	int nsam;     		// total samples in the shot eg max 2M
	int nchan;    		// number of channels, usually 48
	float ESLO[NCHAN];      // SLOPE,
	float EOFF[NCHAN];      // OFFSET   volts[ch][sam] = raw[ch][sam] * ESLO[ch] + EOFF[ch]
};
/*
	char decims[nsamples];    // array with decimation factor in force at each sample.
	                         // T[sam] = T[sam-1] + DT*decims[sam-1]
	short chdata[NCHAN][NSAM]; // 2D array (or, concatenation of 1D arrays, raw channel data)
*/

const char* G_root = ".";
int G_verbose;

struct poptOption opt_table[] = {
	{ "output", 	'o', POPT_ARG_STRING, 	&G_root, 	0, "output to directory [./]" 	},
	{ "show_headers", 's', POPT_ARG_NONE, 	0, 	      's', "dump header files"        	},
	{ "verbose", 	'v', POPT_ARG_INT, 	&G_verbose, 	0, "set verbosity" 		},
	POPT_AUTOHELP
	POPT_TABLEEND
};


void cat(const char* fn, void* buf, int len)
{
	FILE *fp = fopen(fn, "r");
	int nread;
	int nwrite = 0;

	if (fp == 0){
		fprintf(stderr, "ERROR, failed to open \"%s\"\n", fn);
		exit(1);
	}
	while((nread = fread(buf, 1, len, fp)) > 0){
		nwrite += fwrite(buf, 1, nread, FOUT);
	}
	char speed = '\0';
	while(nwrite < len){
		nwrite += fwrite(&speed, 1, 1, FOUT);
	}
}



namespace G {
	int nsam;
}

int dump_header(int ii, const char* hdr){
	FILE* fp = strcmp(hdr, "-") == 0? stdin: fopen(hdr, "r");
	if (fp==0){
		perror(hdr);
		exit(1);
	}
	struct mr_offload h;

	if (fread(&h, sizeof(struct mr_offload), 1, fp) != 1){
		perror("fread");
		exit(1);
	}
	if (G_verbose){
		printf("%4d,%llu,%.0f,%7d,%d,%s\n", h.shot, h.TAI, h.DT, h.nsam, h.nchan, hdr);
		if (G_verbose > 1){
			printf("ESLO:");
			for (int ii = 0; ii < NCHAN; ++ii){
				printf("%7.5f%c", h.ESLO[ii], ii+1==NCHAN? '\n': ',');
			}
			printf("EOFF:");
			for (int ii = 0; ii < NCHAN; ++ii){
				printf("%7.5f%c", h.EOFF[ii], ii+1==NCHAN? '\n': ',');
			}
		}
	}
	strcmp(hdr, "-") == 0 || fclose(fp);
	return 0;
}
int dump_headers(const char** hdrfiles)
{
	const char* hdr;
	int ii;

	for (ii = 0; (hdr = hdrfiles[ii]); ++ii){
		dump_header(ii, hdr);
	}
	return 0;
}
const char* init(int argc, const char** argv) {
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);

	int rc;
	bool _dump_headers = false;
	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		case 's':
			_dump_headers = true;
			break;
		default:
			;
		}
	}

	if (_dump_headers){
	        exit(dump_headers(poptGetArgs(opt_context)));
	}
	
	return poptGetArg(opt_context);
}

struct mr_offload mro = {};

int get_nsam(void)
{
        FILE *pp = popen("get.site 0 TRANS_ACT:POST", "r");

        int post = 0;
        fscanf(pp, "TRANS_ACT:POST %d", &post);
        pclose(pp);
	return post;
}

void get_cal()
{
	bool read_ok = false;
	FILE* fp = fopen("/dev/shm/calblob", "r");
	if (fp){
		read_ok =
		    fread(mro.ESLO, sizeof(float), NCHAN, fp) == NCHAN &&
		    fread(mro.EOFF, sizeof(float), NCHAN, fp) == NCHAN;

		fclose(fp);
	}
	if (!read_ok){
		for (int ii = 0; ii < NCHAN; ++ii){
			mro.ESLO[ii] = 3.0517578e-5;
		}
	}
}
void offload_header() {
	unsigned tai, tai_vernier;

	getKnob(0, "/dev/acq400.1.knobs/shot", &mro.shot);
	getKnob(0, "/dev/acq400.0.knobs/wr_tai_trg", &tai_vernier, "0x%x");
	getKnob(0, "/dev/acq400.0.knobs/wr_tai_cur", &tai);		// @@TODO : should be time at TRG time ..0
	mro.TAI = tai;
	mro.TAI <<=32;
	mro.TAI = tai_vernier;
	mro.DT = 25;
	mro.nsam = get_nsam();
	mro.nchan = NCHAN;
	get_cal();
	fwrite(&mro, sizeof(struct mr_offload), 1, stdout);
}

int offload(void) {
	offload_header();
	void* buf = new short* [mro.nsam];
	cat("/dev/shm/decims", buf, mro.nsam);
	char fname[80];

	int maxsite = NCHAN/8;
	for (int site = 1; site <= maxsite; ++site){
		for (int ch = 1; ch <= 8; ++ch){
			sprintf(fname, "/dev/acq400/data/%d/%02d", site, ch);
			cat(fname, buf, mro.nsam*sizeof(short));
		}
	}
	return 0;
}

int writebin(const char* fmt, const char* root, const char* uutname, int shot, void* data, int len)
{
	char fname[80];

	sprintf(fname, fmt, root, uutname, shot);
	FILE *fp = fopen(fname, "w");
	assert(fp);
	fwrite(data, len, 1, fp);
	fclose(fp);
	return 0;
}

int client(const char* uutname, FILE* in) {
	mr_offload header;

	fread(&header, sizeof(mr_offload), 1, in);

	writebin("%s/%s.%d.hdr", G_root, uutname, header.shot, &header, sizeof(mr_offload));

	char* decims = new char[header.nsam];
	fread(decims, sizeof(char), header.nsam, in);

	writebin("%s/%s.%d.dec", G_root, uutname, header.shot, decims, header.nsam);

	const int len = header.nchan * header.nsam;
	short* payload = new short[len];
	fread(payload, sizeof(short), len, in);

	writebin("%s/%s.%d.dat", G_root, uutname, header.shot, payload, sizeof(short)*len);

	return 0;
}

#include "connect_to.h"
int client(const char* uutname, const char* port)
{
	return client(uutname, connect_to_stream(uutname, port));
}


int main(int argc, const char** argv)
{
	const char* uutname = init(argc, argv);
	if (uutname != 0 && strcmp(uutname, "server") != 0){
		const char* colon = index(uutname, ':');
		if (colon){
			char* _uutname = new char[colon-uutname+1];
			strncpy(_uutname, uutname, colon-uutname);
			_uutname[colon-uutname] = '\0';
			const char* port = colon+1;
			return client(_uutname, port);
		}else{
			return client(uutname, stdin);
		}
	}else{
		return offload();
	}
	return 0;
}
