/* ------------------------------------------------------------------------- */
/* acq400_stream.cpp  		                     	 		     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 pgm, D-TACQ Solutions Ltd                            *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Mar 31, 2013                                                *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * acq400_stream.cpp
 *
 *  Created on: Mar 31, 2013
 *      Author: pgm
 *
 * - Usage
 * - No args: stream and copy to stdout eg the 4210 service
 * - --null-copy : no copy
 * - --pre/--post : pre/post transient BOS - data left in KBUFS
 * -- --pre=0 : easy, first N KBUFS
 * -- --pre!=0 : need to find the cursor.
 * - --demux : controls post shot demux
 * - --oversampling : controls sub-rate streaming (EPICS feed).
 *
 *
 * Dropped: copy/demux (SOS mode).
 *
 * Demux: has to be done in-situ in the kbufs.
 * Then allow an overlay VFS to extract the data.
 * Demux --pre==0 : demux in situ starting at ibuf=0
 * Demux --pre!-0 : demux and write back to ibuf=0
 * Easy to read from linear mapped buffers. Copy to local, DMA back
 * Driver provides a pair of temp buffers?. Allow ping/pong copy back.
 *
 * All the kbufs are mapped to a linear thing: this makes it easy for app-code.
 * Less easy for kernel code ..
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

#include <cstdarg>

#include <sched.h>

//#define BUFFER_IDENT 6
#define VERID	"B1051"

#define NCHAN	4

#include <semaphore.h>
#include <syslog.h>

#include <sys/statvfs.h>

#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"
#include "acq-util.h"
#include "ES.h"
#include "Knob.h"

#include <bitset>
#include "hex_char_to_bin.h"

#define _PFN	__PRETTY_FUNCTION__

#define STDOUT	1



using namespace std;
int timespec_subtract (timespec *result, timespec *x, timespec *y) {
	const int nsps = 1000000000;
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_nsec < y->tv_nsec) {
		int nsec = (y->tv_nsec - x->tv_nsec) / 1000000 + 1;
		y->tv_nsec -= nsps * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_nsec - y->tv_nsec > nsps) {
		int nsec = (x->tv_nsec - y->tv_nsec) / nsps;
		y->tv_nsec += nsps * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait. tv_nsec is certainly positive. */
	if (result){
		result->tv_sec = x->tv_sec - y->tv_sec;
		result->tv_nsec = x->tv_nsec - y->tv_nsec;
	}
	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}


#define BM_NOT_SPECIFIED	'\0'
#define BM_NULL			'n'
#define BM_RAW			'r'	/* no demux */
#define BM_DEMUX 		'h'
#define BM_TEST			't'
#define BM_PREPOST 		'P'

#define SM_STREAM    		'S'
#define SM_TRANSIENT 		'T'

/* all globals in one namespace : G */
namespace G {
	unsigned nchan = NCHAN;
	unsigned nchan_ES;
	unsigned wordsize = 2;		/** choice sizeof(short) or sizeof(int) */
	#define FULL_MASK 0xffffffff
	unsigned mask = FULL_MASK;	/** mask data with this value */
	unsigned m1 = 0;			/** start masking from here */
	int oversampling = 0;
	int devnum = 0;
	const char* script_runs_on_completion = 0;
	const char* aggregator_sites = 0;
	unsigned nsam = 4096;

	int control_handle;

	int pre;
	int post;
	int demux;

	int buffer_mode = BM_NOT_SPECIFIED;
	int stream_mode = SM_STREAM;

	bool soft_trigger;
	sem_t* aggsem;
	char* aggsem_name;
	char* state_file;
	bool show_es;
	FILE* state_fp;
	char* pre_demux_script;
	int show_first_sample;
	int es_diagnostic = getenv_default("ES_DIAGNOSTIC");
	int report_es;
	int nsites;
	int the_sites[6];
	char* progress;
	int corner_turn_delay;	// test pre/post caps on corner turn, buffers
	int exit_on_trigger_fail;

	char* subset;
	char* sum;

	bool null_copy;
	bool fillk;				// fill output scaled by 1k
	int is_spy;				// spy task runs independently of main capture
	int show_events;
	bool double_up;				// channel data appears 2 in a row (ACQ480/4)
	unsigned naxi;				// 0: none, 1=1 channel, 2= 2channels
	int stream_sob_sig;            		// insert start of buffer signature
	unsigned find_event_mask = 0x1;		// 1: EV0, 2:EV1, 3:EV0|EV1
	char* multi_event;			// multi-event definintion [pre,]post
	int live_instance = 1;			// clear if live_instance NOT required
};



int verbose = getenv_default("VERBOSE");
unsigned nb_cat =1;	/* number of buffers to concatenate */

static unsigned get_nchan_ES(unsigned nchan);

unsigned sample_size() {
	return G::nchan * G::wordsize;
}

unsigned es_size() {
	return G::nchan_ES * G::wordsize;
}

unsigned s2b(unsigned samples) {
	return samples*sample_size();
}
unsigned b2s(unsigned bytes) {
	return bytes/sample_size();
}



static int createOutfile(const char* fname) {
	int fd = open(fname,
			O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRGRP|S_IROTH);
	if (fd < 0){
		perror(fname);
		exit(1);
	}
	return fd;
}

#include "Buffer.h"

#define FMT_OUT_ROOT		"/dev/shm/AI.%d.wf"
#define FMT_OUT_ROOT_NEW	"/dev/shm/AI.%d.new"
#define FMT_OUT_ROOT_OLD	"/dev/shm/AI.%d.old"

enum DemuxBufferType {
	DB_REGULAR,
	DB_2D_REGULAR,		// 2DMA, single sample
	DB_DOUBLE,		// double sample: acq480-4
	DB_2D_DOUBLE		// double sample, double AXI, acq480x80
};

class DemuxBufferCommon: public Buffer {

protected:
	const unsigned nchan;
	const unsigned nchan_es;
	AbstractES& evX;


public:
	DemuxBufferCommon(Buffer* cpy): Buffer(cpy),
		nchan(G::nchan), nchan_es(G::nchan_ES),
		evX(*AbstractES::evX_instance())
	{}
	static int demux_size;
	static int verbose;
	static int show_es;
};

unsigned get_nchan_ES(unsigned nchan)
/* get number of channels in ES. We assume same module in each agg site
 * duplicates logic epics load.records load_rgm()
 */
{

	unsigned nchan_es = nchan;

	if (!ISACQ480()){
		FILE* fp = fopen("/var/log/run0.log", "r");
		if (fp){
			int agg_site;

			if (fscanf(fp, "%*s %d", &agg_site) == 1){
				unsigned active_chan;
				unsigned data32;
				if (Knob(agg_site, "active_chan").get(&active_chan) == 1 &&
				    Knob(agg_site, "data32").get(&data32) == 1){
					unsigned nlongs = active_chan/(data32? 1: 2);
					unsigned esp = nlongs >= 8? 1: nlongs >= 4? 2: 4;
					nchan_es *= esp;
				}
			}
			fclose(fp);
		}
	}
	syslog(LOG_DEBUG, "%d  %s nchan:%u nchan_es:%u\n", getpid(), __FUNCTION__,
				nchan, nchan_es);
	return nchan_es;
}


int DemuxBufferCommon::demux_size = ::getenv_default("DemuxBufferSize");
int DemuxBufferCommon::verbose = ::getenv_default("DemuxBufferVerbose");
int DemuxBufferCommon::show_es = ::getenv_default("DemuxBufferShowES");

template <class T, DemuxBufferType N>
class DemuxBuffer: public DemuxBufferCommon {
private:
	unsigned* mask;

	unsigned nsam;
	static T** dddata;
	static T** ddcursors;

	bool data_fits_buffer;
	char** new_names;
	char** old_names;
	char OUT_ROOT[128];
	char OUT_ROOT_NEW[128];
	char OUT_ROOT_OLD[128];
	char CLEAN_COMMAND[160];
	char FIN_FILE[160];

	static u32 ID_MASK;

	static int startchan;



	int lchan(int ichan){
		return ichan+1;
	}
	static void init() {
		if (ID_MASK) return;

		if (getenv("NOSID")){
			ID_MASK = 0x1f;
			fprintf(stderr, "ID_MASK set %02x\n", ID_MASK);
		}else{
			ID_MASK = 0xff;
		}
	}
	void _make_names(const char* root, char** names){
		char buf[160];

		for (unsigned ic = 0; ic < nchan; ++ic){
			int len = sprintf(buf, "%s/CH%02d", root, lchan(ic));
			names[ic] = new char[len+1];
			strcpy(names[ic], buf);
		}
	}
	void make_names() {
		sprintf(OUT_ROOT, FMT_OUT_ROOT, G::devnum);
		sprintf(OUT_ROOT_NEW, FMT_OUT_ROOT_NEW, G::devnum);
		sprintf(OUT_ROOT_OLD, FMT_OUT_ROOT_OLD, G::devnum);
		sprintf(FIN_FILE,   "%s.fin", OUT_ROOT);

		_make_names(OUT_ROOT_NEW, new_names = new char* [nchan]);
		_make_names(OUT_ROOT_OLD, old_names = new char* [nchan]);
	}
	void start() {
		mkdir(OUT_ROOT_NEW, 0777);
		for (unsigned ic = 0; ic < nchan; ++ic){
			ddcursors[ic] =	dddata[ic];
		}
		startchan = 0;
	}
	void clean_old() {
		for (unsigned ic = 0; ic < nchan; ++ic){
			unlink(old_names[ic]);
		}
		rmdir(OUT_ROOT_OLD);
	}
	void fin_cmd() {
		FILE *fp = fopen(FIN_FILE, "w");
		time_t now;

		time(&now);
		fprintf(fp, "%s\n", ctime(&now));
		fclose(fp);
	}
	void finish() {
		clean_old();
		rename(OUT_ROOT, OUT_ROOT_OLD);
		rename(OUT_ROOT_NEW, OUT_ROOT);
		fin_cmd();
		if (G::script_runs_on_completion != 0){
			system(G::script_runs_on_completion);
		}
	}
	static unsigned ch_id(T data)
	{
		return data&ID_MASK;
	}

	void dump(T* src, int nwords){
		char cmd[80];
		sprintf(cmd, "hexdump -ve '%d/%d \"%%08x \" \"\\n\" '",
							nchan, sizeof(T));
		FILE *pp = popen(cmd, "w");
		fwrite(src, sizeof(T), nwords, pp);
		pclose(pp);
	}
	/* NB: this is an out-of-place demux, source is unchanged */
	bool demux(bool start, int start_off, int len);

	bool writeChan(int ic){
		FILE* fp = fopen(new_names[ic], "w");
		if (fp ==0){
			perror(new_names[ic]);
			return true;
		}
		int nelems = ddcursors[ic]-dddata[ic];
		int nwrite = fwrite(dddata[ic], sizeof(T), nelems, fp);
		fclose(fp);
		if (nwrite != nelems || (verbose&&ic==0) || verbose>1){
			fprintf(stderr, "DemuxBuffer::writeChan(%s) ic:%d %d %d %s\n",
					new_names[ic], ic, nwrite, nelems, nwrite!=nelems? "ERROR":"");
		}
		return false;
	}


public:

	virtual int writeBuffer(int out_fd, int b_opts) {
		int demux_len = demux_size? demux_size: buffer_len;
		if ((b_opts&BO_START) != 0){
			start();
		}
		demux((b_opts&BO_START),0, demux_len);
		if ((b_opts&BO_FINISH) != 0){
			for (unsigned ic = 0; ic < nchan; ++ic){
				if (writeChan(ic)){
					// links take out from under
					return -1;
				}
			}
			finish();
		}
		return demux_len;
	}
	virtual int writeBuffer(int out_fd, int b_opts, int start_off, int len)
	{
		if ((b_opts&BO_START) != 0){
			start();
		}
		if (verbose) fprintf(stderr, "DemuxBuffer::writeBuffer %s%s %p + 0x%08x + %d %p..%p\n",
				(b_opts&BO_START)? "S":"", (b_opts&BO_FINISH)? "F":"",
				pdata, start_off, len, pdata+start_off, pdata+start_off+len);

		demux((b_opts&BO_START), start_off, len);
		if ((b_opts&BO_FINISH) != 0){
			for (unsigned ic = 0; ic < nchan; ++ic){
				if (writeChan(ic)){
					if (verbose) fprintf(stderr, "writeChan() fail\n");
					// links take out from under
					return -1;
				}
			}
			finish();
		}
		return buffer_len;
	}

	DemuxBuffer(Buffer* cpy, unsigned _mask) :
		DemuxBufferCommon(cpy)
	{
		if (verbose) fprintf(stderr, "DemuxBuffer T=%d N=%u\n", sizeof(T), N);

		nsam = buffer_len/sizeof(T)/G::nchan;
		init();
		mask = new unsigned[nchan];
		for (unsigned ic = 0; ic < nchan; ++ic){
			mask[ic] = ic < G::m1? FULL_MASK: _mask;
		}
		if (dddata == 0){
			dddata = new T*[nchan];
			ddcursors = new T*[nchan];
			for (unsigned ic = 0; ic < nchan; ++ic){
				ddcursors[ic] =	dddata[ic] = new T[nsam*nb_cat];
			}
		}

		make_names();

		data_fits_buffer = nsam*sizeof(T)*nchan == buffer_len;
		if (verbose>2){
			fprintf(stderr, "nsam:%d _buffer_len:%d data_fits_buffer? %s\n",
					nsam, buffer_len, data_fits_buffer? "YES": "NO");
		}
	}

	virtual unsigned getItem(int ii) {
		T* src = reinterpret_cast<T*>(pdata);
		return src[ii];
	}
	virtual unsigned getSizeofItem() { return sizeof(T); }
};





template <class T, DemuxBufferType N>
bool DemuxBuffer<T, N>::demux(bool start, int start_off, int len) {
	T* src1 = reinterpret_cast<T*>(pdata+start_off);
	T* src = reinterpret_cast<T*>(pdata+start_off);
	int Tlen = len/sizeof(T);
	int isam = 0;

	if (verbose > 1) fprintf(stderr, "demux() start_off:%08x src:%p\n",
			start_off, src);

	if (verbose > 1 && start && ch_id(src[0]) != 0x00){
		fprintf(stderr, "handling misalign at [0] %08x data_fits_buffer:%d\n",
				src[0], data_fits_buffer);
	}

	if (sizeof(T)==4 && start && !data_fits_buffer){
		/* search for start point - site 1 */
		for (; !(ch_id(src[0]) == ch_id(0x20) &&
				ch_id(src[1]) == ch_id(0x21) &&
				ch_id(src[2]) == ch_id(0x22) &&
				ch_id(src[3]) == ch_id(0x23)    ); ++src){
			if (src - src1 > 256){
				if (verbose > 1){
					dump(src1, src-src1);
				}
				fprintf(stderr, "failed to channel align\n");
				return true;
			}
		}
	}

	int startoff = (src - src1);

	if (verbose && startoff){
		fprintf(stderr, "realign: %p -> %p %d %d\n",
				src1, src, src-src1, startoff);
	}
	if (verbose && startchan != 0){
		fprintf(stderr, "start:%d startchan:%d data[0]:%08x\n",
				start, startchan, *src);
	}
	unsigned ichan = startchan;
	/* run to the end of buffer. nsam could be rounded down,
	 * so do not use it.
	 */
	if (verbose) fprintf(stderr, "%s will %s skip ES\n", _PFN, show_es? "NOT":"");

	for (isam = startoff/nchan; true; ++isam, ichan = 0){
		if (!show_es && evX.isES(reinterpret_cast<unsigned*>(src))){
			if (verbose) fprintf(stderr, "skip ES\n");
			src += nchan_es;
		}
		for (; ichan < nchan; ++ichan){
			*ddcursors[ichan]++ = (*src++)&mask[ichan];

			if (src-src1 >= Tlen){
				if (verbose){
					fprintf(stderr,
							"demux() END buf ch:%d src:%p len:%d\n",
							ichan, src,  ddcursors[ichan]-dddata[ichan]);
				}
				if (++ichan >= nchan) ichan = 0;
				startchan = ichan;

				if (verbose && startchan != 0){
					fprintf(stderr,
							"demux() END buf startchan:%d\n", startchan);
				}
				return false;
			}
		}
	}
	/* does not happen */
	return true;
}

template<class T, DemuxBufferType N> unsigned DemuxBuffer<T, N>::ID_MASK;
template<class T, DemuxBufferType N> int DemuxBuffer<T, N>::startchan;
template<class T, DemuxBufferType N> T** DemuxBuffer<T, N>::dddata;
template<class T, DemuxBufferType N> T** DemuxBuffer<T, N>::ddcursors;


template <>
bool DemuxBuffer<short, DB_REGULAR>::demux(bool start, int start_off, int len) {
	short* src1 = reinterpret_cast<short*>(pdata+start_off);
	short* src = reinterpret_cast<short*>(pdata+start_off);
	int shortlen = len/sizeof(short);
	int isam = 0;

	if (verbose > 1) fprintf(stderr, "demux() start_off:%08x src:%p\n",
			start_off, src);

	if (verbose > 1 && start && ch_id(src[0]) != 0x00){
		fprintf(stderr, "handling misalign at [0] %08x data_fits_buffer:%d\n",
				src[0], data_fits_buffer);
	}

	int startoff = 0;

	if (verbose && startchan != 0){
		fprintf(stderr, "start:%d startchan:%d data[0]:%08x\n",
				start, startchan, *src);
	}
	unsigned ichan = startchan;
	/* run to the end of buffer. nsam could be rounded down,
	 * so do not use it.
	 */
	if (verbose) fprintf(stderr, "%s will %s skip ES\n", _PFN, show_es? "NOT":"");

	for (isam = startoff/nchan; true; ++isam, ichan = 0){
		if (!show_es && evX.isES(reinterpret_cast<unsigned*>(src))){
			if (verbose) fprintf(stderr, "skip ES\n");
			src += nchan_es;
		}
		for (; ichan < nchan; ++ichan){
			*ddcursors[ichan]++ = (*src++)&mask[ichan];

			if (src-src1 >= shortlen){
				if (verbose){
					fprintf(stderr,
							"demux() END buf ch:%d src:%p len:%d\n",
							ichan, src,  ddcursors[ichan]-dddata[ichan]);
				}
				if (++ichan >= nchan) ichan = 0;
				startchan = ichan;

				if (verbose && startchan != 0){
					fprintf(stderr,
							"demux() END buf startchan:%d\n", startchan);
				}
				return false;
			}
		}
	}
	/* does not happen */
	return true;
}

template <>
bool DemuxBuffer<short, DB_DOUBLE>::demux(bool start, int start_off, int len) {
	short* src1 = reinterpret_cast<short*>(pdata+start_off);
	short* src = reinterpret_cast<short*>(pdata+start_off);
	int shortlen = len/sizeof(short);
	int isam = 0;

	if (verbose > 1) fprintf(stderr, "demux() start_off:%08x src:%p\n",
			start_off, src);

	if (verbose > 1 && start && ch_id(src[0]) != 0x00){
		fprintf(stderr, "handling misalign at [0] %08x data_fits_buffer:%d\n",
				src[0], data_fits_buffer);
	}

	int startoff = 0;

	if (verbose && startchan != 0){
		fprintf(stderr, "start:%d startchan:%d data[0]:%08x\n",
				start, startchan, *src);
	}
	unsigned ichan = startchan;
	/* run to the end of buffer. nsam could be rounded down,
	 * so do not use it.
	 */
	if (verbose) fprintf(stderr, "%s will %s skip ES\n", _PFN, show_es? "NOT":"");

	for (isam = startoff/nchan; true; ++isam, ichan = 0){
		if (!show_es && evX.isES(reinterpret_cast<unsigned*>(src))){
			if (verbose) fprintf(stderr, "skip ES\n");
			src += nchan_es;
		}
		for (; ichan < nchan; ++ichan){
			*ddcursors[ichan]++ = (*src++)&mask[ichan];
			*ddcursors[ichan]++ = (*src++)&mask[ichan];

			if (src-src1 >= shortlen){
				if (verbose){
					fprintf(stderr,
							"demux() END buf ch:%d src:%p len:%d\n",
							ichan, src,  ddcursors[ichan]-dddata[ichan]);
				}
				if (++ichan >= nchan) ichan = 0;
				startchan = ichan;

				if (verbose && startchan != 0){
					fprintf(stderr,
							"demux() END buf startchan:%d\n", startchan);
				}
				return false;
			}
		}
	}
	/* does not happen */
	return true;
}


/* DB_2D_REGULAR ..  6 x 8 channels
 * DMA0: Sites 1,3,5  # SITE1 => CH01->CH08
 * DMA1: Sites 2,4,6  # SITE2 => CH25->CH32
 *
 * LUTs index from zero, for each offset per DMA block.
 */

const u8 DB_2D_R_MEM_PIN_LUT_48_DMA0[] = {
	[ 0] =  0, [ 1] =  1, [ 2] =  2, [ 3] =  3, [ 4] =  4, [ 5] =  5, [ 6] =  6, [ 7] =  7,
	[ 8] = 16, [ 9] = 17, [10] = 18, [11] = 19, [12] = 20, [13] = 21, [14] = 22, [15] = 23,
	[16] = 32, [17] = 33, [18] = 34, [19] = 35, [20] = 36, [21] = 37, [22] = 38, [23] = 39,
};
const u8 DB_2D_R_MEM_PIN_LUT_48_DMA1[] = {
	[ 0] =  8, [ 1] =  9, [ 2] = 10, [ 3] = 11, [ 4] = 12, [ 5] = 13, [ 6] = 14, [ 7] = 15,
	[ 8] = 24, [ 9] = 25, [10] = 26, [11] = 27, [12] = 28, [13] = 29, [14] = 30, [15] = 31,
	[16] = 40, [17] = 41, [18] = 42, [19] = 43, [20] = 44, [21] = 45, [22] = 46, [23] = 47,
};


template <>
bool DemuxBuffer<short, DB_2D_REGULAR>::demux(bool start, int start_off, int len) {
	short* src000 = reinterpret_cast<short*>(pdata+start_off);
	short* src = src000;
	const int shortlen = len/sizeof(short)/2;
	const unsigned NC2 = nchan/2;
	const unsigned BUFSHORTS = Buffer::bufferlen/sizeof(short);

	if (verbose) fprintf(stderr, "%s start_off:%08x src:%p len:%d\n",
			_PFN, start_off, src, len);

	int b1 = MapBuffer::getBuffer(reinterpret_cast<char*>(src));
	if (b1&1){
		if (verbose) fprintf(stderr, "%s ODD buffer skip\n", _PFN);
		return true;
	}

	/* run to the end of buffer. nsam could be rounded down,
	 * so do not use it.
	 */
	if (verbose) fprintf(stderr, "%s will %s skip ES\n", _PFN, show_es? "NOT":"");


	for (unsigned isam = 0; true; ++isam){
		if (!show_es && evX.isES(reinterpret_cast<unsigned*>(src))){
			if (verbose) fprintf(stderr, "skip ES\n");
			src += nchan_es;
		}
		{
			short* src0 = src;
			for (unsigned ichan = 0; ichan < NC2; ++ichan){
				*ddcursors[DB_2D_R_MEM_PIN_LUT_48_DMA0[ichan]]++ =
						src0[ichan]&mask[DB_2D_R_MEM_PIN_LUT_48_DMA0[ichan]];
			}
		}
		{
			short* src1 = src+BUFSHORTS;
			for (unsigned ichan = 0; ichan < NC2; ++ichan){
				*ddcursors[DB_2D_R_MEM_PIN_LUT_48_DMA1[ichan]]++ =
						src1[ichan]&mask[DB_2D_R_MEM_PIN_LUT_48_DMA1[ichan]];
			}
		}
		src += nchan;
		if (src-src000 >= shortlen){
			if (verbose){
				fprintf(stderr,
					"demux() END buf ch:%d src:%p len:%d\n",
					nchan-1, src,  ddcursors[nchan-1]-dddata[nchan-1]);
			}
			return false;
		}
	}
	if (verbose) fprintf(stderr, "%s 99 DOES NOT HAPPEN\n", _PFN);
	/* does not happen */
	return true;
}

template <>
bool DemuxBuffer<short, DB_2D_DOUBLE>::demux(bool start, int start_off, int len) {
	short* src1 = reinterpret_cast<short*>(pdata+start_off);
	short* src = reinterpret_cast<short*>(pdata+start_off);
	int shortlen = len/sizeof(short)/2;
	int isam = 0;
	unsigned NC2 = nchan/2;
	const unsigned BUFSHORTS = Buffer::bufferlen/sizeof(short);

	if (verbose) fprintf(stderr, "%s start_off:%08x src:%p len:%d\n",
			_PFN, start_off, src, len);

	int b1 = MapBuffer::getBuffer(reinterpret_cast<char*>(src));
	if (b1&1){
		if (verbose) fprintf(stderr, "%s ODD buffer skip\n", _PFN);
		return true;
	}
	/*
	if (b1 > 100){
		if (verbose) fprintf(stderr, "%s no successor buffer skip\n", _PFN);
		return true;
	}
*/
	int startoff = 0;

	unsigned ichan = 0;
	/* run to the end of buffer. nsam could be rounded down so do not use it.  */
	if (verbose) fprintf(stderr, "%s will %s skip ES\n", _PFN, show_es? "NOT":"");

	for (isam = startoff/nchan; true; ++isam, ichan = 0){
		if (!show_es && evX.isES(reinterpret_cast<unsigned*>(src))){
			if (verbose) fprintf(stderr, "skip ES\n");
			src += nchan_es;
		}
		short* src0 = src;
		for (; ichan < NC2; ++ichan){
			*ddcursors[ichan]++ = (*src++)&mask[ichan];
			*ddcursors[ichan]++ = (*src++)&mask[ichan];
		}
		for (short *src2 = src0+BUFSHORTS; ichan < nchan; ++ichan){
			*ddcursors[ichan]++ = (*src2++)&mask[ichan];
			*ddcursors[ichan]++ = (*src2++)&mask[ichan];
		}
		if (src-src1 >= shortlen){
			if (verbose){
				fprintf(stderr,
					"demux() END buf ch:%d src:%p len:%d\n",
					ichan, src,  ddcursors[ichan]-dddata[ichan]);
			}
			return false;
		}
	}
	if (verbose) fprintf(stderr, "%s 99 DOES NOT HAPPEN\n", _PFN);
	/* does not happen */
	return true;
}

/** output multiple samples per buffer */
template <class T>
class OversamplingMapBuffer: public Buffer {
	const int over, asr;
	const int asr1;
	const int nsam;
	T *outbuf;
	int* sums;
public:
	OversamplingMapBuffer(Buffer* cpy, int _oversampling, int _asr) :
		Buffer(cpy),
		over(_oversampling),
		asr(_asr),
		asr1(sizeof(T)==4?8:0),
		nsam(buffer_len/sizeof(T)/G::nchan)
	{
		static int report;
		if (!report &&verbose){
			fprintf(stderr, "OversamplingMapBuffer()\n");
			++report;
		}
		outbuf = new T[nsam*G::nchan/over];
		sums = new int[G::nchan];
	}
	virtual ~OversamplingMapBuffer() {
		delete [] outbuf;
		delete [] sums;
	}
	virtual int writeBuffer(int out_fd, int b_opts) {
		T* src = reinterpret_cast<T*>(pdata);
		int nsum = 0;
		int osam = 0;

		fprintf(stderr, "OversamplingMapBuffer::writeBuffer(<%d>) out_fd:%d\n", sizeof(T), out_fd);
		memset(sums, 0, G::nchan*sizeof(int));

		for (int isam = 0; isam < nsam; ++isam){
			for (unsigned ic = 0; ic < G::nchan; ++ic){
				sums[ic] += src[isam*G::nchan+ic] >> asr1;
			}
			if (++nsum >= over){
				for (unsigned ic = 0; ic < G::nchan; ++ic){
					outbuf[osam*G::nchan+ic] = sums[ic] >> asr;
					sums[ic] = 0;
				}
				++osam;
				nsum = 0;
			}
		}

		return write(out_fd, outbuf, osam*G::nchan*sizeof(T));
	}
};

class _OversamplingMapBufferSingleSample: public Buffer {
public:
	int* sums;
	_OversamplingMapBufferSingleSample(Buffer* cpy):
		Buffer(cpy)
	{
		sums = new int[G::nchan];
	}
	virtual ~_OversamplingMapBufferSingleSample() {
		delete [] sums;
	}
	static void makeReport(const char* cname, int tsize){
		static int report;
		if (!report &&verbose){
			fprintf(stderr, "%s<%s>()\n", cname, tsize==4? "int": "short");
			++report;
		}
	}
};

/** output one sample per buffer */
template <class T>
class OversamplingMapBufferSingleSample: public _OversamplingMapBufferSingleSample {
	const int over, asr;
	const int asr1;
	const int nsam;

	bool checkit_ok(T* src, int isam) {
		T checkit = src[isam*G::nchan+0] >> asr1;
		for (unsigned ic = 1; ic < 4; ++ic){
			T checkit2 = src[isam*G::nchan+ic] >> asr1;
			if (checkit2 != checkit){
				return true;
			}
		}
		return false;
	}
public:
	OversamplingMapBufferSingleSample(Buffer* cpy,
			int _oversampling, int _asr) :
		_OversamplingMapBufferSingleSample(cpy),
		over(_oversampling),
		asr(sizeof(T)==4&&_asr>8 ? 8: _asr),
		asr1(sizeof(T)==4? (8 + _asr - asr):0),
		nsam(buffer_len/sizeof(T)/G::nchan)
	{
		makeReport("OversamplingMapBufferSingleSample", sizeof(T));
	}

	virtual int writeBuffer(int out_fd, int b_opts) {
		T* src = reinterpret_cast<T*>(pdata);
		int stride = nsam/over;

		if (G::show_first_sample){
			memset(sums, 0, G::nchan*sizeof(int));

			for (unsigned ic = 0; ic < G::nchan; ++ic){
				sums[ic] += src[ic];
			}
			int fd = createOutfile("/dev/shm/first_sample");
			write(fd, sums, G::nchan*sizeof(int));
			close(fd);
		}
		memset(sums, 0, G::nchan*sizeof(int));

		for (int isam = 0; isam < nsam; isam += stride){
			if (checkiten){
				/* runs of samples are bad - could be ES, could be strange 0x0000 .. either way, REJECT */
				if (!checkit_ok(src, isam) && !checkit_ok(src, ++isam)){
					if (verbose){
						fprintf(stderr, "checkit_ok reject\n");
					}
					return 0;
				}
			}
			for (unsigned ic = 0; ic < G::nchan; ++ic){
				sums[ic] += src[isam*G::nchan+ic] >> asr1;
			}
		}

		if (asr){
			for (unsigned ic = 0; ic < G::nchan; ++ic){
				sums[ic] >>= asr;
			}
		}
		if (verbose) fprintf(stderr, "writeBuffer() 99\n");

		return write(out_fd, sums, G::nchan*sizeof(int));
	}
};


#define SCOFF	24		/* sample count offset */
#define EFMT	"ERROR [%02d] %10llu at offset:%d %08x -> %s:%08x\n"

int u32toB(int nu32) {
	return nu32*sizeof(unsigned);
}
class ScratchpadTestBuffer: public MapBuffer {
	static bool report_done;
	const unsigned sample_size;
	const unsigned samples_per_buffer;

	static unsigned sample_count;
	static bool sample_count_valid;
	static unsigned long long samples_elapsed;
	static unsigned shim;
	static unsigned buffer_count;

	int last_sample_offset()
	/* offset in u32 */
	{
		return ((samples_per_buffer-1)*sample_size)/sizeof(unsigned);
	}

	bool isValidQuad(unsigned *pbuf, unsigned startoff){
		return (pbuf[startoff+0]&0x0000001f) == 0x0 &&
		       (pbuf[startoff+1]&0x0000001f) == 0x1 &&
		       (pbuf[startoff+2]&0x0000001f) == 0x2 &&
		       (pbuf[startoff+3]&0x0000001f) == 0x3;
	}
	int checkAlignment(unsigned *src) {
		if (isValidQuad(src, shim)){
			return 0;
		}else{

			for (shim = 0; shim < 32; ++shim){
				if (isValidQuad(src, shim)){
					fprintf(stderr, "ERROR [%02d] misalign shim set %d\n", ibuf, shim);
					exit(1);
					return 1;
				}
			}
			fprintf(stderr, "ERROR buffer %5d failed to realign, try next buffer\n", ibuf);
			return -1;
		}
	}
	int scoff(int offset = 0)
	/** return offset in u32 of Sample Count, compensated by shim */
	{
		return offset + (shim+SCOFF)%32;
	}
	int byteoff(int sample){
		return sample*sample_size;
	}

	void searchPointOfError(unsigned *src)
	{
		fprintf(stderr, ".. searching exact location\n");
		unsigned sc1 = src[scoff()];

		for (unsigned ii = 0; ii < samples_per_buffer; ++ii, src += G::nchan){
			if (!isValidQuad(src, shim)){
				fprintf(stderr, "Channel Jump at sample %d offset 0x%08x\n",
							ii, byteoff(ii));
				break;
			}else{
				if (ii){
					unsigned sc2 = src[scoff()];
					if (sc2 != sc1+ii){
						fprintf(stderr,
						"Missed samples at sample %d offset 0x%08x %u -> %u\n",
							ii, byteoff(ii), sc1+ii, sc2);
						break;
					}
				}
			}
		}
	}
public:
	ScratchpadTestBuffer(const char* _fname, int _buffer_len):
		MapBuffer(_fname, _buffer_len),
		sample_size(sizeof(unsigned) * G::nchan),
		samples_per_buffer(_buffer_len/sample_size)
	{
		if (!report_done){
			fprintf(stderr, "%20s : %u\n", "samples_per_buffer", samples_per_buffer);
			fprintf(stderr, "%20s : %u\n", "sample_size", sample_size);
			fprintf(stderr, "%20s : %u\n", "last_sample_offset()", last_sample_offset());
			report_done = true;
		}
	}
	virtual ~ScratchpadTestBuffer() {

	}
	virtual int writeBuffer(int out_fd, int b_opts) {
		++buffer_count;
		unsigned* src = reinterpret_cast<unsigned*>(pdata);

		if (checkAlignment(src) < 0){
			return 0;
		}
		unsigned sc1 = src[scoff()];
		unsigned sc2 = src[scoff(last_sample_offset())];

		fprintf(stderr, "[%02d] %d %d  %08x %08x\n", ibuf, sc1, sc2, sc1, sc2);

		if (sample_count_valid){
			if (sc1 != sample_count + 1){
				fprintf(stderr, EFMT, ibuf, samples_elapsed,
						u32toB(SCOFF), sample_count, "sc1", sc1);
			}
		}
		if (sc2 != sc1 + samples_per_buffer-1){
			fprintf(stderr, EFMT, ibuf, samples_elapsed,
					u32toB(last_sample_offset() + SCOFF),
					sc1+samples_per_buffer-2, "sc2", sc2);
			searchPointOfError(src);
			sample_count_valid = false;
			exit(1);
			return 0;
		}else{
			sample_count = sc2;
			sample_count_valid = true;
			samples_elapsed += samples_per_buffer;
			return buffer_len;
		}
	}
};

bool ScratchpadTestBuffer::report_done;
unsigned long long ScratchpadTestBuffer::samples_elapsed;
unsigned ScratchpadTestBuffer::sample_count;
bool ScratchpadTestBuffer::sample_count_valid;
unsigned ScratchpadTestBuffer::shim;
unsigned ScratchpadTestBuffer::buffer_count;

int ASR(int os)
/** calculate Arith Shift Right needed to ensure no overflow for os */
{
	int asr = 0;
	for (; 1<<asr < os; ++asr){
		;
	}
	return asr;
}

int Buffer::create(const char* root, int _buffer_len)
{
	char* fname = new char[128];
	sprintf(fname, "%s.hb/%03d", root, Buffer::last_buf);

	static int nreport;
	if (nreport == 0 && verbose){
		++nreport;
		if (verbose) fprintf(stderr, "Buffer::create() G::buffer_mode: %c\n", G::buffer_mode);
	}

	Buffer* newbuf;

	switch(G::buffer_mode){
	case BM_NULL:
		newbuf = new NullBuffer(fname, _buffer_len);
	case BM_TEST:
		newbuf = new ScratchpadTestBuffer(fname, _buffer_len);
	default:
		newbuf = new MapBuffer(fname, _buffer_len);
	}
	the_buffers.push_back(newbuf);
	assert(newbuf->ibuf+1 == the_buffers.size());
	return newbuf->ibuf;
}


class BufferCloner {
	static void cloneBuffers(BufferCloner& cloner)
	{
		vector<Buffer*> cpyBuffers = Buffer::the_buffers;

		Buffer::the_buffers.clear();
		for (unsigned ii = 0; ii < cpyBuffers.size(); ++ii){
			Buffer::the_buffers[ii] = cloner(cpyBuffers[ii]);
		}
	}
public:
	virtual Buffer* operator() (Buffer* cpy) = 0;

	template <class T>
	static void cloneBuffers()
	{
		T cloner;

		BufferCloner::cloneBuffers(cloner);
	}
};

class OversamplingBufferCloner: public BufferCloner {
	static Buffer* createOversamplingBuffer(Buffer* cpy);
public:
	OversamplingBufferCloner() {}
	virtual Buffer* operator() (Buffer* cpy) {
		return createOversamplingBuffer(cpy);
	}
};


Buffer* OversamplingBufferCloner::createOversamplingBuffer(Buffer* cpy)
{
	int os = abs(G::oversampling);
	bool single = G::oversampling < 0;

	if (G::wordsize == 2){
		if (single){
			return new OversamplingMapBufferSingleSample<short>(
					cpy, os, ASR(os));
		}else{
			return new OversamplingMapBuffer<short>(
					cpy, os, ASR(os));
		}
	}else{
		if (single){
			return new OversamplingMapBufferSingleSample<int>(
					cpy, os, ASR(os));
		}else{
			return new OversamplingMapBuffer<int> (
					cpy, os, ASR(os));
		}
	}
}


class DemuxBufferCloner: public BufferCloner {
	static Buffer* createDemuxBuffer(Buffer *cpy)
	{
		switch(G::wordsize){
		case sizeof(short):
			if (G::naxi==2){
				if(G::double_up){
					if (verbose) fprintf(stderr, "createDemuxBuffer DB_2D_DOUBLE\n");
					return new DemuxBuffer<short, DB_2D_DOUBLE>(cpy, G::mask);
				}else{
					if (verbose) fprintf(stderr, "createDemuxBuffer DB_2D_REGULAR\n");
					return new DemuxBuffer<short, DB_2D_REGULAR>(cpy, G::mask);
				}
			}else{
				if (G::double_up){
					return new DemuxBuffer<short, DB_DOUBLE>(cpy, G::mask);
				}else{
					return new DemuxBuffer<short, DB_REGULAR>(cpy, G::mask);
				}
			}
		case sizeof(int):
			return new DemuxBuffer<int, DB_REGULAR>(cpy, G::mask);
		default:
			fprintf(stderr, "ERROR: wordsize must be 2 or 4");
			exit(1);
		}
	}

public:
	virtual Buffer* operator() (Buffer* cpy) {
		return createDemuxBuffer(cpy);
	}
};


enum STATE {
	ST_STOP,
	ST_ARM,
	ST_RUN_PRE,
	ST_RUN_POST,
	ST_POSTPROCESS,
	ST_CLEANUP
};

#define NSinMS	1000000
#define MSinS	1000


static inline void timespec_diff(struct timespec *a, struct timespec *b,
    struct timespec *result) {
    result->tv_sec  = a->tv_sec  - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}
static long diffmsec(struct timespec* t0, struct timespec* t1){
	struct timespec result;
	timespec_diff(t1, t0, &result);

	return result.tv_sec * 1000 + result.tv_nsec/1000000;
}

#define MIN_REPORT_INTERVAL_MS	200


#define 	PRINT_WHEN_YOU_CAN	false

class Timer {
	const char* id;
	struct timespec t0;

public:
	Timer(const char* _id): id(_id) {
		clock_gettime(CLOCK_REALTIME_COARSE, &t0);
	}
	long diffmsec() {
		struct timespec time_now;
		clock_gettime(CLOCK_REALTIME_COARSE, &time_now);
		return ::diffmsec(&t0, &time_now);
	}
	long report(int seq = 0) {
		long rc = diffmsec();
		fprintf(stderr, "Timer::report(%d) %s %ld msec\n", seq, id, rc);
		return rc;
	}
};

struct Progress {
	enum STATE state;
	unsigned pre;
	unsigned post;
	unsigned long long elapsed;
	struct timespec last_time;
	static long min_report_interval;
	const char* name;
	char previous[80];

	FILE* status_fp;

	bool isRateLimited() {
		struct timespec time_now;
		clock_gettime(CLOCK_REALTIME_COARSE, &time_now);

		if (min_report_interval == 0 || diffmsec(&last_time, &time_now) >= min_report_interval){
			last_time = time_now;
			return false;
		}else{
			return true;
		}
	}
	Progress(FILE *_fp, const char* _name = "Progress") :
			status_fp(_fp) {
		memset(this, 0, sizeof(Progress));
		name = _name;
		status_fp = stderr;

		if (verbose){
			fprintf(status_fp, "min_report_interval set %ld\n", min_report_interval);
		}
		clock_gettime(CLOCK_REALTIME_COARSE, &last_time);

		if (verbose) fprintf(stderr, "Progress %s\n", name);
	}
	static Progress& instance(FILE* fp = 0);

	virtual void printState(char current[]) {

	}
	virtual void print(bool ignore_ratelimit = true, int extra = 0) {
	}
	virtual void setState(enum STATE _state){

	}

	static Progress null_progress;
};

Progress Progress::null_progress(stderr);


class ProgressImpl: public Progress {
	virtual void printState(char current[]) {
		if (G::state_fp){
			rewind(G::state_fp);
			fputs(current, G::state_fp);
			fflush(G::state_fp);
		}
	}
public:
	ProgressImpl(FILE *_fp) : Progress(_fp, "ProgressImpl") {
	}

	virtual void print(bool ignore_ratelimit = true, int extra = 0) {
		char current[128];
		snprintf(current, 128, "STX %d %d %d %llu %d\n", state, pre, post, elapsed, extra);

		if ((ignore_ratelimit || !isRateLimited()) && strcmp(current, previous)){
			fputs(current, status_fp);
			fflush(status_fp);
			strcpy(previous, current);
		}
		printState(current);
	}
	virtual void setState(enum STATE _state){
		state = _state;
		print();
	}
};



int shuffle_test;
int fill_ramp_incr;
int fill_zero;

struct poptOption opt_table[] = {
	{ "wrbuflen", 0, POPT_ARG_INT, &Buffer::bufferlen, 0,
			"reduce buffer len" },
	{ "spy", 0, POPT_ARG_INT, &G::is_spy, 0, "separate spy task" },
	{ "null-copy", 0, POPT_ARG_NONE, 0, BM_NULL,
			"no output copy"    },
	{ "fill-scale", 0, POPT_ARG_NONE, 0, 'F',
					"no output, fill output scaled by 1k"    },
	{ "verbose",   'v', POPT_ARG_INT, &verbose, 0,
			"set verbosity"	    },
	{ "report-es", 'r', POPT_ARG_INT, &G::report_es, 0,
			"report es position every find" },
	{ "show-es", 'E', POPT_ARG_INT, &G::show_es, 0,
			"leave es in data" },
	{
	  "findEvent-mask", 0, POPT_ARG_INT, &G::find_event_mask, 0,
	  	  	 "which event to seek 1:EV0, 2:EV1, 3:BOTH"
	},
	{
	  "multi-event", 0, POPT_ARG_STRING, &G::multi_event, 0,
	  	  	  "multi-event pre,post action"
	},
	{ "hb0",       0, POPT_ARG_NONE, 0, BM_DEMUX },
	{ "test-scratchpad", 0, POPT_ARG_NONE, 0, BM_TEST,
			"minimal overhead data test buffer top/tail for sample count"},
	{ "nchan",    'N', POPT_ARG_INT, &G::nchan, 0 },
	{ "wordsize", 'w', POPT_ARG_INT, &G::wordsize, 0,
			"data word size 2|4" },
	{ "oversampling", 'O', POPT_ARG_INT, &G::oversampling, 0,
			"set oversampling (negative: single output sample per buffer)"},
	{ "mask",      'M', POPT_ARG_INT, &G::mask, 0,
			"set data mask"},
	{ "m1",        'm', POPT_ARG_INT, &G::m1, 0,
			"index of first masked channel"},
	{ "nsam",       0,  POPT_ARG_INT, &G::nsam, 0,
			"desired output samples" },
	{ "oncompletion", 0, POPT_ARG_STRING, &G::script_runs_on_completion, 0,
			"run this script on completion of buffer"
	},
	{ "pre",	0, POPT_ARG_INT, &G::pre, 'P',
			"transient capture, pre-length"
	},
	{ "post",       0, POPT_ARG_INT, &G::post, 'O',
			"transient capture, post-length"
	},
	{ "demux",   'D', POPT_ARG_INT, &G::demux, 'D',
			"force demux / no demux after transient"
	},
	{ "sites",      0, POPT_ARG_STRING, &G::aggregator_sites, 0,
			"group of aggregated sites to lock"
	},
	{ "soft-trigger", 0, POPT_ARG_INT, &G::soft_trigger, 0,
			"assert soft-trigger after start"
	},
	{ "state-file",   0, POPT_ARG_STRING, &G::state_file, 'S',
			"write changing state string to this file"
	},
	{ "shuffle_test", 0, POPT_ARG_INT, &shuffle_test, 0,
			"time buffer shuffle"
	},
	{ "fill_ramp", 0, POPT_ARG_INT, &fill_ramp_incr, 'R', "fill with test data, negative means fill and drop out"},
	{ "fill_zero", 0, POPT_ARG_INT, &fill_zero, 'Z', "fill with zeros, negative means fill and drop out"},
	{ "pre-demux-script", 0, POPT_ARG_STRING, &G::pre_demux_script, 0,
			"breakout before demux"
	},
	{ "progress", 'p', POPT_ARG_STRING, &G::progress, 0,
			"log to this file"
	},
	{ "corner-turn-delay", 'c', POPT_ARG_INT, &G::corner_turn_delay, 0,
			"increase this to test effect of event at corner .. 510 ish"
	},
	{
	  "exit-on-trigger-fail", 0, POPT_ARG_INT, &G::exit_on_trigger_fail, 0,
	  	  	  "force quit on trigger fail"
	},
	{ "show-events", 0, POPT_ARG_INT, &G::show_events, 0,
			"show events in data"
	},
	{ "nbuffers", 0, POPT_ARG_INT, &Buffer::nbuffers, 0,
			"restrict number of buffers in use NB: NOT tested"
	},
        { "stream-sob-sig", 0, POPT_ARG_INT, &G::stream_sob_sig, 0,
                       "insert Start of Buffer signature in stream"
        },
	{
	  "live-instance", 0, POPT_ARG_INT, &G::live_instance, 0,
	                "default:1 set 0 to cancel the live instance where it makes no sense (eg short, high intensity shot)"
	},
	{ "subset", 0, POPT_ARG_STRING, &G::subset, 0, "reduce output channel count" },
	{ "sum",    0, POPT_ARG_STRING, &G::sum, 0, "sum N channels and output on another stream" },
	POPT_AUTOHELP
	POPT_TABLEEND
};



const char* root;

void acq400_stream_getstate(void);

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>




void ident(const char* tid = "acq400_stream") {
	char fname[80];
	sprintf(fname, "/var/run/%s.%d.pid", tid, G::devnum);
	FILE* fp = fopen(fname, "w");
	if (fp == 0) {
		perror(fname);
		exit(1);
	}
	fprintf(fp, "%d", getpid());
	fclose(fp);
}

static bool it_was_sig_term;


static void default_wait_on_sigterm(int signo)
{
	pid_t wpid;
	int status = 0;
	if (verbose){
		fprintf(stderr, "%s %d\n", _PFN, getpid());
	}

	while ((wpid = wait(&status)) > 0){
		if (verbose){
			fprintf(stderr, "%s %d reaps %d status:%d\n", _PFN, getpid(), wpid, status);
		}
	}
	exit(0);
}
static void wait_and_cleanup_sighandler(int signo)
{
	it_was_sig_term = true;
}
static void wait_and_cleanup(pid_t child)
{
	sigset_t  emptyset, blockset;
	int error_rc = 0;

	if (verbose) fprintf(stderr, "%s 01 pid %d\n", _PFN, getpid());

	ident();
	sigemptyset(&blockset);
	sigaddset(&blockset, SIGHUP);
	sigaddset(&blockset, SIGINT);
	sigaddset(&blockset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockset, NULL);

	Progress::instance();
	struct sigaction sa;
	sa.sa_handler = wait_and_cleanup_sighandler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	sigemptyset(&emptyset);
	fd_set exceptfds;
	FD_ZERO(&exceptfds);
	FD_SET(0, &exceptfds);
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	fprintf(stderr, "%s 01\n", _PFN);

	for(bool finished = false; !finished;){
		int ready = pselect(1, &readfds, NULL, &exceptfds, NULL, &emptyset);

		if (ready == -1){
			if (errno == EINTR){
				if (it_was_sig_term){
					fprintf(stderr, "%s sig_term detected, quit\n", _PFN);
					finished = true;
				}else{
					fprintf(stderr, "%s pselect return EINTR but no signal detected, continue\n", _PFN);
				}
			}else{
				fprintf(stderr, "ERROR %s pselect %d\n", _PFN, errno);
				finished = true;
			}
		}else if (FD_ISSET(0, &exceptfds)){
			if (verbose) fprintf(stderr, "%s exception on stdin why? feof:%d ferror:%d\n", _PFN, feof(stdin), ferror(stdin));
			if (feof(stdin) || ferror(stdin)){
				clearerr(stdin);
				finished = true;
				error_rc = 1;
			}else{
				continue;
			}
		}else if (FD_ISSET(0, &readfds)){
			if (feof(stdin)){
				if (verbose) fprintf(stderr,"%s EOF\n", _PFN);
				finished = true;
			}else if (ferror(stdin)){
				if (verbose) fprintf(stderr, "%s ERROR\n", _PFN);
				finished = true;
				error_rc = 2;
			}else{
				char stuff[80];
				fgets(stuff, 80, stdin);

				if (verbose) fprintf(stderr, "%s data on stdin %s\n", _PFN, stuff);
				if (strncmp(stuff, "quit", 4) == 0){
					finished = true;
				}else{
					fprintf(stderr, "options> quit\n");
				}
			}
		}else{
			if (verbose) fprintf(stderr, "%s out of pselect, not sure why\n", _PFN);
		}
	}

	fprintf(stderr, "%s 50\n", _PFN);
	sigaddset(&blockset, SIGTERM);
	sigdelset(&blockset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockset, NULL);
	verbose && fprintf(stderr, "%s %d exterminate\n", _PFN, getpid());
	Progress::instance().setState(ST_CLEANUP);
	kill(0, SIGTERM);

	int nwait = 0;
	pid_t wpid;
	int status = 0;
	while ((wpid = waitpid(-getpgrp(), &status, 0)) > 0){
		++nwait;
		verbose && fprintf(stderr, "%d waited for %d\n", nwait, wpid);
	}

	fprintf(stderr, "%s 99\n", _PFN);
	exit(error_rc);
}

static void hold_open(int site)
{
	char devname[80];
	char message[80];

	sprintf(devname, "/dev/acq400.%d.cs", site);
	FILE *fp = fopen(devname, "r");
	if (fp == 0){
		perror(devname);
		exit(1);
	}
	while(fread(message, 1, 80, fp)){
		chomp(message);
		bool quit = strstr(message, "ERROR") != 0;
		fprintf(stderr, "%s : \"%s\" %s\n", devname, message, quit? "QUIT ON ERROR": "OK");
		if (quit){
			break;
		}
	}
	exit(1);
}

std::vector<pid_t> holders;

static void make_aggsem() {
	G::aggsem_name = new char[80];
	sprintf(G::aggsem_name, "/acq400_stream.%d", getpid());
	G::aggsem = sem_open(G::aggsem_name, O_CREAT, S_IRWXU, 0);
	if (G::aggsem == SEM_FAILED){
		perror(G::aggsem_name);
		exit(1);
	}
	if (verbose) fprintf(stderr, "G_aggsem:%p\n", G::aggsem);
}

static void holder_wait_and_pass_aggsem() {
	int val;

	sem_getvalue(G::aggsem, &val);
	if (verbose) fprintf(stderr, "%d  %s sem:%d\n", getpid(), "before wait", val);
	if (sem_wait(G::aggsem) != 0){
		perror("sem_wait");
	}

	if (verbose) fprintf(stderr, "%d  %s\n", getpid(), "after wait");
	if (sem_post(G::aggsem) != 0){
		perror("sem_post");
	}

	if (verbose) fprintf(stderr, "%d  %s\n", getpid(), "after post");
	sem_close(G::aggsem);
	sem_unlink(G::aggsem_name);	// sem is only destroyed AFTER all users close()
}

static void hold_open(const char* sites)
{
	int *ss = G::the_sites;
	G::nsites = sscanf(sites, "%d,%d,%d,%d,%d,%d",
				ss+0, ss+1, ss+2, ss+3, ss+4, ss+5);

	if (G::nsites){
		make_aggsem();
		pid_t child = fork();
		if (child != 0){
			/* original becomes reaper */
			wait_and_cleanup(child);
		}else{
			for (int isite = 0; isite < G::nsites; ++isite ){
				child = fork();

				if (child == 0) {
					syslog(LOG_DEBUG, "%d  %10s %d\n", getpid(), "hold_open", isite);
					holder_wait_and_pass_aggsem();
					hold_open(G::the_sites[isite]);
					assert(1);
				}else{
					sched_yield();
					holders.push_back(child);
				}
			}
		}
	}
	/* newly forked main program continues to do its stuff */
}

static void kill_the_holders() {
	std::vector<pid_t>::iterator it;
	for (it = holders.begin(); it != holders.end(); ++it){
		int status;
		kill(*it, SIGTERM);
		wait(&status);
		if (verbose) fprintf(stderr, "kill_the_holders %d\n", *it);
	}
}

void shuffle_all_down1() {
	char *to = Buffer::the_buffers[0]->getBase();
	char *from = Buffer::the_buffers[1]->getBase();
	int len = Buffer::bufferlen * (Buffer::nbuffers-1);
	fprintf(stderr, "shuffle_all_down1: to:%p from:%p len:%d\n", to, from, len);
	memcpy(to,from,len);
}

template <class T>
void demux_all_down1(bool use_new) {
	T* to = use_new? new T[Buffer::bufferlen/sizeof(T)] :
			reinterpret_cast<T*>(Buffer::the_buffers[0]->getBase());
	T* from = reinterpret_cast<T*>(Buffer::the_buffers[1]->getBase());
	const unsigned nchan = G::nchan;
	const unsigned nsam = Buffer::bufferlen * (Buffer::nbuffers-1) / sample_size();
	const unsigned tomask = use_new? Buffer::bufferlen/sizeof(T)-1: 0xffffffff;

	fprintf(stderr, "demux_all_down1 nchan:%d nsam:%d ws=%d mask:%08x\n",
			nchan, nsam, sizeof(T), tomask);

	for (unsigned sample = 0; sample < nsam; ++sample){
		for (unsigned chan = 0; chan < nchan; ++chan){
			to[(chan*nsam + sample)&tomask] = from[sample*nchan + chan];
		}
	}
}
void do_shuffle_test(int level)
/* TEST MODE: time iterating all the buffers */
{
	switch(level){
	case 1:
		shuffle_all_down1();
		break;
	case 2:
	case 3:
		if (G::wordsize == 4){
			demux_all_down1<unsigned>(level&1);
		}else{
			demux_all_down1<short>(level&1);
		}
		break;

	default:
		fprintf(stderr, "shuffle_test level %d not supported\n", level);
	}
	exit(0);
}

void do_fill_ramp(int inc)
{
	unsigned* cursor = reinterpret_cast<unsigned*>(MapBuffer::get_ba0());
	unsigned* ba99 = reinterpret_cast<unsigned*>(MapBuffer::get_ba_hi());
	unsigned xx = 0;

	while(cursor < ba99){
		*cursor++ = xx += inc;
	}
}



void init_globs(void)
{
	getKnob(0, "/etc/acq400/0/NCHAN", &G::nchan);
	G::nchan_ES = get_nchan_ES(G::nchan);
	unsigned int data32 = false;
	getKnob(0, "/etc/acq400/0/data32", &data32);
	G::wordsize = data32? sizeof(int): sizeof(short);

	if (G::es_diagnostic){
		fprintf(stderr, "set ES_DIAGNOSTIC:%d\n", G::es_diagnostic);
	}
}

void checkHolders();

void reserveBuffers(void)
/* open and hold until death */
{
	open("/dev/acq400.0.rsvd", O_RDONLY);
}
void aggregator_init()
{
	if (G::devnum == 0 && G::aggregator_sites == 0){
		static char _sites[32];
		getKnob(0, "/etc/acq400/0/sites", _sites);
		G::aggregator_sites = _sites;
		if (verbose) fprintf(stderr, "default sites:%s\n", G::aggregator_sites);
	}
	reserveBuffers();
	/* else .. defaults to 0 */
	checkHolders();
	if (G::aggregator_sites != 0){
		hold_open(G::aggregator_sites);
	}
}

#define MODPRAMS "/sys/module/acq420fmc/parameters/"
#define DFB	 MODPRAMS "distributor_first_buffer"

void init(int argc, const char** argv) {
	char* progname = new char(strlen(argv[0]));
	if (strcmp(progname, "acq400_stream_getstate") == 0){
		acq400_stream_getstate();
	}
	delete [] progname;

	init_globs();
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	bool fill_ramp = false;

	getKnob(-1, DFB,   &Buffer::nbuffers);
	if (Buffer::nbuffers == 0){
		getKnob(G::devnum, "nbuffers",  &Buffer::nbuffers);
	}
	getKnob(G::devnum, "bufferlen", &Buffer::bufferlen);
	getKnob(G::devnum, "has_axi_dma", &G::naxi);

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		case 'F':
			G::null_copy = BM_NULL;
			G::fillk = 1;
			break;
		case 'n':
			G::null_copy = BM_NULL;
			break;
		case BM_DEMUX:
			fprintf(stderr, "--hb0 has been superceded. quitting\n");
			exit(1);
			break;
		case 'D':
			if (G::demux <= 0){
				G::buffer_mode = BM_RAW;
			}
			break;
		case 'O':
			if (G::oversampling == 0) break;
			// fall thru
		case 'P':
			G::stream_mode = SM_TRANSIENT;
			break;
		case 'S':
			G::state_fp = fopen(G::state_file, "r+");
			if (G::state_fp == 0){
				G::state_fp = fopen(G::state_file, "w");
			}
			if (G::state_fp == 0){
				perror(G::state_file);
				exit(1);
			}
			break;
		case 'R':
			fill_ramp = true;
			break;
		case BM_TEST:
			G::buffer_mode = BM_TEST;
			break;
		case 'p':
			if (strcmp(G::progress, "-") == 0){
				Progress::instance(stdout);
			}else{
				FILE *fp = fopen(G::progress, "w");
				if (!fp){
					perror(G::progress); exit(1);
				}else{
					Progress::instance(fp);
				}
			}
		}
	}
	/* do this early so all descendents get the same pgid .. so we can kill them :-) */
	setpgid(0, 0);
	struct sigaction sa;
	sa.sa_handler = default_wait_on_sigterm;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);

	Buffer::sample_size = sample_size();

	openlog("acq400_stream", LOG_PID, LOG_USER);
	syslog(LOG_DEBUG, "hello world %s", VERID);
	const char* devc = poptGetArg(opt_context);
	if (devc){
		G::devnum = atoi(devc);
	}


	if (G::stream_mode == SM_TRANSIENT){
		if (G::buffer_mode == BM_NOT_SPECIFIED){
			G::buffer_mode = BM_PREPOST;
		}
	}

	root = getRoot(G::devnum);

	if (ISACQ480()){
		if (G::nchan == 4 || (G::nchan==8 && G::naxi==2)){
			G::double_up = true;
			if (verbose) fprintf(stderr, "G::double_up set true\n");
		}
	}

	for (unsigned ii = 0; ii < Buffer::nbuffers; ++ii){
		Buffer::create(root, Buffer::bufferlen);
	}
	MapBuffer::reserve();
	if (shuffle_test){
		do_shuffle_test(shuffle_test);
	}
	if (fill_zero){
		do_fill_ramp(0);
		if (fill_zero < 0){
			exit(0);
		}
	}else if (fill_ramp){
		bool quit_on_fill = fill_ramp_incr < 0;
		fill_ramp_incr = abs(fill_ramp_incr);

		do_fill_ramp(fill_ramp_incr);
		if (quit_on_fill){
			exit(0);
		}
	}

}

class StreamHead {

	static bool has_pre_post_live_demux(void);
	static StreamHead* createLiveDataInstance();
protected:
	static const char* stream_fmt;

	int fc;
	int fout;
	static int multi_event;			/* file handle to multi-event comms */

	StreamHead(int _fc, int _fout = 1) : fc(_fc), fout(_fout) {
		assert(fc >= 0);
		assert(fout >= 0);
	}
	virtual ~StreamHead() {}

	virtual int getBufferId() {
		char buf[80];
		int rc = read(fc, buf, 80);

		if (rc > 0){
			buf[rc] = '\0';
			unsigned ib = strtoul(buf, 0, 10);
			assert(ib >= 0);
			assert(ib <= Buffer::nbuffers);
			return ib;
		}else{
			return rc<0 ? rc: -1;
		}
	}
	virtual char* findEvent(Buffer* the_buffer) {
		fprintf(stderr, "%s STUB\n", _PFN);
		return 0;
	}
	static void multiEventServer(int fd_in);
	static void createMultiEventInstance(const char* def);


public:
	virtual void stream() {
		int ib;

		if (verbose) fprintf(stderr, "StreamHead::stream()\n");

		while((ib = getBufferId()) >= 0){
			Buffer::the_buffers[ib]->writeBuffer(fout, Buffer::BO_NONE);
		}
	}
	virtual void startStream() {
		stream();
	}
	static StreamHead* instance();
};

int StreamHead::multi_event;
const char* StreamHead::stream_fmt = "%s.c";

#define TRG_POLL_MS 100

class StreamHeadImpl: public StreamHead {

protected:
	Progress& actual;
	int samples_buffer;
	int f_ev;
	int nfds;
	bool event_received;

	int ev_num;
	int verbose;
	int buffer_id_verbose;
	int stop_on_fail;
	int buffers_searched;
	char event_info[80];
	AbstractES& evX;
	AbstractES& ev0;
	AbstractES& ev1;
	AbstractES& evA;	/* Active Event Signature */

        unsigned sob_count;
        unsigned* sob_buffer;
        unsigned buffer_count;

	int MAX_BACKTRACK;
	int MAX_FORTRACK;

        void insertStartOfBufferSignature(int ib){
        	if (verbose) fprintf(stderr, "StreamHeadImpl::insertStartOfBufferSignature() %d ib:%d bc:%d\n",
        			sob_count, ib, buffer_count);

        	buffer_count += 1;
        	for (unsigned ii = sob_count/2; ii < sob_count; ++ii){
                       sob_buffer[ii] = (ii+1) == sob_count? buffer_count: ib;
        	}
        	write(1, sob_buffer, sob_count*sizeof(unsigned));
        }

	void close() {
		if (fc){
			::close(fc);
			fc = 0;
		}
		kill_the_holders();
		if (G::aggsem){
		       	sem_close(G::aggsem);
		       	sem_unlink(G::aggsem_name);
		}
	}
	virtual ~StreamHeadImpl() {
		close();
		if (sob_buffer){
			delete [] sob_buffer;
		}
	}

	void setState(enum STATE state) {
		if (verbose) fprintf(stderr, "StreamHeadImpl::setState(%d)\n", state);
		actual.setState(state);
		if (verbose) fprintf(stderr, "StreamHeadImpl::setState(%d) DONE\n", state);
	}

	void do_soft_trigger() {
		setKnob(0, "soft_trig", "0");
		setKnob(0, "soft_trig", "1");
		setKnob(0, "soft_trig", "0");
	}
	bool is_triggered() {
		unsigned trig = 0;
		getKnob(G::the_sites[0], "is_triggered", &trig);
		return trig;
	}
	void soft_trigger_control() {
		int repeat = 1;
		do_soft_trigger();
		usleep(TRG_POLL_MS*1000);
		while (!is_triggered()){
			if (repeat%100 == 0){
				fprintf(stderr, "WARNING: failed to trigger in %d ms\n", repeat*TRG_POLL_MS);
				if (G::exit_on_trigger_fail){
					system("kill -9 $(cat /var/run/acq400_stream_main.0.pid)");
					exit(1);
				}
			}
			usleep(TRG_POLL_MS*1000);
			++repeat;
			if (!is_triggered()) {
				do_soft_trigger();
			}
		}
		if (verbose || repeat){
			fprintf(stderr, "soft_trigger_control() repeat %d\n", repeat);
		}
	}

	void schedule_soft_trigger(void) {
		if (verbose) fprintf(stderr, "schedule_soft_trigger()");
		pid_t child = fork();
		if (child == 0){
			ident("acq400_stream_st");
			goRealTime(10);
			soft_trigger_control();
			exit(0);
		}
	}
	static int open_feed() {
		char fname[128];
		sprintf(fname, stream_fmt, root);
		int _fc = open(fname, O_RDONLY);
		assert(_fc > 0);
		return _fc;
	}


	void startEventWatcher() {
		f_ev = open("/dev/acq400.1.ev", O_RDONLY);
		if (f_ev < 0){
			perror("/dev/acq400.1.ev");
			exit(1);
		}
		if (f_ev > fc){
			nfds = f_ev+1;
		}
	}
	void report(const char* id, int ibuf, char *esp);

	void esDiagnostic(Buffer* the_buffer, unsigned *cursor)
	{
		if (G::es_diagnostic == 0) return;

		fprintf(stderr, "%s 01 %d\n", _PFN, G::es_diagnostic);
		FILE *fp = fopen("/dev/shm/es", "w");
		if (!fp){
			perror("/dev/shm/es");
		}else{
			fwrite(cursor, G::wordsize, G::nchan, fp);
			fclose(fp);
		}
		fp = fopen("/dev/shm/es5", "w");
		if (!fp){
			perror("/dev/shm/es5");
		}else{
			fwrite(cursor-2*G::nchan, G::wordsize, 5*G::nchan, fp);
			fclose(fp);
		}

		if (G::es_diagnostic > 1){
			fprintf(stderr, "%s heavy\n", _PFN);
			char fname[80];
			snprintf(fname, 80, "/tmp/es_buffer.%03d", the_buffer->ib());
			char* buffer = new char[the_buffer->bufferlen];
			the_buffer->copyBuffer(buffer);
			fp = fopen(fname, "w");
			if (fp){
				fwrite(buffer, 1, the_buffer->bufferlen, fp);
				fclose(fp);
				delete [] buffer;
			}else{
				perror(fname);
			}
		}
		fprintf(stderr, "%s 99\n", _PFN);
	}
	bool findEvent(int *ibuf, char** espp) {

		unsigned long usecs;
		int b1, b2, b3;
		int nscan = sscanf(event_info, "%lu %d %d", &usecs, &b1, &b2);

		if (verbose) fprintf(stderr, "%s \"%s\" nscan=%d %s\n", _PFN,
				event_info, nscan, (b1 == -1 && b2 == -1)? "FAIL": "finding..");

		assert(nscan == 3);
		if (b1 == -1 && b2 == -1){
			if (verbose) fprintf(stderr, "findEvent scan fail \"%s\"\n", event_info);
			return false;
		}

		if (verbose) fprintf(stderr, "findEvent b1=%d b2=%d\n", b1, b2);

		Buffer* buffer2 = Buffer::the_buffers[b2];


		if (verbose) fprintf(stderr, "call findEvent buffer2 %d\n", buffer2->ib());
		char* esp = findEvent(buffer2);
		if (esp){
			report("b2", b2, esp);
			if (ibuf) *ibuf = b2;
			if (espp) *espp = esp;
			if (verbose) fprintf(stderr, "%s return b2=%d esp=%p true\n", _PFN, b1, esp);
			return true;
		}else{
			if (verbose) fprintf(stderr, "b2 not found\n");
		}

		if (verbose) fprintf(stderr, "call findEvent buffer2+ %d\n", buffer2->succ());
		Buffer* buffer3 = buffer2;
		for (int iforward = 1; iforward < MAX_FORTRACK; ++iforward){
			esp = findEvent(buffer3 = Buffer::the_buffers[b3 = buffer3->succ()]);
			if (esp){
				report("b3", b3, esp);
				if (ibuf) *ibuf = b3;
				if (espp) *espp = esp;
				if (verbose) fprintf(stderr, "%s return b3=%d esp=%p true\n", _PFN, b1, esp);
				return true;
			}else{
				if (verbose) fprintf(stderr, "b3=%d not found\n", b3);
			}
		}

		Buffer* buffer1;
		if (b1 == -1){
			buffer1 = Buffer::the_buffers[b1 = buffer2->pred()];
			if (verbose) fprintf(stderr, "b1==-1 now set %d\n", b1);
		}else{
			buffer1 = Buffer::the_buffers[b1];
		}
		if (verbose) fprintf(stderr, "call findEvent buffer1 %d\n", buffer1->ib());
		esp = findEvent(buffer1);
		if (esp){
			report("b1", b1, esp);
			if (ibuf) *ibuf = b1;
			if (espp) *espp = esp;
			if (verbose) fprintf(stderr, "%s return b1=%d esp=%p true\n", _PFN, b1, esp);
			return true;
		}

		if (verbose) fprintf(stderr, "backtrack\n");
		for (int backtrack = 1;
			(buffer1 = Buffer::the_buffers[buffer1->pred()]) != buffer2;
			++backtrack){
			esp = findEvent(buffer1);
			if (esp){
				char bn[16]; sprintf(bn, "bt%d", backtrack);
				report(bn, buffer1->ib(), esp);
				if (ibuf) *ibuf = b1;
				if (espp) *espp = esp;
				return true;
			}
			if (backtrack >= MAX_BACKTRACK){
				break;
			}
		}
		reportFindEvent(buffer1, FE_FAIL);
		if (verbose) fprintf(stderr, "ERROR: not found anywhere\n");
		if (stop_on_fail){
			fprintf(stderr, "STOP ON FAIL\n");
			exit(1);
		}
		return false;
	}
	int countES(char* start, int len) {
		unsigned stride = G::nchan*G::wordsize/sizeof(unsigned);
		unsigned *cursor = reinterpret_cast<unsigned*>(start);
		unsigned *base = cursor;
		int lenw = len/G::wordsize;
		int escount = 0;

		for (; cursor - base < lenw; cursor += stride){
			if (evX.isES(cursor)){
				if (verbose) fprintf(stderr, "FOUND: %08x %08x\n", cursor[0], cursor[4]);
				++escount;
			}
		}
		return escount;
	}
	enum FE_STATUS { FE_IDLE, FE_SEARCH, FE_FOUND, FE_NOTFOUND, FE_FAIL, FE_COUNT };

	unsigned FE_HISTO[FE_COUNT];

	void reportFindEvent(Buffer* the_buffer, enum FE_STATUS festa){
		int ib = the_buffer == 0? 0: the_buffer->ib();
		FE_HISTO[festa]++;
		FILE* fp = fopen("/dev/shm/festa", "w");
		assert(fp);
		fprintf(fp, "%u,%u\n", FE_HISTO[FE_FOUND], FE_HISTO[FE_NOTFOUND]);
		fclose(fp);

		fprintf(stderr, "findEvent=%d,%d,%d\n", festa, ib, buffers_searched);
	}
	virtual char* findEvent(Buffer* the_buffer) {
		unsigned stride = G::nchan*G::wordsize/sizeof(unsigned);
		unsigned *base = reinterpret_cast<unsigned*>(the_buffer->getBase());
		int len32 = the_buffer->getLen()/sizeof(unsigned);
		int sample_offset = 0;

		++buffers_searched;
		reportFindEvent(the_buffer, FE_SEARCH);
		if (verbose) fprintf(stderr, "%s 01 base:%p lenw %d len32 %d\n", _PFN,
				base, the_buffer->getLen()/G::wordsize, len32);



		for (unsigned *cursor = base; cursor - base < len32; cursor += stride, sample_offset += 1){
			if (verbose > 2) fprintf(stderr, "findEvent cursor:%p\n", cursor);
			if (evA.isES(cursor)){
				reportFindEvent(the_buffer, FE_FOUND);
				if (verbose){
					fprintf(stderr, "FOUND: %d offset %d %08x %08x %08x %08x\n",
						the_buffer->ib(), (cursor-base)*sizeof(int),
									cursor[0], cursor[1], cursor[2], cursor[3]);
				}
				esDiagnostic(the_buffer, cursor);
				return reinterpret_cast<char*>(cursor);
			}
		}
		reportFindEvent(the_buffer, FE_NOTFOUND);
		if (verbose) fprintf(stderr, "findEvent 99 NOT FOUND\n");

		return 0;
	}
	bool findEventSearchAll(int *ibuf, char** espp) {
		unsigned stride = G::nchan*G::wordsize/sizeof(unsigned);
		unsigned *base = reinterpret_cast<unsigned*>(MapBuffer::get_ba_lo());
		unsigned *top  = reinterpret_cast<unsigned*>(MapBuffer::get_ba_hi());
		int length_all  = top - base;
		int len32 = Buffer::the_buffers[2]->getLen()/sizeof(unsigned);
		int sample_offset = 0;
		unsigned *cursor = base;

		buffers_searched = 0;

		Buffer* the_buffer = MapBuffer::getBuffer(cursor);
		reportFindEvent(the_buffer, FE_SEARCH);
		if (verbose) fprintf(stderr, "%s 01 base:%p lenw %d len32 %d length_all %d stride %d\n", _PFN,
				base, the_buffer->getLen()/G::wordsize, len32, length_all, stride);



		for (; cursor - base < length_all; cursor += stride, sample_offset += 1){
			if (verbose > 2) fprintf(stderr, "%s cursor:%p\n", _PFN, cursor);
			if (evA.isES(cursor)){
				the_buffer = MapBuffer::getBuffer(cursor);
				*ibuf = the_buffer->ib();
				*espp = reinterpret_cast<char*>(cursor);
				reportFindEvent(the_buffer, FE_FOUND);
				if (verbose){
					fprintf(stderr, "%s FOUND: %d offset %d %08x %08x %08x %08x\n",
						_PFN,
						the_buffer->ib(), (cursor-base)*sizeof(int),
						cursor[0], cursor[1], cursor[2], cursor[3]);
				}
				esDiagnostic(the_buffer, cursor);
				return reinterpret_cast<char*>(cursor);
			}
		}
		reportFindEvent(the_buffer = MapBuffer::getBuffer(cursor), FE_NOTFOUND);
		if (verbose) fprintf(stderr, "%s 99 NOT FOUND sample_offset %d\n", _PFN, sample_offset);

		return false;
	}
	virtual int getBufferId() {
		int ib = StreamHead::getBufferId();
		if (buffer_id_verbose) fprintf(stderr, "StreamHeadImpl::getBufferId() %d %s\n", ib, G::stream_sob_sig? "SOB":"");
		if (G::stream_sob_sig){
			insertStartOfBufferSignature(ib);
		}
		return ib;
	}
	static void set_axi_oneshot(int value){
		setKnob(0, "/sys/module/acq420fmc/parameters/AXI_ONESHOT", value);
	}
public:
	StreamHeadImpl(Progress& progress) : StreamHead(1234),
		actual(progress),
		samples_buffer(Buffer::bufferlen/sample_size()),
		f_ev(0), nfds(0), event_received(0), ev_num(0), verbose(0),
		buffers_searched(0),
		evX(*AbstractES::evX_instance()),
		ev0(*AbstractES::ev0_instance()),
		ev1(*AbstractES::ev1_instance()),
		evA((G::find_event_mask&3)==3? evX: (G::find_event_mask&2)==2? ev1: ev0),
		sob_count(0), sob_buffer(0), buffer_count(0),
		MAX_BACKTRACK(getenv_default("MAX_BACKTRACK", 3)),
		MAX_FORTRACK(getenv_default("MAX_FORTRACK", 4))
	{
		verbose = getenv_default("StreamHeadImplVerbose");
		buffer_id_verbose = getenv_default("StreamHeadImplBufferIdVerbose");

		if (G::stream_sob_sig){
			if (verbose) fprintf(stderr, "StreamHeadImpl::StreamHeadImpl() : G::stream_sob_sig %d\n", G::stream_sob_sig);
			sob_count = sample_size()/sizeof(unsigned);
			sob_buffer = new unsigned[sob_count];
			for (unsigned ii = 0; ii < sob_count/2; ++ii){
				sob_buffer[ii] = SOB_MAGIC;
			}
		}

		stop_on_fail = getenv_default("StreamHeadImplStopOnFail");
		if (verbose) fprintf(stderr, "StreamHeadImpl() pid %d progress: %s\n", getpid(), actual.name);
		memset(FE_HISTO, 0, sizeof(FE_HISTO));
		reportFindEvent(0, FE_IDLE);
	}
	virtual void startStream() {
		fc = open_feed();
		stream();
	}
	virtual void onStreamStart() {

	}
	virtual void onStream(int ib){
		Buffer::the_buffers[ib]->writeBuffer(1, Buffer::BO_NONE);
	}
	virtual void onStreamEnd() {

	}
	virtual void stream() {
		int ib;
		if (verbose) fprintf(stderr, "StreamHeadImpl::stream() :\n");

		ident("acq400_stream_headImpl");
		setState(ST_ARM);
		if (G::soft_trigger){
			schedule_soft_trigger();
		}
		onStreamStart();
		while((ib = getBufferId()) >= 0){
			if (verbose) fprintf(stderr, "StreamHeadImpl::stream() : %d\n", ib);
			onStream(ib);
			switch(actual.state){
			case ST_ARM:
				setState(ST_RUN_PRE);
			default:
				;
			}
			actual.elapsed += samples_buffer;
			actual.print(PRINT_WHEN_YOU_CAN);
		}
		setState(ST_CLEANUP);
		onStreamEnd();
	}
	friend class StreamHead;
};


class NullStreamHead: public StreamHeadImpl {
protected:
	virtual void _println(const char* fmt, ...){
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		fflush(stdout);
	}
public:
	virtual void onStream(int ib) {
		_println("%3d\n", ib);
	}

	NullStreamHead(Progress& progress) :
		StreamHeadImpl(progress)
	{}
};

class NullStreamHeadDivK: public NullStreamHead {
	int kbuf_len;
	char* kbuf;
protected:
	virtual void _println(const char* fmt, ...){
		va_list args;
		va_start(args, fmt);
		vsprintf(kbuf, fmt, args);
		write(1, kbuf, kbuf_len);
	}
public:
	NullStreamHeadDivK(Progress& progress) :
		NullStreamHead(progress)
	{
		kbuf_len = Buffer::the_buffers[0]->getLen()/1024;
		kbuf = new char [kbuf_len];
		memset(kbuf, '\r', kbuf_len);
	}
	virtual ~NullStreamHeadDivK() {
		delete [] kbuf;
	}
};

class StreamHeadHB0: public StreamHeadImpl  {
protected:
	virtual void stream();

public:
       StreamHeadHB0(): StreamHeadImpl(Progress::null_progress) {
               if (G::script_runs_on_completion == 0){
                       G::script_runs_on_completion = "/tmp/ondemux_complete";
               }
       }
       virtual ~StreamHeadHB0() {
	       setKnob(0, "/etc/acq400/0/live_mode", "0");
       }
};

void StreamHeadHB0__verbose(unsigned ib[], Buffer* b0, Buffer* b1){
	int last = b0->getLen()/b0->getSizeofItem() - 1;
	fprintf(stderr, "b0:%p b1:%p\n", b0, b1);
	fprintf(stderr,
		"buffer end %3d %08x %08x %08x %08x init %3d\n",
		ib[0], 	b0->getItem(last-1), b0->getItem(last),
		b1->getItem(0),	b1->getItem(1), ib[1]);
	fprintf(stderr,
		"buffer end %3d %08x %08x %08x %08x init %3d\n",
		ib[0],
		b0->getItem(last-1)&0x0ff,
		b0->getItem(last)&0x0ff,
		b1->getItem(0)&0x0ff,
		b1->getItem(1)&0x0ff, ib[1]);
}
void StreamHeadHB0::stream() {
	char buf[80];
	int rc;
	int nb = 0;
	bool quit_at_end_loop = false;

	while((rc = read(fc, buf, 80)) > 0){
		buf[rc] = '\0';

		unsigned ib[2];
		int nscan = sscanf(buf, "%d %d", ib, ib+1);

		assert(nscan >= 1);
		if (nscan > 0) assert(ib[0] >= 0 && ib[0] < Buffer::nbuffers);
		if (nscan > 1 && !(ib[1] >= 0 && ib[1] < Buffer::nbuffers)){
			fprintf(stderr, "ASSERT: ib[0]=%d ib[1]=%d nb:%d\n", ib[0], ib[1], Buffer::nbuffers);
			nscan = 1;
			quit_at_end_loop = true;
		}
		if (nscan > 1) assert(ib[1] >= 0 && ib[1] < Buffer::nbuffers);

		if (verbose) fprintf(stderr, "\n\n\nUPDATE:%4d nscan:%d read: %s",
				++nb, nscan, buf);

		if (nb_cat > 1 && nscan == 2){
			Buffer* b0 = Buffer::the_buffers[ib[0]];
			Buffer* b1 = Buffer::the_buffers[ib[1]];
			if (verbose){
				StreamHeadHB0__verbose(ib, b0, b1);
			}
			if (verbose > 10){
				fprintf(stderr, "no write\n"); continue;
			}
			b0->writeBuffer(1, Buffer::BO_START);
			b1->writeBuffer(1, Buffer::BO_FINISH);
		}else if (nscan == 2){
			Buffer* b1 = Buffer::the_buffers[ib[1]];
			b1->writeBuffer(1, Buffer::BO_START|Buffer::BO_FINISH);
		}else if (nscan == 1){
			Buffer* b0 = Buffer::the_buffers[ib[0]];
			b0->writeBuffer(1, Buffer::BO_START|Buffer::BO_FINISH);
		}
		if (verbose) fprintf(stderr, "UPDATE:%4d finished\n", nb);

		if (quit_at_end_loop){
			break;
		}
	}
}

#define OBS_ROOT "/dev/shm/transient"
#define TOTAL_BUFFER_LIMIT	(Buffer::nbuffers * Buffer::bufferlen)

#define LIVE_PRE	"/etc/acq400/0/live_pre"
#define LIVE_POST	"/etc/acq400/0/live_post"

class StreamHeadLivePP : public StreamHeadHB0 {
	unsigned pre;
	unsigned post;
	const int sample_size;
	const int es_size;
	int* sample_interval_usecs;

	int prelen() { return pre*sample_size; }
	int postlen() { return post*sample_size; }

	void getSampleInterval();
	void startSampleIntervalWatcher();


	static bool getPram(const char* pf, unsigned* pram)
	{
		FILE *fp = fopen(pf, "r");
		if (!fp) {
			return false;
		}
		char aline[32];
		bool rc = false;
		if (fgets(aline, 32, fp)){
			rc = sscanf(aline, "%u", pram) == 1;
		}
		fclose(fp);
		return rc;
	}
	static bool getPP(unsigned *_pre, unsigned* _post)
	{
		unsigned pp[2];
		if (getPram(LIVE_PRE, pp) && getPram(LIVE_POST, pp+1)){
			if (*_pre != pp[0] || *_post != pp[1]){
				if (::verbose){
					fprintf(stderr, "%s %d,%d => %d,%d\n", _PFN, *_pre, *_post, pp[0], pp[1]);
				}
				*_pre = pp[0];
				*_post = pp[1];
				return true;
			}
		}

		return false;
	}
	bool getPP(void) {
		return getPP(&pre, &post);
	}

	void waitPost() {
		int siu = 100;	/* assume min SR=10kHz, ensure data has arrived before read .. */
		int wait_us;
		if (sample_interval_usecs){
			siu = *sample_interval_usecs;
		}
		wait_us = siu*post;
		wait_us = min(wait_us, 500000); /* Live PP should never sleep > 1s */
		if (verbose) fprintf(stderr, "%s usleep %d\n", _PFN, wait_us);
		usleep(wait_us);
	}
	static bool event0_enabled(int site){
		char event_line[80];
		if (getKnob(site, "event0", event_line) == 1){
			unsigned ena = 0;
			if (sscanf(event_line, "event0=%u", &ena) == 1){
				return ena != 0;
			}
		}
		return 0;
	}

	int  _stream();
	static int verbose;
	static bool show_es;
public:
	StreamHeadLivePP():
			pre(0), post(4096),
			sample_size(G::nchan*G::wordsize),
			es_size(::es_size()){

		if (verbose) fprintf(stderr, "%s pid:%d %s verbose=%d\n",
				_PFN, getpid(), actual.name, verbose);

		startEventWatcher();
		startSampleIntervalWatcher();

		if (verbose) fprintf(stderr, "%s: buffer[0] : %p, [1]: %p\n",
				_PFN,
				Buffer::the_buffers[0]->getBase(),
				Buffer::the_buffers[1]->getBase());

		MAX_BACKTRACK = 0;
		MAX_FORTRACK = 0;
	}

	static bool hasPP() {
		unsigned pp[2];

		return event0_enabled(1) && getPP(pp, pp+1) && pp[0]+pp[1] > 0;
	}
	virtual void stream();
};

int StreamHeadLivePP::verbose = getenv_default("StreamHeadLivePPVerbose");
bool StreamHeadLivePP::show_es = getenv_default("StreamHeadLivePPShowES");

bool StreamHead::has_pre_post_live_demux(void) {
	return StreamHeadLivePP::hasPP();
}

void StreamHeadLivePP::getSampleInterval()
{
	FILE *pp = popen("get.site 1 SIG:sample_count:FREQ", "r");

	int freq;
	if (fscanf(pp, "SIG:sample_count:FREQ %d", &freq) == 1 && freq > 0){
		int siu = 1000000/freq;
		if (siu != *sample_interval_usecs){
			if (abs(siu - *sample_interval_usecs) > 5){
				syslog(LOG_DEBUG, "SI change %d -> %d\n",
						*sample_interval_usecs, siu);
				*sample_interval_usecs = siu;
			}
		}
	}
	pclose(pp);
}
void StreamHeadLivePP::startSampleIntervalWatcher() {
	int rc = shmget(0xdeadbeee, sizeof(int), IPC_CREAT|0666);
	if (rc == -1){
		perror("shmget()");
		exit(1);
	}else{
		void *shm = shmat(rc, 0, 0);
		if (shm == (void*)-1){
			perror("shmat()");
			exit(1);
		}else{
			sample_interval_usecs = (int*)shm;
			*sample_interval_usecs = 100;

			if (fork() == 0){
				ident("acq400_stream_siw");
				while (1){
					sleep(1);
					getSampleInterval();
				}
			}
		}
	}
}

int StreamHeadLivePP::_stream() {
	int rc = 0;
	char* b0 = MapBuffer::get_ba_lo();
	char* b1 = MapBuffer::get_ba_hi();

	if (verbose > 1) fprintf(stderr, "%s: f_ev %d\n", _PFN, f_ev);
// @@TODO: why does it DIE HERE?

	sigset_t  emptyset;

	fd_set exceptfds;
	struct timespec pto;
	fd_set readfds;

#define INIT_SEL do { \
	sigemptyset(&emptyset); \
	pto.tv_sec = 1; \
	pto.tv_nsec = 0; \
	FD_ZERO(&exceptfds); \
	FD_SET(f_ev, &exceptfds); \
	FD_ZERO(&readfds); \
	FD_SET(f_ev, &readfds); \
	} while(0)


	INIT_SEL;
	// 1637099 -1 201    sample_count, hb0 hb1
	for( getPP(); rc >= 0; getPP()){
		int bo1 = Buffer::BO_NONE;
		int bo2 = Buffer::BO_NONE;

		INIT_SEL;
		rc = pselect(f_ev+1, &readfds, NULL, &exceptfds, &pto, &emptyset);
		if (rc < 0){
			if (verbose > 1) fprintf(stderr, "StreamHeadLivePP::stream(): pselect error %d\n", errno);
			fprintf(stderr, "%s: pselect error nfds:%d pto.%lu\n", _PFN, f_ev+1, pto.tv_sec);
			perror("pselect()");
			exit(1);
		}

		if (FD_ISSET(f_ev, &exceptfds)){
			rc = -1; break;
		}else if (!FD_ISSET(f_ev, &readfds)){
			continue;
		}else if ((rc = read(f_ev, event_info, 80)) <= 0){
			return -1;
		}

		if (pre){
			bo1 |= Buffer::BO_START;
		}else{
			bo2 |= Buffer::BO_START;
		}
		if (!post){
			bo1 |= Buffer::BO_FINISH;
		}else{
			bo2 |= Buffer::BO_FINISH;
		}

		event_info[rc] = '\0';
		chomp(event_info);

		event_received = true;

		int ibuf;
		char* es;

		if (!findEvent(&ibuf, &es)){
			if (verbose) fprintf(stderr, "%s 390\n", _PFN);
			continue;	// silently drop it. there will be more
		}
		if ((unsigned)ibuf+MAX_FORTRACK >= Buffer::nbuffers-1){
			if (verbose) fprintf(stderr, "%s 391\n", _PFN);
			continue;
		}

		ev_num += 1;

		if (multi_event){
			char ev_message[128];
			sprintf(ev_message, "%d,%d,%p\n", ev_num, ibuf, es);
			write(multi_event, ev_message, strlen(ev_message));
		}

		if (es+postlen() > Buffer::the_buffers[ibuf]->getEnd()){
			// only data in this buffer is guaranteed to be there .. drop it
			if (verbose) fprintf(stderr, "%s 395\n", _PFN);
			continue;
		}
		if (verbose){
			fprintf(stderr, "%s found %d %p pre %d post %d\n", _PFN,
					ibuf, es, pre, post);
		}
		char *es1 = es + (show_es? 0: es_size);

		// all info referenced to buffer 0!
		Buffer* buf = Buffer::the_buffers[0];
		b0 = buf->getBase();

		if (!(es - prelen() > b0 && es1 + postlen() < b1 )){
			if (verbose) fprintf(stderr, "%s 49\n", _PFN);
			continue;	// silently drop it. there will be more
		}
		if (pre){
			int escount = countES(es-prelen(), prelen());
			int eslen = escount*es_size;
			int start_off = es - prelen() - b0 - eslen;
			int len = prelen()+eslen;

			if (es - prelen() > b0 + eslen){
				if (verbose) fprintf(stderr, "%s 54 escount:%d\n", _PFN, escount);
			}

			int nb = buf->writeBuffer(1, bo1, start_off, len);
			if (verbose) fprintf(stderr, "%s 56 wb %d, %d => %d\n", _PFN, start_off, len, nb);
		}
		if (post){
			int escount = countES(es1, postlen());
			int eslen = escount*es_size;
			int start_off = es1 - b0;
			int len = postlen()+eslen;

			if (es1 - b0 > postlen() - eslen){
				if (verbose) fprintf(stderr, "%s 57 escount:%d\n", _PFN, escount);
			}
			waitPost();

			int nb = buf->writeBuffer(1, bo2, start_off, len);
			if (verbose) fprintf(stderr, "%s 59 wb %d, %d => %d\n", _PFN, start_off, len, nb);
		}
	}
	if (verbose) fprintf(stderr, "%s 99 rc %d\n", _PFN, rc);
	return rc;
}

void StreamHeadLivePP::stream() {
	while(1){
		int rc = _stream();
		if (rc == 0){
			continue;
		}
		if (rc == -1){
			switch(errno){
			case EINTR:
				fprintf(stderr, "%s quit:EINTR\n", _PFN); break;
			case EAGAIN:
				fprintf(stderr, "%s quit:EAGAIN\n", _PFN); break;
			default:
				fprintf(stderr, "%s quit error:%d\n", _PFN, errno); break;
			}
		}
	}
}

typedef std::vector<MapBuffer*> MBUFV;
typedef std::vector<MapBuffer*>::iterator MBUFV_IT;
typedef std::vector<FILE *> FPV;
typedef std::vector<FILE *>::iterator FPV_IT;


#define TRANSOUT	OBS_ROOT"/ch"



/* TRANSIENT DEMUX */

class Demuxer {
protected:
	static int verbose;
	const int wordsize;
	int bufstep;

	struct BufferCursor {
		const int channel_buffer_bytes;	/* number of bytes per channel per buffer */
		int ibuf;
		char* base;
		char* cursor;

		int channel_remain_bytes() {
			return channel_buffer_bytes - (cursor - base);
		}
		int total_remain_bytes() {
			return channel_remain_bytes() * G::nchan;
		}
		void init(int _ibuf) {
			ibuf = _ibuf;
			base = cursor = Buffer::the_buffers[ibuf]->getBase();
			if (verbose)
				fprintf(stderr, "BufferCursor::init(%d) base:%p\n", ibuf, base);
		}
		BufferCursor(int _cbb) :
			channel_buffer_bytes(_cbb)
		{
			init(0);
		}
	} dst;
	const int channel_buffer_sam() {
		return dst.channel_buffer_bytes/wordsize;
	}
	/* watch out for end of source buffer ! */
	virtual int _demux(void* start, int nbytes) = 0;
	Demuxer(int _wordsize, int _cbb) : wordsize(_wordsize), bufstep(1), dst(_cbb) {
		if (verbose) fprintf(stderr, "Demuxer: verbose=%d\n", verbose);
	}
public:
	virtual ~Demuxer() {}

	virtual int demux(void *start, int nbytes);
	int operator()(void *start, int nbytes){
		if (verbose) fprintf(stderr, "%s %p %d\n", _PFN, start, nbytes);
		return demux(start, nbytes);
	}
	static int _msync (void *__addr, size_t __len, int __flags)
	{
		int rc;
		if (verbose) fprintf(stderr,
			"DemuxerImpl::_msync() 01 %p %d %08x\n",
			__addr, __len, __flags);

		rc = msync(__addr, __len, __flags);


		if (verbose) fprintf(stderr, "DemuxerImpl::_msync() 99 rc %d\n", rc);

		return rc;
	}
	static Demuxer* instance(enum DemuxBufferType N, unsigned WS = sizeof(short));
};

int Demuxer::verbose = getenv_default("DemuxerVerbose");



template <class T>
class DemuxerImpl : public Demuxer {

	virtual int _demux(void* start, int nbytes);
	AbstractES& evX;
	FILE* esf;


protected:
	DemuxerImpl() : Demuxer(sizeof(T), Buffer::bufferlen/G::nchan),
		evX(*AbstractES::evX_instance())
	{
		if (verbose) fprintf(stderr, "%s %d\n", _PFN, dst.channel_buffer_bytes);
		esf = fopen("/dev/shm/es.log", "w");
	}
public:

	virtual ~DemuxerImpl() {
		fclose(esf);
	}
	friend class Demuxer;
};

template <class T>
int DemuxerImpl<T>::_demux(void* start, int nbytes){
	const int nsam = b2s(nbytes);
	const int cbs = channel_buffer_sam();
	T* pdst = reinterpret_cast<T*>(dst.cursor);
	T* psrc = reinterpret_cast<T*>(start);
	T* psrc0 = psrc;
	int b1 = MapBuffer::getBuffer(reinterpret_cast<char*>(psrc));
	const int step = G::double_up? 2: 1;

	if (verbose) fprintf(stderr, "%s (%p %d) b:%d %p = %p * nsam:%d cbs:%d\n",
			_PFN, start, nbytes, b1, pdst, psrc, nsam, cbs);

	if (verbose) fprintf(stderr, "%s step %d double_up %d nchan:%d\n", _PFN, step, G::double_up, G::nchan);

	for (int sam = 0; sam < nsam; sam += step, psrc += G::nchan*step){
		if (psrc >= psrc0 + Buffer::bufferlen){
			int b2 = MapBuffer::getBuffer(reinterpret_cast<char*>(psrc));
			if (b1 != b2){
				// @TODO does this ever happen?
				if (verbose) fprintf(stderr, "%s new buffer %03d -> %03d\n", _PFN, b1, b2);
				Progress::instance().print(true, b1);
				b1 = b2;
				psrc0 = psrc;
			}
		}

		if (evX.isES(reinterpret_cast<unsigned*>(psrc))){
			unsigned p1 = reinterpret_cast<unsigned>(psrc);
			p1 -= reinterpret_cast<unsigned>(MapBuffer::get_ba_lo());
			fwrite(&p1, sizeof(unsigned), 1, esf);
			p1 /= G::nchan*sizeof(T);
			fwrite(&p1, sizeof(unsigned), 1, esf);
			fwrite(psrc, sizeof(T), G::nchan, esf);
		}
		T* psrct = psrc;
		for (unsigned chan = 0; chan < G::nchan; ++chan){
			pdst[chan*cbs+sam] = *psrct++;
			if (step == 2){
				pdst[chan*cbs+sam+1] = *psrct++;
			}
		}
	}
	dst.cursor = reinterpret_cast<char*>(pdst+nsam);

	Progress::instance().print(true, b1);

	if (verbose) fprintf(stderr, "%s return %d\n", _PFN, nbytes);
	return nbytes;
}


template <DemuxBufferType N>
class DualAxiDemuxerImpl : public Demuxer {
	virtual int _demux(void* start, int nbytes);
	int inst_chan;
	FILE* fp_decims;
	u8 *decims;
	u8 *decims_cursor;
protected:
	DualAxiDemuxerImpl() : Demuxer(sizeof(short), 2*Buffer::bufferlen/G::nchan),
		inst_chan(-1)
	{
		bufstep = 2;
		if (getenv("DualAxiDemuxerInstChan")){
			inst_chan = atoi(getenv("DualAxiDemuxerInstChan")) - 1;
		}
		if (verbose) fprintf(stderr, "%s %d\n", _PFN, dst.channel_buffer_bytes);
	}
public:
	virtual int demux(void* start, int nbytes);

	virtual ~DualAxiDemuxerImpl() {
		if (verbose) fprintf(stderr, "%s\n", _PFN);
	}
	friend class Demuxer;
};



void Demuxer_report(int buffer);

static bool DualAxiDemuxerImpl_DB_2D_REGULAR_nopullback;

Demuxer* Demuxer::instance(enum DemuxBufferType N, unsigned WS)
{
	switch(N){
	case DB_REGULAR:
		switch(WS){
		case sizeof(short):
			return new DemuxerImpl<short>;
		case sizeof(int):
			return new DemuxerImpl<int>;
		default:
			assert(WS==sizeof(short)||WS==sizeof(int));
			return 0;
		}
		break;
	case DB_2D_REGULAR:
		assert(WS==sizeof(short));
		DualAxiDemuxerImpl_DB_2D_REGULAR_nopullback =
				getenv_default("DB_2D_REGULAR_NOPULLBACK", 0);
		return new DualAxiDemuxerImpl<DB_2D_REGULAR>;
	case DB_DOUBLE:
		assert(WS==sizeof(short));
		return new DemuxerImpl<int>;
	case DB_2D_DOUBLE:
		return new DualAxiDemuxerImpl<DB_2D_DOUBLE>;
	default:
		assert(0);
	}
}
int interesting(int sam, int nsam)
{
	return sam < 4 || sam > (nsam-6);
}
short ramp;
short done_ramping;

template <DemuxBufferType N>
int DualAxiDemuxerImpl<N>::demux(void* start, int nbytes){
	return ::Demuxer::demux(start, nbytes);
}


template <DemuxBufferType N>
int  DualAxiDemuxerImpl<N>::_demux(void* start, int nbytes) {
	assert(2+2 == 5);
}

#define VERBOSE_INNER_LOOP 0

template <>
int  DualAxiDemuxerImpl<DB_2D_DOUBLE>::_demux(void* _start, int nbytes) {
	const int nsam = b2s(nbytes);
	const int cbs = channel_buffer_sam()/2;
	char* start = reinterpret_cast<char*>(_start);
	short* pdst = reinterpret_cast<short*>(dst.cursor);
	int nsrc = MAX(nbytes/sizeof(short), 2*Buffer::bufferlen/sizeof(short));
	short* srcbuf = new short[nsrc];
	short* psrc = srcbuf;
	int b1 = MapBuffer::getBuffer(reinterpret_cast<char*>(start));
	const unsigned step = G::double_up? 2: 1;
	const unsigned NC = G::nchan;
	const unsigned NC2 = G::nchan/2;
	const unsigned SRCINC = step==2? NC: NC2;
	const int BUFSHORTS = Buffer::bufferlen/sizeof(short);
	int change_over = 0;

	memcpy(psrc, start, nsrc*sizeof(short));

	if (verbose) fprintf(stderr, "DualAxiDemuxerImpl<DB_2D_DOUBLE>::_demux()\n");
	if (verbose) fprintf(stderr, "%s (%p %d) b:%d %p = %p * nsam:%d cbs:%d\n",
			_PFN, start, nbytes, b1, pdst, psrc, nsam, cbs);

	if (verbose) fprintf(stderr, "%s step %d double_up %d nchan:%d\n", _PFN, step, G::double_up, G::nchan);

	for (int sam = 0, sambuf = 0; sam < nsam; sam += step, sambuf += step, psrc += SRCINC){
		if (sambuf == cbs){
			if (verbose) fprintf(stderr, "%s step pdst from %p up %d to %p sam=%d\n",
						_PFN, pdst, BUFSHORTS, pdst+BUFSHORTS, sam);
			pdst += BUFSHORTS;	// step to next destination buffer
			sambuf = 0;
			if (++change_over == 2){
				psrc = srcbuf;
				memcpy(psrc, start += 2*Buffer::bufferlen, nsrc*sizeof(short));
				change_over = 0;
				if (verbose) fprintf(stderr, "%s reload srcbuf from %p\n", _PFN, start);
			}
		}
		unsigned chan = 0;
		// harvest first half channels from first src buf
		for (short* psrct = psrc; chan < NC2; ++chan){
#if VERBOSE_INNER_LOOP
			if (verbose > 1 && interesting(sambuf, cbs)) fprintf(stderr, "%s [%d][%7d] pdst[%6d] = [%7d]\n", _PFN, chan, sam, chan*cbs+sambuf, psrct-srcbuf);
#endif
			pdst[chan*cbs+sambuf] = *psrct++;
			if (step==2) pdst[chan*cbs+sambuf+1] = *psrct++;
		}
		// harvest second half channels from second src buf
		for (short* psrct = psrc + BUFSHORTS; chan < NC; ++chan){
#if VERBOSE_INNER_LOOP
			if (verbose > 1 && interesting(sambuf, cbs)) fprintf(stderr, "%s [%d][%7d] pdst[%6d] = [%7d]\n", _PFN, chan, sam, chan*cbs+sambuf, psrct-srcbuf);
#endif
			pdst[chan*cbs+sambuf] = *psrct++;
			if (step==2) pdst[chan*cbs+sambuf+1] = *psrct++;
		}
#ifdef BUFFER_IDENT
		if (inst_chan >= 0){
			pdst[inst_chan*cbs+sambuf] = ++ramp;
			if (step==2) pdst[inst_chan*cbs+sambuf+1] = ++ramp;
		}
#endif
	}


	if (verbose) fprintf(stderr, "%s set cursor %p + %d => %p\n", _PFN, dst.cursor, nbytes, dst.cursor+nbytes);
	dst.cursor += nbytes/G::nchan;

	Progress::instance().print(true, b1);

	if (verbose) fprintf(stderr, "%s return %d\n", _PFN, nbytes);

	delete [] srcbuf;
	return nbytes;
}

static char getMR10DEC(void){
	unsigned mr10 = 3;
	getKnob(0, "/etc/acq400/0/NCHAN", &mr10);
	switch (mr10){
	default:
	case 3:
		return 32;
	case 2:
		return 16;
	case 1:
		return 8;
	case 0:
		return 4;
	}
}

/**
 * DB_2D_REGULAR : wrap ::demux(), create decims array. adjust the decims to fit real hardware.
 */
template<>
int DualAxiDemuxerImpl<DB_2D_REGULAR>::demux(void* start, int nbytes){
	int nsam = nbytes/sample_size();
	const u8 decim_rates[] = {
			[0] = 2,
			[1] = 1,
			[2] = getMR10DEC(),
			[3] = 2
		};

	if (verbose) fprintf(stderr, "%s open fp_decims %d\n", _PFN, nsam);
	if (verbose) fprintf(stderr, "%s decim_rates %u,%u,%u,%u\n", _PFN,
				decim_rates[0], decim_rates[1], decim_rates[2], decim_rates[3]);

	fp_decims = fopen("/dev/shm/decims", "w");
	assert(fp_decims);
	decims_cursor = decims = new u8[nsam];

	int rc = ::Demuxer::demux(start, nbytes);

	const unsigned last = decims_cursor - decims;
	const unsigned notouch = DualAxiDemuxerImpl_DB_2D_REGULAR_nopullback? last: 2;
	unsigned ii = 0;
	u8 *decims_out = new u8[nsam];
	for (; ii < notouch; ++ii){
		decims_out[ii] = decim_rates[decims[ii]];
	}
	for (; ii < last; ++ii){			// does not exec if notouch==last
		u8 id = decims[ii];
		if (id == 2){
			if (decims[ii-2] == 1){			// SPRINT->JOG : pull 2
				id = 1;
			}else if (decims[ii-1] == 0){		// RUN->JOG: pull 1
				id = 2;
			}
		}
		decims_out[ii] = decim_rates[id];
	}

	int nw = fwrite(decims_out, sizeof(u8), nsam, fp_decims);
	if (verbose) fprintf(stderr, "%s closing down fp_decims %d\n", _PFN, nw);
	fclose(fp_decims);
	delete [] decims_out;
	delete [] decims;
	return rc;
}


template <>
int  DualAxiDemuxerImpl<DB_2D_REGULAR>::_demux(void* _start, int nbytes) {
	const unsigned BUFSHORTS = Buffer::bufferlen/sizeof(short);
	const unsigned NC2 = G::nchan/2;

	const int nsam = b2s(nbytes);
	const int cbs = channel_buffer_sam()/2;
	char* start = reinterpret_cast<char*>(_start);
	short* pdst = reinterpret_cast<short*>(dst.cursor);
	unsigned nsrc = MAX(nbytes/sizeof(short), 2*BUFSHORTS);
	short* srcbuf = new short[nsrc];
	short* psrc = srcbuf;
	int b1 = MapBuffer::getBuffer(reinterpret_cast<char*>(start));
	int change_over = 0;

	memcpy(psrc, start, nsrc*sizeof(short));

	if (verbose) fprintf(stderr, "DualAxiDemuxerImpl<DB_2D_REGULAR>::_demux()\n");
	if (verbose) fprintf(stderr, "%s (%p %d) b:%d %p = %p * nsam:%d cbs:%d\n",
			_PFN, start, nbytes, b1, pdst, psrc, nsam, cbs);

	for (int sam = 0, sambuf = 0; sam < nsam; sam += 1, sambuf += 1, psrc += NC2){
		if (sambuf >= cbs){
			if (verbose) fprintf(stderr, "%s step pdst from %p up %d to %p sam=%d\n",
						_PFN, pdst, BUFSHORTS, pdst+BUFSHORTS, sam);
			pdst += BUFSHORTS;	// step to next destination buffer
			sambuf = 0;
			if (++change_over == 2){
				psrc = srcbuf;
				memcpy(psrc, start += 2*Buffer::bufferlen, nsrc*sizeof(short));
				change_over = 0;
				if (verbose) fprintf(stderr, "%s reload srcbuf from %p\n", _PFN, start);
			}
		}
		unsigned chan = 0;
		short* psrct = psrc;
		// harvest first half channels from first src buf
		for (; chan < 1; ++chan){
			short xx = *psrct++;
			*decims_cursor++ = xx&0x3;
			pdst[DB_2D_R_MEM_PIN_LUT_48_DMA0[chan]*cbs+sambuf] = xx & 0xFFFC;
		}
		for (; chan < 8; ++chan){
			short xx = *psrct++;
			pdst[DB_2D_R_MEM_PIN_LUT_48_DMA0[chan]*cbs+sambuf] = xx & 0xFFFC;
		}
		for ( ; chan < NC2; ++chan){
#if VERBOSE_INNER_LOOP
			if (verbose > 1 && interesting(sambuf, cbs)) {
				fprintf(stderr, "%s [%d][%7d] pdst[%6d] = [%7d]\n",
					_PFN, DB_2D_R_MEM_PIN_LUT_48_DMA0[chan], sam, chan*cbs+sambuf, psrct-srcbuf);
			}
#endif
			pdst[DB_2D_R_MEM_PIN_LUT_48_DMA0[chan]*cbs+sambuf] = *psrct++;
		}
		// harvest second half channels from second src buf
		chan = 0;
		for (psrct = psrc + BUFSHORTS; chan < NC2; ++chan){
#if VERBOSE_INNER_LOOP
			if (verbose > 1 && interesting(sambuf, cbs)){
				fprintf(stderr, "%s [%d][%7d] pdst[%6d] = [%7d]\n",
					_PFN, DB_2D_R_MEM_PIN_LUT_48_DMA1[chan], sam, chan*cbs+sambuf, psrct-srcbuf);
			}
#endif
			pdst[DB_2D_R_MEM_PIN_LUT_48_DMA1[chan]*cbs+sambuf] = *psrct++;
		}
#ifdef BUFFER_IDENT
		if (inst_chan >= 0){
			pdst[inst_chan*cbs+sambuf] = ++ramp;
		}
#endif
	}


	if (verbose) fprintf(stderr, "%s set cursor %p + %d => %p\n", _PFN, dst.cursor, nbytes, dst.cursor+nbytes);
	dst.cursor += nbytes/G::nchan;

	Progress::instance().print(true, b1);

	if (verbose) fprintf(stderr, "%s return %d\n", _PFN, nbytes);
	if (verbose) fprintf(stderr, "%s delete [] %p\n", _PFN, srcbuf);
	delete [] srcbuf;
	return nbytes;
}


int Demuxer::demux(void* start, int nbytes)
/* return bytes demuxed */
{
	char* startp = reinterpret_cast<char*>(start);
	int rc = 0;

	if (verbose) fprintf(stderr, "%s 01 (%p %d)\n", _PFN, start, nbytes);

	assert(nbytes%sample_size() == 0);

	while(nbytes){
		int trb = dst.total_remain_bytes();

		if (verbose) fprintf(stderr, "%s 10 nbytes:%d trb:%d\n", _PFN, nbytes, trb);

		if (nbytes < trb){
			rc += _demux(startp, nbytes);
			nbytes = 0;
		}else{
			int nb = _demux(startp, trb);
			// assert(nb == trb);
			dst.init(dst.ibuf+bufstep);
			nbytes -= trb;
			startp += nb;
			rc += nb;
			if (verbose) fprintf(stderr, "%s 50 rc:%d startp:%p\n", _PFN, rc, startp);
		}
	}
	if (verbose) fprintf(stderr, "%s 99  rc:%d\n", _PFN, rc);
	return rc;
}

/* @@todo .. map to shm area for external monitoring. */



long Progress::min_report_interval = getenv_default("MIN_REPORT_INTERVAL_MS", MIN_REPORT_INTERVAL_MS);

Progress& Progress::instance(FILE *fp) {
	static Progress* _instance;

	/*
	if (!_instance){
		_instance = new Progress;

	}
	*/
	if (_instance == 0){
		if (verbose) fprintf(stderr, "Progress:instance() : size %d\n", sizeof(Progress));

		int rc = shmget(0xdeadbeef, sizeof(Progress), IPC_CREAT|0666);
		if (rc == -1){
			perror("shmget()");
			exit(1);
		}else{
			void *shm = shmat(rc, 0, 0);
			if (shm == (void*)-1){
				perror("shmat()");
				exit(1);
			}else{
				Progress *p = new ProgressImpl(fp? fp: stdout);
				_instance = (Progress*)shm;
				memcpy(_instance, p, sizeof(Progress));
			}
		}

	}

	if (verbose) fprintf(stderr,"Progress::instance() %p pid:%d\n", _instance, getpid());
	return *_instance;
}


void acq400_stream_getstate(void)
{
	Progress::instance().print();
	exit(0);
}

#define BLOG	"/etc/acq400/0/full_buffers"

class BufferLog {
	int ib0;
	bool all_full;
	FILE* blog;

public:
	BufferLog() : ib0(-1), all_full(false) {
		blog = fopen(BLOG, "w");
		if (blog == 0){
			perror(BLOG);
			exit(1);
		}
	}
	~BufferLog() {
		fprintf(blog, "\n");
		fclose(blog);
	}
	bool update(int ib){
		if (ib0 == -1){
			ib0 = ib;
			all_full = false;
		}else if (ib == ib0){
			all_full = true;
		}
		if (!all_full){
			fprintf(blog, "%03d ", ib);
		}
		return all_full;
	}
};



class StreamHeadClient {
protected:
	int start;
	int len;

	void getStartLen(const char* def) {
		int args[2];
		switch (sscanf(def, "%d,%d", args, args+1)){
		case 2:
			start = args[0]>=1? args[0]-1: 0;
			len = args[1];
			break;
		case 1:
			start = 0;
			len = args[0];
			break;
		default:
			start = 0;
			len = G::nchan;
		}
	}

public:
	virtual void onStreamStart() = 0;
	virtual void onStreamBufferStart(int ib) = 0;
	virtual void onStreamEnd() = 0;
	virtual ~StreamHeadClient() {}
};
typedef std::vector<StreamHeadClient*>::iterator  IT;

class SumStreamHeadClient: public StreamHeadClient {

protected:
	int outfd;
public:
	SumStreamHeadClient(const char* def) {
		getStartLen(def);
		outfd = open("/dev/shm/sumstreamclient", O_WRONLY);
	}
	virtual void onStreamStart() {}
	virtual void onStreamBufferStart(int ib) {}
	virtual void onStreamEnd() {}
	virtual ~SumStreamHeadClient() {}

	static 	SumStreamHeadClient* instance(const char* def);
};

template <class T>
class SumStreamHeadClientImpl: public SumStreamHeadClient {
	int* sum_buf;
	int nbuf;
public:
	SumStreamHeadClientImpl<T>(const char* def) : SumStreamHeadClient(def)
	{
		nbuf = Buffer::bufferlen/sample_size();
		sum_buf = new int[nbuf];
	}
	virtual void onStreamBufferStart(int ib) {
		Buffer* buffer = Buffer::the_buffers[ib];
		T* data = reinterpret_cast<T*>(buffer->getBase());
		T* end = reinterpret_cast<T*>(buffer->getEnd());
		int totchan = G::nchan;
		int sum;
		int shr = sizeof(T) == 4? 8: 0;


		for (int isam = 0; data < end; data += totchan, ++isam){
			int ic;
			for (ic = start, sum = 0; ic < start+len; ++ic){
				sum += data[ic] >> shr;
			}
			sum_buf[isam] = sum;

		}
		write(outfd, sum_buf, nbuf*sizeof(int));
	}
};

SumStreamHeadClient* SumStreamHeadClient::instance(const char* def) {
	if (G::wordsize == 4){
		return new SumStreamHeadClientImpl<int>(def);
	}else{
		return new SumStreamHeadClientImpl<short>(def);
	}
}

class SubsetStreamHeadClient: public StreamHeadClient {
protected:

public:
	SubsetStreamHeadClient(const char* def) {
		getStartLen(def);
	}
	virtual void onStreamStart() {}
	virtual void onStreamBufferStart(int ib) = 0;
	virtual void onStreamEnd() {}
	virtual ~SubsetStreamHeadClient() {}

	static SubsetStreamHeadClient* instance(const char* def);
};

template <class T>
class SubsetStreamHeadClientImpl: public SubsetStreamHeadClient {

protected:
	T* buf;
	int nbuf;
	int totchan;
	AbstractES& evX;
	int es_log;
public:
	SubsetStreamHeadClientImpl<T>(const char* def) :
		SubsetStreamHeadClient(def),
		evX(*AbstractES::evX_instance())
	{
		nbuf = Buffer::bufferlen/sample_size()*len;
		buf = new T [nbuf];

		totchan = G::nchan;
		if (G::double_up){
			totchan *= 2;
			start *= 2;
			len *= 2;
			fprintf(stderr, "%s totchan %d start %d len %d\n",
					_PFN, totchan, start, len);
		}
	}
	virtual void onStreamBufferStart(int ib) {
		Buffer* buffer = Buffer::the_buffers[ib];
		T* data = reinterpret_cast<T*>(buffer->getBase());
		T* end = reinterpret_cast<T*>(buffer->getEnd());
		T* pb = buf;

		for (int isam = 0; data < end; data += totchan, ++isam, pb += len){
			if (evX.isES(reinterpret_cast<unsigned*>(data))){
				write(STDOUT, buf, (pb-buf)*sizeof(T));
				write(STDOUT, data, totchan*sizeof(T));
				pb = buf;
			}else{
				memcpy(pb, data+start, len*sizeof(T));
			}
		}
		write(STDOUT, buf, (pb-buf)*sizeof(T));
	}
};


const unsigned MAXBIT = 256;
typedef std::bitset<MAXBIT> ChannelMask;

int get_channelcount(ChannelMask& cm){
        int nset = 0;
        for (int ii = 0; ii < 256; ++ii){
                if (cm[ii]){
                        ++nset;
                }
        }
        return nset;
}

int get_topchan(ChannelMask& cm){
        int topchan = 0;
        for (int ii = 0; ii < 256; ++ii){
                if (cm[ii]){
                        topchan = ii;
                }
        }
        return topchan;
}
int get_firstchan(ChannelMask& cm){
	for (int ii = 0; ii < 256; ++ii){
		if (cm[ii]) return ii;
	}
	return 0;
}

template <class T>
class SubsetStreamHeadClientChannelMask: public SubsetStreamHeadClientImpl<T> {
	ChannelMask& cm;
	const int topchan;
	const int firstchan;

	static char* _cm2def(ChannelMask& _cm){
		char* def = new char[32];
		snprintf(def, 32, "%d", get_channelcount(_cm));
		return def;
	}
	T* csample;		// contiguous sample bounce buffer

	T* gather(T* sample){
		T* pdst = csample;
		for (int ii = firstchan; ii <= topchan; ++ii){
			if (cm[ii]){
				*pdst++ = sample[ii];
			}
		}
		return csample;
	}
	static int verbose;
public:
	SubsetStreamHeadClientChannelMask<T>(ChannelMask& _cm):
		SubsetStreamHeadClientImpl<T>(_cm2def(_cm)),
		cm(_cm),
		topchan(get_topchan(_cm)),
		firstchan(get_firstchan(_cm)){
		csample = new T[this->len];

		if (verbose){
			fprintf(stderr, "%s firstchan:%d topchan:%d len:%d totchan:%d\n",
					__FUNCTION__, firstchan, topchan, this->len, this->totchan);
		}
	}
	virtual void onStreamBufferStart(int ib) {
		Buffer* buffer = Buffer::the_buffers[ib];

		T* data = reinterpret_cast<T*>(buffer->getBase());
		T* end = reinterpret_cast<T*>(buffer->getEnd());
		T* pb = this->buf;
		T* pb0 = this->buf;
		const int totchan(this->totchan);
		const int len(this->len);

		for (int isam = 0; data < end; data += totchan, ++isam, pb += len){
			memcpy(pb, gather(data), len*sizeof(T));
		}
		write(STDOUT, pb0, (pb-pb0)*sizeof(T));
	}
};

template <class T>
int SubsetStreamHeadClientChannelMask<T>::verbose = getenv_default("SubsetStreamHeadClientChannelMaskVerbose");

SubsetStreamHeadClient* SubsetStreamHeadClient::instance(const char* def)
{
	ChannelMask *cm;

	if (strncmp(def, "0x", 2) == 0){
		cm = new ChannelMask(hexStrToBin(def+2));  // arbitrary mask, hex definition
	}else if (strncmp(def, "0b", 2) == 0){
		cm = new ChannelMask(def+2);		   // arbitrary mask, binary definition
	}else{
		cm = 0;					   // else range [from] to
	}

	if (cm){
		if (G::wordsize == 4){
			return new SubsetStreamHeadClientChannelMask<int>(*cm);    // arbitrary mask <int>
		}else{
			return new SubsetStreamHeadClientChannelMask<short>(*cm);  // arbitrary mask <short>
		}
	}else{
		if (G::wordsize == 4){
			return new SubsetStreamHeadClientImpl<int>(def);	   // range <int>
		}else{
			return new SubsetStreamHeadClientImpl<short>(def);	   // range <short>
		}
	}
	fprintf(stderr, "%s ERROR: unable to create instance\n", __FUNCTION__);
	assert(0);
}


struct Segment {
	char* base; int len;
	Segment(char* _base, int _len):
		base(_base), len(_len) {}
};

typedef vector<Segment>::iterator SegmentIterator;

template <class T>
void dump(T* src, int nwords){
	char cmd[80];
	sprintf(cmd, "hexdump -ve '%d/%d \"%%08x \" \"\\n\" ' >/tmp/es",
						G::nchan, sizeof(T));
	FILE *pp = popen(cmd, "w");
	fwrite(src, sizeof(T), nwords, pp);
	pclose(pp);
}


class BufferDistribution {

private:
	vector<Segment> segments;

	vector<Segment>& _getSegments();

	void sortToLinearBuffer(char* start, char* finish);
	void getSegmentsLinear();
	void getSegmentsPrecorner();
	void getSegmentsPostcorner();

public:
	int ibuf;
	char* esp;
	char* ba_lo;
	char* ba_hi;
	int tailroom;
	int headroom;
	bool pre_fits;
	bool post_fits;
	static int verbose;
	int bd_scale;			/* set 2 for dual-buffer (stride back at half rate) */

	BufferDistribution(int _ibuf, char *_esp, int _bd_scale = 1) :
		ibuf(_ibuf), esp(_esp),
		ba_lo(MapBuffer::get_ba_lo()),
		ba_hi(MapBuffer::get_ba_hi()),
		tailroom(b2s(esp - ba_lo)),
		headroom(b2s(ba_hi - esp)),
		pre_fits(tailroom >= G::pre),
		post_fits(headroom >= G::post),
		bd_scale(_bd_scale)
	{
		if (verbose) fprintf(stderr, "BufferDistributionVerbose\n");
	}
	void show() {
		fprintf(stderr, "%s pre_fits:%d post_fits:%d\n", _PFN, pre_fits, post_fits);

		if (pre_fits&&post_fits){
			fprintf(stderr, "linear: %p |%p| %p\n",
				esp - s2b(G::pre), esp, esp + s2b(G::post));
			fprintf(stderr, "buffers:%s | %s | %s\n",
			MapBuffer::listBuffers(esp - s2b(G::pre), esp),
			MapBuffer::listBuffers(esp, esp),
			MapBuffer::listBuffers(esp, esp + s2b(G::post)));
			if (verbose > 1){
				fprintf(stderr, "buffers:%s | %s | %s\n",
				MapBuffer::listBuffers(esp - s2b(G::pre), esp, true),
				MapBuffer::listBuffers(esp, esp, true),
				MapBuffer::listBuffers(esp, esp + s2b(G::post), true));
			}

			if (G::wordsize == 4){
				dump<unsigned>(reinterpret_cast<unsigned*>(esp), G::nchan);
			}else{
				dump<unsigned>(reinterpret_cast<unsigned*>(esp), G::nchan/2);
			}

		}else if (!pre_fits){
			fprintf(stderr, "precorner: %p-%p, %p |%p| %p\n",
					ba_hi-s2b(G::pre-tailroom), ba_hi,
					ba_lo,
					esp,
					esp + s2b(G::post));
		}else{
			fprintf(stderr, "postcorner: %p |%p| %p %p-%p\n",
					esp - s2b(G::pre),
					esp,
					ba_hi,
					ba_lo, ba_lo + s2b(G::post-headroom));
		}
		fprintf(stderr, "%s 99\n", _PFN);

	}
	void report() {
		FILE *fp = fopen("/dev/shm/estime", "w");
		if (fp == 0){
			perror("/dev/shm/estime");
			return;
		}
		fprintf(fp, "%d\n", reinterpret_cast<unsigned*>(esp)[4]);
		fclose(fp);
		fp = fopen("/dev/shm/esbin", "w");
		if (fp == 0){
			perror("/dev/shm/esbin");
			return;
		}
		fwrite(esp, 1, sample_size(), fp);
		fclose(fp);
	}


	enum BD_MODE { BD_ERROR=-1, BD_LINEAR, BD_PRECORNER, BD_POSTCORNER };

	enum BD_MODE mode() const {
		if (pre_fits&&post_fits){
			return BD_LINEAR;
		}
		if (!pre_fits && !post_fits){
			return BD_ERROR;
		}else if (!pre_fits){
			return BD_PRECORNER;   // ESP is AFTER CORNER
		}else{
			return BD_POSTCORNER;  // ESP is BEFORE CORNER
		}
	}

	const char* showMode() {
		switch(mode()){
		case BD_LINEAR: 	return "BD_LINEAR";
		case BD_PRECORNER:	return "BD_PRECORNER";
		case BD_POSTCORNER:	return "BD_POSTCORNER";
		default:
			return "ERROR bad mode";
		}
	}

	void showSegments() {
		int seg = 0;
		for (SegmentIterator it = getSegments().begin();
				it != getSegments().end(); ++it, ++seg){
			int len = (*it).len;
			char *b0 = (*it).base;
			char *b1 = b0+len-1;

			fprintf(stderr, "%s segments %d %p..%p %d buffer:%03d ..%03d\n", _PFN,
					seg, b0, b1, len, MapBuffer::getBuffer(b0), MapBuffer::getBuffer(b1));
		}
	}
	vector<Segment>& getSegments();
};

int BufferDistribution::verbose = getenv_default("BufferDistributionVerbose");

void StreamHeadImpl::report(const char* id, int ibuf, char *esp){
	if (!G::report_es) return;
	fprintf(stderr, "StreamHeadPrePost::report: buffer:%s [%d] esp:%p\n",
			id, ibuf, esp);
	fprintf(stderr, "Buffer length bytes: %d\n", Buffer::bufferlen);
	fprintf(stderr, "Buffer length samples: %d\n", Buffer::samples_per_buffer());
	fprintf(stderr, "Prelen: %d\n", G::pre);
	fprintf(stderr, "Postlen: %d\n", G::post);

	BufferDistribution(ibuf, esp).show();
}

/*
 * Buffer Memory:
 *
 * BD_LINEAR: TRIVIAL
 * Before:
 *            R R R R S S S
 * |-|-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
 * 0-L-----------------------------------------------H1
 * After:
 *  R R R R S S S
 *
 *
 * BD_PRECORNER:
 * Before:
 *    R R S S S                                     R
 * |-|-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
 * 0-L-----------------------------------------------H1
 *
 * Shuffle:
 *    R R R S S S
 * |-|-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
 * 0-L-----------------------------------------------H1
 *
 */



void BufferDistribution::getSegmentsLinear()
{
	int prebytes = s2b(G::pre);
	int postbytes = s2b(G::post);
	int esp_decr = prebytes;	/* wind esp back this far */

	if (verbose) fprintf(stderr, "%s bd_scale:%d\n", _PFN, bd_scale);

	if (bd_scale == 2){
	/* This is a little _tricky_ ...
	 * channels are split over two buffers ..
	 * we have to do special stuff to comp for E,O buffers
	 *  [RE][RO][FE][FO][FE][FO][EE][EO]
	 *  EE, OE : the ESP is here, but the data set spans multiple buffers pre
	 *  FE, FO : Even, Odd complete buffers: 0..N, MUST be pairs.
	 *  RR, RO : Start buffer pair with residue pre.
	 *
	 *  (1) calc bytes before ES at EE buffer start
	 *  (2) calc number of full buffers pre
	 *  (3) calc residue in RE
	 *  (4) wind back nb+2 to get to the beginning of RE
	 *  (5) start at END RE less residue
	 *
	 *  (6) alternatively, if it all fits in EE, then simply index back/bd_scale
	 *  .. scale down because half the data is in EO ..
	 */
		int ib = MapBuffer::getBuffer(esp);
		int offbytes = esp - MapBuffer::ba(ib);

		if (verbose > 1) fprintf(stderr, "%s (0) ib:%d offbytes:%d esp_decr %d\n", _PFN, ib, offbytes, esp_decr);

		if (prebytes/bd_scale > offbytes){
			if (verbose) fprintf(stderr, "%s MULTI\n", _PFN);
/* (1) */		int bbbs = prebytes - offbytes/bd_scale;
/* (2) */		int nb = bbbs / MapBuffer::bufferlen;
/* (3) */		int residue = bbbs - nb*MapBuffer::bufferlen;

			if (verbose > 1) fprintf(stderr, "%s (3) bbs:%d nb:%d residue:%d\n", _PFN, bbbs, nb, residue);

/* (4) */		esp_decr = offbytes + (nb+2)*MapBuffer::bufferlen;
/* (5) */		esp_decr += MapBuffer::bufferlen - residue/bd_scale;
		}else{
			if (verbose) fprintf(stderr, "%s SAME\n", _PFN);
			if (verbose > 1) fprintf(stderr, "%s (6) offbytes:%d esp_decr %d->%d\n", _PFN, offbytes, esp_decr, esp_decr/bd_scale);
/* (6) */		esp_decr /= bd_scale;
		}
	}
	segments.push_back(Segment(esp-esp_decr, prebytes));
	if (G::show_es){
		segments.push_back(Segment(esp, es_size()));
	}
	segments.push_back(Segment(esp+es_size(), postbytes));
}

enum BS_STATE { BS_UNKNOWN = -1, BS_EMPTY, BS_PRE = 0x1, BS_POST = 0x2, BS_ES = 0x3 };

struct BufferSeq {
	static int verbose;
	const unsigned ibuf;
	enum BS_STATE state;
	int iseq;
	char *esp;
	char* ba() {
		return MapBuffer::ba(ibuf);
	}
	BufferSeq(int _ibuf, int _iseq, char* _esp, BS_STATE _state) :
		ibuf(_ibuf),
		state(_state),
		iseq(_iseq), esp(_esp)
	{}
	BufferSeq(int _ibuf):
		ibuf(_ibuf),
		state(BS_UNKNOWN), iseq(0), esp(0)
	{}
	void copy(BufferSeq* from){
		memcpy(ba(), from->ba(), Buffer::bufferlen);
		state = from->state;
		iseq = from->iseq;
		if (from->esp){
			esp = from->esp + (ba() - from->ba());
		}else{
			esp = 0;
		}
	}
	void print() {
		fprintf(stderr, "%s ib:%03d is:%3d state:%d ba:%p esp:%p\n",
				_PFN, ibuf, iseq, state, ba(), esp);
	}

	void clr() {
		iseq = 0;
		state = BS_EMPTY;
		esp = 0;
	}
	static void mv(BufferSeq* from, BufferSeq* to) {
		if (verbose) fprintf(stderr, "%s %03d->%03d\n", _PFN, from->ibuf, to->ibuf);
		to->copy(from);
		from->clr();
	}
	static void swap(BufferSeq* aa, BufferSeq* bb, BufferSeq* tt) {
		if (verbose) fprintf(stderr, "%s %03d,%03d,%03d\n", _PFN, aa->ibuf, bb->ibuf, tt->ibuf);
		tt->copy(aa);
		aa->copy(bb);
		bb->copy(tt);
	}

	static void print(vector<BufferSeq*>& mm){
		vector<BufferSeq*>::const_iterator it;

		for (it = mm.begin(); it != mm.end(); ++it){
			(*it)->print();
		}
	}
	static int findSeq(vector<BufferSeq*>& mm, int iseq){
		vector<BufferSeq*>::const_iterator it;
		for (it = mm.begin(); it != mm.end(); ++it){
			if ((*it)->iseq == iseq){
				if (verbose > 1) fprintf(stderr, "%s iseq %d found at %03d\n", _PFN, iseq, (*it)->ibuf);
				return (*it)->ibuf;
			}
		}
		fprintf(stderr, "%s iseq %d NOT found\n", _PFN, iseq);
		return -1;
	}
	static int findES(vector<BufferSeq*>& mm) {
		vector<BufferSeq*>::const_iterator it;
		for (it = mm.begin(); it != mm.end(); ++it){
			if ((*it)->state == BS_ES){
				if (verbose > 1) fprintf(stderr, "%s ES found at %03d\n", _PFN, (*it)->ibuf);
				return (*it)->ibuf;
			}
		}
		fprintf(stderr, "%s ES NOT found\n", _PFN);
		return -1;
	}
};

int BufferSeq::verbose = getenv_default("BufferSeqVerbose");


void BufferDistribution::sortToLinearBuffer(char* start, char* finish)
{
	vector<BufferSeq*> mm;
	int iseq = 0;

	unsigned ib_esp = MapBuffer::getBuffer(esp);

	/* force mm to use same indices as MapBuffer */
	for (unsigned ib = 0; ib < MapBuffer::getBuffer(ba_lo); ++ib){
		mm.push_back(new BufferSeq(ib));
	}
	for (unsigned ib = MapBuffer::getBuffer(ba_lo);
			ib < MapBuffer::nbuffers; ++ib){
		mm.push_back(new BufferSeq(ib));
	}



	for (unsigned ib = MapBuffer::getBuffer(start); ib < MapBuffer::nbuffers; ++ib){
		assert(mm[ib]->ibuf == ib);
		mm[ib]->iseq = ++iseq;
		if (ib < ib_esp){
			mm[ib]->state = BS_PRE;
		}else if (ib == ib_esp){
			mm[ib]->state = BS_ES;
			mm[ib]->esp = esp;
		}else{					// not pre-corner
			mm[ib]->state = BS_POST;
		}
	}

	unsigned ib_fin = MapBuffer::getBuffer(finish);

	for (unsigned ib = MapBuffer::getBuffer(ba_lo); ib <= ib_fin; ++ib){
		mm[ib]->iseq = ++iseq;
		if (ib_esp < ib_fin && ib < ib_esp){
			mm[ib]->state = BS_PRE;
		}else if (ib == ib_esp){
			mm[ib]->state = BS_ES;
			mm[ib]->esp = esp;
		}else{
			mm[ib]->state = BS_POST;
		}
	}

	if (verbose) BufferSeq::print(mm);

	if (verbose) fprintf(stderr, "%s Assign tmp\n", _PFN);
	BufferSeq* tmp = mm[0];
	if (verbose) tmp->print();

	if (verbose) fprintf(stderr, "%s Now for the swap on sequence length %d\n", _PFN, iseq);

	int iseq99 = iseq;
	for (iseq = 1; iseq <= iseq99; ++iseq){
		int ib_from = BufferSeq::findSeq(mm, iseq);
		assert(ib_from != -1);
		if (mm[iseq]->state == BS_EMPTY){
			BufferSeq::mv(mm[ib_from],mm[iseq]);
		}else{
			BufferSeq::swap(mm[ib_from],mm[iseq], tmp);
		}
		Progress::instance().print(true, -ib_from);
	}
	tmp->clr();		// essential or findES will faind spurious result.
	if (verbose) fprintf(stderr, "%s print sorted seq\n", _PFN);
	if (verbose) BufferSeq::print(mm);

	int ib_es = BufferSeq::findES(mm);
	assert(ib_es != -1);
	esp = mm[ib_es]->esp;

	return getSegmentsLinear();
}
void BufferDistribution::getSegmentsPrecorner()
{
	if (verbose) fprintf(stderr, "%s\n", _PFN);

	char* start = ba_hi-s2b(G::pre-tailroom);
	char* finish = ba_lo+s2b(tailroom)+s2b(G::post);

	return sortToLinearBuffer(start, finish);

}
void BufferDistribution::getSegmentsPostcorner()
{
	if (verbose) fprintf(stderr, "%s()\n", _PFN);

	char* start = esp - s2b(G::pre);
	char* finish = ba_lo + s2b(G::post-headroom);

	return sortToLinearBuffer(start, finish);
}

vector<Segment>& BufferDistribution::getSegments() {
	if (segments.size() == 0){
		switch(mode()){
		case BD_LINEAR:
			getSegmentsLinear();
			break;
		case BD_PRECORNER:
			getSegmentsPrecorner();
			break;
		case BD_POSTCORNER:
			getSegmentsPostcorner();
			break;
		default:
			fprintf(stderr, "%s() ERROR BAD MODE\n", _PFN);
			exit(1);
		}
		if (verbose) showSegments();
	}
	return segments;
}

/* BLock Transfer */
class BLT {
	const char* base;
	char* cursor;
public:
	BLT(char* _cursor) :
		base(_cursor),
		cursor(_cursor)
	{}
	virtual int operator() (char* from, int nbytes){
		if (verbose) fprintf(stderr, "blt() %p %p %d\n", cursor, from, nbytes);
		memcpy(cursor, from, nbytes);
		cursor += nbytes;
		return cursor - base;
	}
};



/*
 *  @@todo buffer_start
 *  pre==0, nodemux : ba0
 *  pre==0, demux   : ba1
 *  pre!=0, nodemux : epos - pre
 *  pre!=0, nodemux : epos - pre
 */

#define NOTIFY_HOOK "/dev/acq400/data/.control"


class StreamHeadWithClients: public StreamHeadImpl {
protected:
	vector <StreamHeadClient*> peers;

public:
	StreamHeadWithClients(Progress& progress) :
			StreamHeadImpl(progress)
	{}
	void addClient(StreamHeadClient* pp){
		if (verbose) fprintf(stderr, "append %p\n", pp);
		peers.push_back(pp);
	}

	virtual void onStreamStart() {
		for (IT it = peers.begin(); it != peers.end(); ++it){
			(*it)->onStreamStart();
		}
	}
	virtual void onStream(int ib){
		for (IT it = peers.begin(); it != peers.end(); ++it){
			if (verbose > 1) fprintf(stderr, "call peer %p %p\n", *it, this);
			(*it)->onStreamBufferStart(ib);
		}
	}
	virtual void onStreamEnd() {
		for (IT it = peers.begin(); it != peers.end(); ++it){
			(*it)->onStreamEnd();
		}
	}
};

class EventController {
	const int site;
	const char* evt;
	char enable_state[10];
	bool valid;
public:
	EventController(int _site, const char* _evt) : site(_site), evt(_evt), valid(false) {
		char event_status[80];
		if (getKnob(site, "event0", event_status) != 1){
			fprintf(stderr, "ERROR: failed to read event0 knob");
		}else{
		// event0=1,0,1 enable d0 RISING
			if (sscanf(event_status, "event0=%s", enable_state) != 1){
				fprintf(stderr, "ERROR: failed to extract enable_state");
			}else{
				valid = true;
			}
		}
	}
	void disable(void) {
		if (valid) setKnob(site, evt, "0,0,0");
	}
	void enable(void) {
		if (valid) setKnob(site, evt, enable_state);
	}
};
class StreamHeadPrePost: public StreamHeadWithClients, StreamHeadClient  {
protected:
	unsigned pre;
	unsigned post;

	unsigned total_bs;
	unsigned nobufs;

	bool cooked;
	char* typestring;
	static bool clear_data_on_arm;
	static int verbose;
	static bool skip_roi_search;
	static bool full_search;

	unsigned ib;
	bool accepting_events;
	EventController event0;

	/* COOKED=1 NSAMPLES=1999 NCHAN=128 >/dev/acq400/data/.control */
	/* setting NSAMPLES > 0 makes data available to clients, NSAMPLES==0 : no data */

	void notify_result(int nsamples) {
		char resbuf[128];
		sprintf(resbuf, "COOKED=%d NSAMPLES=%d NCHAN=%d TYPE=%s\n",
				cooked? 1:0, nsamples, G::nchan,
				G::wordsize==2? "SHORT": "LONG");
		if (verbose) fprintf(stderr, "%s \"%s\"\n", _PFN, resbuf);
		FILE* fp = fopen(NOTIFY_HOOK, "w");
		if (fp == 0) {
			perror(NOTIFY_HOOK);
			exit(1);
		}
		fprintf(fp, resbuf);
		fclose(fp);
	}
	void notify_result() {
		notify_result(G::pre+G::post);
	}
	void report_shot() {
		unsigned last_armed_shot, last_completed_shot, last_successful_shot;
		getEtcKnob(G::aggregator_sites[0]-'0', "shot", &last_armed_shot);
		getEtcKnob(0, "shot_complete", &last_completed_shot);
		getEtcKnob(0, "shot", &last_successful_shot);
		printf("SHOT=%u,%u,%u,%u\n", actual.state, last_armed_shot, last_completed_shot, last_successful_shot);
	}
	void update_last_successful_shot() {
		char cmd[80];
		snprintf(cmd, 80, "cp /etc/acq400/%c/shot /etc/acq400/0/shot", G::aggregator_sites[0]);
		printf("updating site 0 shot: \"%s\"\n", cmd);
		system(cmd);
	}
	virtual void postProcess(int ibuf, char* es) {
		BLT blt(MapBuffer::get_ba0());
		if (pre){
			if (verbose) fprintf(stderr, "%s pre\n", _PFN);
			BufferDistribution bd(ibuf, es);

			bd.report();
			if (verbose) bd.show();

			for (SegmentIterator it = bd.getSegments().begin();
				it != bd.getSegments().end(); ++it   ){
				blt((*it).base, (*it).len);
			}
		}else{
			if (verbose) fprintf(stderr, "%s pre==0\n", _PFN);
			/* probably redundant. pre==0, no reserve */
			if (es != 0 && es != MapBuffer::get_ba0()){
				blt(es, s2b(G::post));
			}
		}

	}
	virtual void onStreamStart() 		 {}
	virtual void onStreamBufferStart(int ib) {
		if (verbose>1) fprintf(stderr, "%s onStreamBufferStart %d\n", _PFN, ib);
	}
	virtual void onStreamEnd() {
		int verbose2 = getenv_default("ACQ400_STREAM_DEBUG_STREAM_END");
		if (verbose2 > verbose){
			verbose = verbose2;
		}

		setState(ST_POSTPROCESS); actual.print();
		Demuxer::_msync(MapBuffer::get_ba_lo(),
			MapBuffer::get_ba_hi()-MapBuffer::get_ba_lo(), MS_SYNC);

		if (pre){
			int ibuf;
			char* es;
			Timer roiTime("ROI");
			Timer fullTime("ALL");

			if (!skip_roi_search && findEvent(&ibuf, &es)){
				roiTime.report();
				if (verbose) fprintf(stderr, "%s %s call postProcess\n", _PFN, "ROI");
				postProcess(ibuf, es);
				roiTime.report(1);
				goto on_success;
			}else if (full_search && findEventSearchAll(&ibuf, &es)){
				fullTime.report();
				if (verbose) fprintf(stderr, "%s %s call postProcess\n", _PFN, "ALL");
				postProcess(ibuf, es);
				fullTime.report(1);
				goto on_success;
			}
			fprintf(stderr, "%s ERROR EVENT NOT FOUND, DATA NOT VALID RAW offloads entire DRAM\n", _PFN);
			notify_result((MapBuffer::get_ba_hi()-MapBuffer::get_ba_lo())/sample_size() - sizeof(unsigned));
			goto on_fail;
		}else{
			postProcess(0, MapBuffer::get_ba_lo());
		}
  on_success:
		update_last_successful_shot();
		notify_result();
  on_fail:
		report_shot();
	}

	virtual int bufferSkip(unsigned &ib) {
		return 0;
	}
	unsigned old_ib;

	virtual int getBufferId() {
		char buf[80];
		fd_set readfds;
		int rc;

		FD_ZERO(&readfds);
		FD_SET(fc, &readfds);
		FD_SET(f_ev, &readfds);

		if (verbose>2) fprintf(stderr, "%s fc %d f_ev %d nfds %d\n", _PFN, fc, f_ev, nfds);

		for(fd_set readfds0 = readfds;
			(rc = select(nfds, &readfds, 0, 0, 0)) > 0; readfds = readfds0){
			if (verbose>2) fprintf(stderr, "select returns  %d\n", rc);

			if (FD_ISSET(f_ev, &readfds)){
				if (verbose>2) fprintf(stderr, "%s FD_ISSET f_ev %d\n", _PFN, f_ev);
				rc = read(f_ev, event_info, 80);
				if (rc > 0){
					event_info[rc] = '\0';
					chomp(event_info);
					if (verbose) fprintf(stderr, "%s fd_ev read \"%s\"  accepting? %s\n",
								_PFN, event_info, accepting_events? "YES": "NO");
					if (accepting_events){
						event_received = true;
					}
				}else{
					syslog(LOG_DEBUG, "read error  %d\n", rc);
				}
			}
			if (FD_ISSET(fc, &readfds)){
				if (verbose>3) fprintf(stderr, "%s FD_ISSET fc %d\n", _PFN, fc);
				rc = read(fc, buf, 80);

				if (rc > 0){
					buf[rc] = '\0';
					ib = atoi(buf);
					assert(ib >= 0);
					assert(ib <= Buffer::nbuffers);

					if (old_ib+1 < Buffer::nbuffers && ib != old_ib+1){
						if (verbose) fprintf(stderr, "WARNING: buffer skip %d => %d\n", old_ib, ib);
					}
					old_ib = ib;
					if (verbose > 1) fprintf(stderr, "%d\n", ib);
					if (bufferSkip(ib)){
						continue;
					}
					return ib;
				}else{
					return -1;
				}
			}
		}

		if (rc < 0){
			syslog(LOG_DEBUG, "%s select error  %d\n", _PFN, rc);
		}
		return -1;
	}

	void store_event_time()
	{
		FILE *fp = fopen("/etc/acq400/1/event_time", "w");
		if (fp){
			fprintf(fp, "%lu\n", time(0));
		}
		fclose(fp);
	}
	void streamCore() {
		BufferLog blog;
		int ib;
		int post_run_over_one = 0;
		struct timespec finish_time;
		int buffers_over_pre = 0;

		while((ib = getBufferId()) >= 0){
			blog.update(ib);

			if (verbose > 1) fprintf(stderr,
					"%s 01 ib:%d state:%d pre %d/%d post %d/%d\n", _PFN,
					ib, actual.state, actual.pre, pre, actual.post, post);

			switch(actual.state){
			case ST_ARM:
				if (!clear_data_on_arm){
					notify_result(0);	// clear on first buffer, the data really has GONE
				}
				setState(pre? ST_RUN_PRE: ST_RUN_POST);
			default:
				;
			}
			for (IT it = peers.begin(); it != peers.end(); ++it){
				if (verbose > 2) fprintf(stderr, "%s call peer %p %p\n",
						_PFN, *it, this);
				(*it)->onStreamBufferStart(ib);
			}
			if (verbose > 1) fprintf(stderr, "%s done with peers\n", _PFN);

			switch(actual.state){
			case ST_RUN_PRE:
				if (verbose>1) fprintf(stderr, "%s ST_RUN_PRE %d %d\n", _PFN, actual.pre, pre);

				if (actual.pre < pre){
					if (actual.pre + samples_buffer < pre){
						actual.pre += samples_buffer;
					}else{
						actual.pre = pre;
					}
				}
				if (actual.pre >= pre){
					if (event_received){
						store_event_time();
						clock_gettime(CLOCK_REALTIME, &finish_time);
						finish_time.tv_sec += 1;
						setState(ST_RUN_POST);
					}else{
						if (++buffers_over_pre > G::corner_turn_delay){
							accepting_events = true;
							event0.enable();
						}
					}
				}
				break;
			case ST_RUN_POST:
				if (verbose>1) fprintf(stderr, "%s ST_RUN_POST %d\n", _PFN, ib);
				actual.post += samples_buffer;
				if (actual.post > post){
					actual.post = post;
					actual.elapsed += samples_buffer;
					/* always overrun by one buffer to ensure good data
					 * when ES is adjusted
					 */
					if (post_run_over_one++ > 1){
						setState(ST_POSTPROCESS);
						if (verbose) fprintf(stderr,
							"%s quits, extra %d\n", _PFN, post_run_over_one);
						return;
					}
				}
				break;
			default:
				if (verbose) fprintf(stderr, "%s dodgy default %d\n", _PFN, actual.state);
			}
			actual.elapsed += samples_buffer;
			actual.print(PRINT_WHEN_YOU_CAN);

			if (verbose>1) fprintf(stderr, "%s %d 99\n", _PFN, ib);
		}

		if (verbose) fprintf(stderr, "%s ERROR bad bufferId %d\n", _PFN, ib);
	}


#define RSV	"/dev/acq400.0.rsv"

	void estop() {
		setKnob(0, "estop", "1");
	}
	static void initVerbose() {
		if (verbose) fprintf(stderr, "StreamHeadPrePostVerbose set %d\n", verbose);
	}



public:
	StreamHeadPrePost(Progress& progress, int _pre, int _post) :
		StreamHeadWithClients(progress),
		pre(_pre), post(_post),
		cooked(false), ib(666666), accepting_events(false),
		event0(1, "event0")
		{

		initVerbose();
		event0.disable();
		actual.status_fp = stdout;
		if (verbose) fprintf(stderr, "%s\n", _PFN);
		setState(ST_STOP);
		unsigned total_bs = (pre+post)*sample_size() + Buffer::bufferlen;
		nfds = fc+1;

		while((pre+post)*sample_size() > TOTAL_BUFFER_LIMIT){
			if (post) post -= samples_buffer/2;
			if (pre)  pre  -= samples_buffer/2;
		}

		/* round total buffer up to multiple of sample size */
		/* @@todo .. nobody understands this WARNING
		if (total_bs > (pre+post)*sample_size()){
			fprintf(stderr,
			"WARNING reducing to fit memory pre:%d post:%d\n",
					pre, post);
		}
		*/


		nobufs = total_bs/Buffer::bufferlen;
		while (nobufs*Buffer::bufferlen < total_bs || nobufs < 2){
			++nobufs;
		}

		if (verbose){
			fprintf(stderr, "%s pre:%d post:%d\n", _PFN, pre, post);
			fprintf(stderr, "%s sample_size:%d\n", _PFN, sample_size());
			fprintf(stderr, "%s total buffer:%d obs:%d nob:%d\n", _PFN,
				total_bs, Buffer::bufferlen, nobufs);
		}
		peers.push_back(this);

		if (G::naxi && pre == 0){
			if (verbose){
				fprintf(stderr, "set_axi_oneshot(%d)\n", 1);
			}
			set_axi_oneshot(1);
		}

		startEventWatcher();
	}
	virtual ~StreamHeadPrePost() {
		// destructor is NOT called ?? @todo
	}



	virtual void stream() {
		if (clear_data_on_arm){
			notify_result(0);    // result data is cleared regardless of whether any data flows or not.
		}
		setState(ST_ARM);
		report_shot();
		for (IT it = peers.begin(); it != peers.end(); ++it){
			(*it)->onStreamStart();
		}

		if (G::soft_trigger){
			schedule_soft_trigger();
		}

		streamCore();
		estop();

		// NB first peer is myself:
		for (IT it = peers.begin(); it != peers.end(); ++it){
			(*it)->onStreamEnd();
		}

		setState(ST_CLEANUP);

		close();
	}
};

bool StreamHeadPrePost::clear_data_on_arm 	= ::getenv_default("ClearDataOnArm", 1);
bool StreamHeadPrePost::skip_roi_search 	= ::getenv_default("StreamHeadPrePostSkipRoiSearch", 0);
bool StreamHeadPrePost::full_search 		= ::getenv_default("StreamHeadPrePostFullSearch", 1);
int StreamHeadPrePost::verbose 			= ::getenv_default("StreamHeadPrePostVerbose");

class SubrateStreamHead: public StreamHead {
	static int createOutfile() {
		return ::createOutfile("/dev/shm/subrate");
	}
public:
	SubrateStreamHead():
		StreamHead(open("/dev/acq400.0.bq", O_RDONLY), 1) {
			ident("acq400_stream_bq");
			close(fout);		/* we open it every time .. */
			BufferCloner::cloneBuffers<OversamplingBufferCloner>();
	}
	virtual ~SubrateStreamHead() {
	}
	virtual void stream() {
		for (int ib; (ib = getBufferId()) >= 0; ){
			if (verbose){
				fprintf(stderr,
				  "%s [%d] pid:%d\n", _PFN, ib, getpid());
			}
			fout = createOutfile();
			Buffer::the_buffers[ib]->writeBuffer(fout, Buffer::BO_NONE);
			close(fout);
			if (verbose){
				fprintf(stderr, "%s:%d 99\n", _PFN, ib);
			}
		}
	}
};


class DemuxingStreamHeadPrePost: public StreamHeadPrePost  {
	char *cursor;
	void *event_cursor;	/* mark where event was detected */

	std::vector <MapBuffer*> outbuffers;
	Demuxer& demuxer;


	virtual void postProcess(int ibuf, char* es) {
		if (G::pre_demux_script){
			system(G::pre_demux_script);
		}

		if (verbose) fprintf(stderr, "%s 01 ibuf %d %p\n", _PFN, ibuf, es);
		if (pre){
			BufferDistribution bd(ibuf, es, bd_scale);
			bd.report();
			int iseg = 0;

			for (SegmentIterator it = bd.getSegments().begin();
				it != bd.getSegments().end(); ++it, ++iseg   ){
				if (verbose) fprintf(stderr, "%s 33 segment:%d\n", _PFN, iseg);
				demuxer((*it).base, (*it).len);
			}
			if (verbose) fprintf(stderr, "%s 89 %d/%d segments processed\n",
					_PFN, iseg, bd.getSegments().size());
		}else{
			demuxer(es, s2b(G::post));
		}
		cooked = true;
		if (verbose) fprintf(stderr, "%s 99\n", _PFN);
	}
protected:
	int bd_scale;
public:

	DemuxingStreamHeadPrePost(Progress& progress, Demuxer& _demuxer, int _pre, int _post) :
		StreamHeadPrePost(progress, _pre, _post),
		demuxer(_demuxer), bd_scale(1) {
		if (verbose) fprintf(stderr, "%s\n", _PFN);
	}
};

bool first = true;

class DemuxingStreamHeadPrePostDualBuffer: public DemuxingStreamHeadPrePost {
	bool bale_out;
protected:
	virtual char* findEvent(Buffer* the_buffer) {
		fprintf(stderr, "\n%s override starts here\n", _PFN);
		if ((the_buffer->ib()&1) == 1){
			fprintf(stderr, "DISCARD odd-number buffer %d\n", the_buffer->ib());
			return 0;
		}
		char* esp = DemuxingStreamHeadPrePost::findEvent(the_buffer);

		if (esp){
			if (verbose) fprintf(stderr, "%s calling findEvent on succ %d,%d\n", _PFN, the_buffer->ib(), the_buffer->succ());
			char* esps = DemuxingStreamHeadPrePost::findEvent(
				Buffer::the_buffers[the_buffer->succ()]);

			if (esps){
				fprintf(stderr, "%s %d succ rules esp:%p\n", _PFN, the_buffer->ib(), esp);
				if (bale_out){
					fprintf(stderr, "Bailing out to allow inspection %d %d\n", the_buffer->ib(), the_buffer->succ());
					exit(1);
				}
				return esp;
			}else{
				fprintf(stderr, "%s %d ERROR only half ES found\n", _PFN, the_buffer->ib());
				return 0;
			}
		}else{
			fprintf(stderr, "%s %d NOT FOUND\n", _PFN, the_buffer->ib());
			return 0;
		}
	}
public:
	virtual int bufferSkip(unsigned &ib) {
		if (first && verbose){
			fprintf(stderr, "%s FIRST buffer %d\n", _PFN, ib);
			first = false;
		}
		/* wait for ODD buffer, then report EVEN */
		if ((ib&1) != 0){
			ib -= 1;
			return 0;
		}else{
			return 1;
		}
	}
	DemuxingStreamHeadPrePostDualBuffer(Progress& progress, Demuxer& _demuxer, int _pre, int _post) :
		DemuxingStreamHeadPrePost(progress, _demuxer, _pre, _post),
		bale_out(getenv_default("DemuxingStreamHeadPrePostDualBufferBaleOut"))
	{
		bd_scale = 2;
		samples_buffer *= 2;
		fprintf(stderr, "%s %s samples_buffer %d\n", _PFN, bale_out? "bale_out": "", samples_buffer);
	}
};
void checkHolders() {
	for (int isite = 0; isite < G::nsites; ++isite){
		unsigned holder_ready = 0;
		for(int pollcat = 0;; ++pollcat, usleep(10000)){
			if (getKnob(G::the_sites[isite], "continuous_reader",
					&holder_ready) != 1){
				fprintf(stderr, "ERROR: getKnob continuous_reader\n");
				exit(1);
			}
			if (holder_ready){
				fprintf(stderr, "ERROR:checkHolders: BUSY ALREADY\n");
				exit(1);
			}

			if (pollcat > 100){
				fprintf(stderr, "ERROR:waitHolders: timeout\n");
				exit(1);
			}
		}
	}
}
void waitHolders() {
	for (int isite = 0; isite < G::nsites; ++isite){
		unsigned holder_ready = 0;
		for(int pollcat = 0;; ++pollcat, usleep(10000)){
			if (getKnob(G::the_sites[isite], "continuous_reader",
					&holder_ready) != 1){
				fprintf(stderr, "ERROR: getKnob continuous_reader\n");
				exit(1);
			}
			if (holder_ready){
				break;
			}

			if (pollcat > 100){
				fprintf(stderr, "ERROR:waitHolders: timeout\n");
				exit(1);
			}
		}
	}
}

StreamHead* StreamHead::createLiveDataInstance()
{
	ident("acq400_stream_LDI");

	for (nb_cat = 1;
	     nb_cat*Buffer::bufferlen/(G::nchan*G::wordsize) < G::nsam; ++nb_cat){
		;
	}
	BufferCloner::cloneBuffers<DemuxBufferCloner>();

	const char* _sf = ::getenv("StreamHead_LDI_SOURCE");
	stream_fmt = _sf? _sf: "%s.hb0";

	bool live_pp = G::stream_mode != SM_TRANSIENT && has_pre_post_live_demux();
	setKnob(0, "/etc/acq400/0/live_mode", live_pp? "2": "1");
	if (live_pp){
		return new StreamHeadLivePP;
	}else{
		return new StreamHeadHB0;
	}
}

#define ECL_NOLIMIT	0

void setEventCountLimit(int limit)
{
	fprintf(stderr, "setEventCountLimit() %d\n", limit);
	setKnob(0,
	"/sys/module/acq420fmc/parameters/acq400_event_count_limit", limit);
}



class MultiEventServer {
	const char* b0;
	const char* b1;
	FILE* fp_in;
	int verbose;
	int suppress_event;
	int pre;
	int post;
	int maxfiles;
	int pre_bytes;
	int post_bytes;
	int update_number;
	bool dynamic;				/* get dynamic updates */

	const int maxfiles_limit;
	int ev_count;

	unsigned get_hb_last() {
		unsigned hb_last;
		getKnob(0, "hb_last", &hb_last);
		if (verbose > 1) fprintf(stderr, "%s hb_last %u\n", __FUNCTION__, hb_last);
		return hb_last;
	}
	static unsigned delta_hb(unsigned hb0, unsigned hb1){
		if (hb1 >= hb0){
			return hb1 - hb0;
		}else{
			return Buffer::nbuffers - hb0 + hb1;
		}
	}
	unsigned post_buffers() {
		return post/(Buffer::bufferlen/Buffer::sample_size);
	}
	static long df(int fd)
	{
		struct statvfs buf;
		if (fstatvfs(fd, &buf) == 0){
			/* play it safe, don't go over 50% */
			if (buf.f_bavail < buf.f_blocks/2){
				return 0;
			}else{
				return buf.f_bsize * buf.f_bavail/2;
			}
		}else{
			return -1;
		}
	}
	void handle_event(int ev, int ibuf, char* esp){
		char fn[80];
		sprintf(fn, "event-%03d-%lu-%u-%u.dat", ev, time(0), pre, post);
		unsigned hb0 = get_hb_last();
		if (verbose) fprintf(stderr, "MultiEventServer::handle_event() %s\n", fn);

		char fn0[80];
		sprintf(fn0, "/tmp/.%s", fn);  /* work in a hidden file */

		FILE *fp = fopen(fn0, "w");
		if (fp == 0){
			perror("fopen");
			exit(EXIT_FAILURE);
		}
		long freebytes = df(fileno(fp));
		if (freebytes < pre_bytes+post_bytes){
			if (verbose) fprintf(stderr, "MultiEventServer::handle_event LOW memory %ld do not store\n", freebytes);
			fwrite(esp, 1, sample_size(), fp);
		}else{
			int linear_pre = esp-b0;
			int linear_post = b1-esp;

			if (pre_bytes > linear_pre){
				fwrite(b1-(pre_bytes-linear_pre), 1, pre_bytes-linear_pre, fp);
				fwrite(esp-linear_pre, 1, linear_pre, fp);
			}else{
				fwrite(esp-pre_bytes, 1, pre_bytes, fp);
			}

			unsigned dhb;
			for(unsigned pb = post_buffers(); (dhb = delta_hb(hb0, get_hb_last())) < pb+1; ){
				if (verbose) fprintf(stderr, "%s wait post %u < %u\n", __FUNCTION__, dhb, pb);
				usleep(10000);
			}
			int ss = sample_size() * suppress_event;
			if (post_bytes > linear_post){
				fwrite(esp+ss, 1, linear_post-ss, fp);
				fwrite(b0, 1, post_bytes-linear_post, fp);
			}else{
				fwrite(esp+ss, 1, post_bytes-ss, fp);
			}
		}
		fclose(fp);
		char fn1[80];
		sprintf(fn1, "/tmp/%s", fn);
		rename(fn0, fn1);				/* atomic, file safe to use */
		setKnob(0, "/etc/acq400/1/multi_event_latest", fn);
	}
	void set_maxfiles(int _maxfiles){
		maxfiles = _maxfiles > maxfiles_limit? maxfiles_limit: _maxfiles;
	}
#define MULTI_EVENT_EQUALS	"MULTI_EVENT="			// prefix in /dev/shm/settings file
	void update_prams(const char* _def){
		if (strncmp(_def, MULTI_EVENT_EQUALS, strlen(MULTI_EVENT_EQUALS)) == 0){
			_def += strlen(MULTI_EVENT_EQUALS);
		}
		int _update_number;
		int _maxfiles;
		int nf = sscanf(_def, "%d,%d,%d,%d", &pre, &post, &_maxfiles, &_update_number);

		if (verbose) fprintf(stderr, "MultiEventServer::serve() update was %d nf=%d %s\n",
				update_number, nf, _def);

		if (nf == 3 || (nf == 4 && _update_number > update_number)){
			if (_maxfiles == 0){
				set_maxfiles(0);		// disable
			}
			if (nf == 4){
				update_number = _update_number;
				if (_maxfiles){
					set_maxfiles(_maxfiles + ev_count);
				}
			}else{
				if (maxfiles != 0){
					set_maxfiles(_maxfiles + ev_count);	// could be signal to start;
				}
			}
			pre_bytes = pre*sample_size();
			post_bytes = (post+1)*sample_size();
		}
	}

	void update_prams(void) {
		FILE* fp = fopen("/dev/shm/multi_event_settings", "r");
		if (fp != 0){
			char def[80];
			if (fgets(def, 80, fp)){
				update_prams(def);
			}
			fclose(fp);
		}
	}
public:
	MultiEventServer(const char* def, int _fd_in):
		b0(MapBuffer::get_ba_lo()),
		b1(MapBuffer::get_ba_hi()),
		fp_in(fdopen(_fd_in, "r")),
		update_number(0),
		maxfiles_limit(getenv_default("MultiEventServerMaxfilesLimit", 199)),
		ev_count(0)
	{
		ident("acq400_stream_MES");
		verbose = getenv_default("MultiEventServerVerbose");
		suppress_event = getenv_default("MultiEventServerSuppressEvent");
		if (verbose) fprintf(stderr, "MultiEventServer\n");

		update_prams(def);
	}

	void serve () {
		char buf[128];
		// 17,34,0x46782160
		while (fgets(buf, 128, fp_in)){
			if (verbose) fprintf(stderr, "MultiEventServer::serve() %s", buf);
			update_prams();
			int ev;
			int ibuf;
			void* esp;

			if (verbose) fprintf(stderr, "MultiEventServer::serve() ev_count %d maxfiles %d\n",
					ev_count, maxfiles);

			if (ev_count++ < maxfiles && sscanf(buf, "%d,%d,%p", &ev, &ibuf, &esp) == 3){
				handle_event(ev, ibuf, (char*)esp);
			}
		}
	}
};
void StreamHead::createMultiEventInstance(const char* def)
{
	int pipefd[2] = {};
        pid_t cpid;

        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        cpid = fork();
        if (cpid == -1){
             perror("fork");
             exit(EXIT_FAILURE);
        }

        if (cpid == 0){    		/* Child reads from pipe */
             close(pipefd[1]);          /* Close unused write end */

             (new MultiEventServer(def, pipefd[0]))->serve();  /* blocks */
        }else{            		/* Parent writes argv[1] to pipe */
             close(pipefd[0]);          /* Close unused read end */
             multi_event = pipefd[1];
             nice(13);
             ident("acq400_stream_mei");
             fprintf(stderr, "StreamHead::createMultiEventInstance() multi_event pipe:%d\n", multi_event);
             // return to main line
        }
}

StreamHead* StreamHead::instance() {
	static StreamHead* _instance;


	//nice(-10);
	if (_instance == 0){
		StreamHeadImpl::set_axi_oneshot(0);
		setEventCountLimit(
				G::show_events? ECL_NOLIMIT:
				G::stream_mode == SM_TRANSIENT? 1: ECL_NOLIMIT);

		if (G::is_spy){
			goRealTime(10);
			return _instance = new StreamHead(
					open("/dev/acq400.0.bqf", O_RDONLY), 1);
		}

		if (G::multi_event){
			createMultiEventInstance(G::multi_event);
		}
		if (G::oversampling && fork() == 0){
			_instance = new SubrateStreamHead;
			return _instance;
		}

		if (G::live_instance && fork() == 0){
			_instance = createLiveDataInstance();
			return _instance;
		}

		goRealTime(10);
		ident("acq400_stream_main");
		if (G::stream_mode == SM_TRANSIENT){
			if (G::buffer_mode == BM_PREPOST && G::demux > 0){
				Demuxer *demuxer;

				if (G::naxi == 2){
					demuxer = Demuxer::instance(G::double_up? DB_2D_DOUBLE: DB_2D_REGULAR);
					_instance = new DemuxingStreamHeadPrePostDualBuffer(
						Progress::instance(), *demuxer, G::pre, G::post);
				}else{
					demuxer = Demuxer::instance(DB_REGULAR, G::wordsize);
					_instance = new DemuxingStreamHeadPrePost(
						Progress::instance(), *demuxer, G::pre, G::post);
				}
			}else{
				_instance = new StreamHeadPrePost(
					Progress::instance(), G::pre, G::post);
			}
		}else{
			if (G::null_copy){
				G::buffer_mode = BM_NULL;
			}
			switch(G::buffer_mode){
			case BM_NULL:
				if (G::fillk){
					_instance = new NullStreamHeadDivK(Progress::instance());
				}else{
					_instance = new NullStreamHead(Progress::instance());
				}
				break;
			default:
				if (G::sum || G::subset){
					StreamHeadWithClients * sh = new StreamHeadWithClients(Progress::instance());
					if (G::sum){
						sh->addClient(SumStreamHeadClient::instance(G::sum));
					}
					if (G::subset){
						sh->addClient(SubsetStreamHeadClient::instance(G::subset));
					}
					_instance = sh;
				}else{
					_instance = new StreamHeadImpl(Progress::instance());
				}
				break;
			}
		}
		aggregator_init();
		if (G::aggsem){
			if (sem_post(G::aggsem) != 0){
				perror("sem_post");
			}
		}
		waitHolders();
		if (verbose) fprintf(stderr, "%s %p\n", _PFN, _instance);
	}
	return _instance;
}

int main(int argc, const char** argv)
{
	init(argc, argv);
	StreamHead::instance()->startStream();
	return 0;
}
