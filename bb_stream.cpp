/* streaming DAC */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include "popt.h"

using namespace std;

#include "Buffer.h"
#include "Env.h"
#include "Knob.h"
#include "knobs.h"
#include "tcp_server.h"

#include "local.h"

class BufferReader;

namespace G {
	unsigned play_bufferlen;		// Change bufferlen on play
	char* port = 0;				// 0 no server (inetd), else make a server
	char* host = 0;
	int verbose = 0;
	const char *dev = "/dev/acq400.0.dac";
	int inetd_tcp_wait = 0;
	int auto_soft_trigger = false;
	int ab_swap;
	char* file_loop;
	BufferReader *reader;
	FILE* fp;
};

struct poptOption opt_table[] = {
	{
	  "port", 'p', POPT_ARG_STRING, &G::port, 0, "server port 0: no tcp server (using inetd)"
	},
	{
	  "host", 'H', POPT_ARG_STRING, &G::host, 0, "server host 0: allow any host"
	},
	{
	  "inetd_tcp_wait", 'T', POPT_ARG_INT, &G::inetd_tcp_wait, 0, "run from inetd stream wait"
	},
	{
          "dev",  'd', POPT_ARG_STRING, &G::dev, 0, "device"
	},
	{
          "auto_soft_trigger",  't', POPT_ARG_INT, &G::auto_soft_trigger, 0, "trigger auto"
	},
	{
	  "ab_swap", 'a', POPT_ARG_INT, &G::ab_swap, 0, "reverse buffer order"
	},
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	{
	  "file_loop", 'f', POPT_ARG_STRING, &G::file_loop, 0, "loop over this file"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

class BufferReader {
protected:
	FILE* fp;
public:
	BufferReader(FILE* _fp): fp(_fp) {}

	// read FULL buffers
	virtual int read_buffers(unsigned descriptors[], int ndesc) {
		int totbytes = 0;

		for (int id = 0; id < ndesc; ++id){
			char* bp = Buffer::the_buffers[0]->getBase() + descriptors[id]*Buffer::bufferlen;
			// @todo : ASSUMES G::play_bufferlen is available!
			int nb = fread(bp, 1, G::play_bufferlen, fp);
			if (nb <= 0){
				fprintf(stderr, "read_buffers fread returned %d\n", nb);
				return 0;
			}else{
				totbytes += nb;
			}
		}
		return totbytes;
	}
};

class LoopFileReader: public BufferReader {
public:
	LoopFileReader(const char* fname):
		BufferReader(fopen(fname, "r"))
	{
		if (fp == 0){
			fprintf(stderr, "ERROR failed to open file \"%s\"\n", fname);
			exit(1);
		}
	}

	virtual int read_buffers(unsigned descriptors[], int ndesc) {
		int totbytes = 0;

		for (int id = 0; id < ndesc; ++id){
			char* bp = Buffer::the_buffers[0]->getBase() + descriptors[id]*Buffer::bufferlen;
			unsigned nbuf = 0;
			int nb;

			for (int nb0 = 999; nbuf < G::play_bufferlen; nb0 = nb){
				nb = fread(bp+nbuf, 1, G::play_bufferlen-nbuf, fp);
				if (nb <= 0){
					if (nb0 <= 0){
						fprintf(stderr, "ERROR two strikes and out at %d %d %d\n", nbuf, nb0, nb);
						exit(1);
					}
					rewind(fp);
					continue;
				}
				nbuf += nb;
			}
			totbytes += nbuf;
		}
		return totbytes;
	}
};


class SUCC {
	int max_val;
	int min_val;
	int previous;

public:
	SUCC() : max_val(-1), min_val(-1), previous(-1) {

	}
	bool operator() (int next){
		if (next == -1){
			return false;
		}
		if (min_val == -1){
			min_val = next;
		}else if (next > max_val){
			max_val = next;
		}
		if (previous == -1){
			previous = next;
			return true;
		}else{
			if (next == previous+1){
				previous = next;
				return true;
			}else if (previous+1 > max_val && next==min_val){
				previous = next;
				return true;
			}else{
				previous = next;
				return false;
			}
		}
	}
};
int playloop() {
	unsigned descriptors[2];
	unsigned tmp;

	FILE* fp = fopen(G::dev, "a+");
	if (fp == 0){
		fprintf(stderr, "ERROR failed to open file\n");
		return -1;
	}
	setvbuf(fp, NULL, _IONBF, 0);

	if (G::verbose) fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);

	int nw = 0;
	bool soft_trigger_requested = G::auto_soft_trigger;
	SUCC succ;

	for (int totbuf = 0; ; totbuf += nw){
		if (G::verbose>1) fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
		int nr = fread(descriptors, sizeof(unsigned), 2, fp);
		if (nr == 2){
			bool status0 = succ(descriptors[0]);
			bool status1 = succ(descriptors[1]);

			if (G::verbose) printf("%s rd %03d %03d %s %s\n", __FUNCTION__,
					descriptors[0], descriptors[1], status0? "OK": "FAIL", status1? "OK":"FAIL");
		}else{
			printf("read returned %d\n", nr);
			return -1;
		}

		int nbytes = G::reader->read_buffers(descriptors, 2);
		if (nbytes <= 0){
			return nbytes;
		}

		if (soft_trigger_requested && totbuf >= G::auto_soft_trigger){
			Knob("/etc/acq400/0/soft_trigger").set(1);
			soft_trigger_requested = false;
		}
		if (G::ab_swap){
			SWAP(descriptors[0], descriptors[1], tmp);
		}

		nw = fwrite(descriptors, sizeof(unsigned), 2, fp);
		if (nw == 2){
			if (G::verbose>1) printf("%s wr %03d %03d\n", __FUNCTION__, descriptors[0], descriptors[1]);
		}else{
			printf("write returned %d\n", nw);
			return -1;
		}
	}
	return 0;
}

int playloop_redirect(FILE* fin, FILE* fout)
{
	close(0); dup(fileno(fin));
	close(1); dup(fileno(fout));
	return playloop();
}

#define MODPRAMS "/sys/module/acq420fmc/parameters/"
#define BUFLEN	 MODPRAMS "bufferlen"
#define NBUF	 MODPRAMS "nbuffers"

void set_dist_awg(unsigned dist_s1)
{
	char cmd[80];
	snprintf(cmd, 80, "/etc/acq400/%u/AWG:DIST AWG", dist_s1);
	system(cmd);
}

void ui(int argc, const char** argv)
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);

	int rc;
	unsigned dist_s1 = 0;

	get_local_env("/dev/shm/awg_settings");
	G::auto_soft_trigger = Env::getenv("SOFT_TRIGGER", 0);

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		default:
			;
		}
	}


	getKnob(-1, BUFLEN, &Buffer::bufferlen);
	getKnob(-1, "/etc/acq400/0/dist_bufferlen_play", &G::play_bufferlen);
	if (G::play_bufferlen == 0 || G::play_bufferlen > Buffer::bufferlen){
		G::play_bufferlen = Buffer::bufferlen;
	}else{
		if (G::play_bufferlen >= 4096 && G::play_bufferlen%4096 == 0){
			Buffer::bufferlen = G::play_bufferlen;
		}
	}
	setKnob(-1, "/dev/acq400.0.knobs/dist_bufferlen", G::play_bufferlen);
	getKnob(0, "/etc/acq400/0/play0_ready", &dist_s1);
	if (dist_s1){
		set_dist_awg(dist_s1);
	}else{
		fprintf(stderr, "ERROR: distributor not set");
		exit(-1);
	}
	if (G::file_loop){
		G::reader = new LoopFileReader(G::file_loop);
	}else{
		G::reader = new BufferReader(stdin);
	}
}
int main(int argc, const char* argv[]) {
	ui(argc, argv);
	BufferManager bm("/dev/acq400.0", 0);
	if (G::inetd_tcp_wait){
		return inetd_tcp_wait(playloop_redirect);
	}else if (G::port){
		return tcp_server(G::host, G::port, playloop_redirect);
	}else{
		return playloop();
	}
}
