/* ------------------------------------------------------------------------- */
/* Buffer.cpp  D-TACQ ACQ400 FMC  DRIVER                                   
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

#define VERID	"B1007"

#define NCHAN	4

#include <semaphore.h>
#include <syslog.h>

#include <vector>

#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"


using namespace std;

#include "Buffer.h"


Buffer::Buffer(const char* _fname, int _buffer_len):
	fname(_fname),
	ibuf(last_buf++),
	buffer_len(_buffer_len)
{
	fd = open(fname, O_RDWR, 0777);

	if (fd < 0){
		perror(fname);
		exit(1);
	}


}
Buffer::Buffer(Buffer* cpy) :
	fd(cpy->fd),
	fname(cpy->fname),
	ibuf(cpy->ibuf),
	buffer_len(cpy->buffer_len),
	pdata(cpy->pdata)
{}

int MapBuffer::includes(void *cursor)
/** returns remain length if buffer includes cursor */
{
	char* cc = static_cast<char *>(cursor);
	char* bp = static_cast<char *>(pdata);
	char *be = bp + buffer_len;
	int remain = 0;
	if (cc >= bp && cc <= be){
		remain =  buffer_len - (cc-bp);
	}

	if (verbose){
		fprintf(stderr, "%s %s ret %d\n", __func__, fname, remain);
	}
	return remain;
}

MapBuffer::MapBuffer(const char* _fname, int _buffer_len) :
				Buffer(_fname, _buffer_len)
{

	pdata = static_cast<char*>(mmap(static_cast<void*>(ba1), bufferlen,
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0));

	if (pdata != ba1){
		fprintf(stderr, "mmap() failed to get the hint:%p actual %p\n", ba1, pdata);
		exit(1);
	}
	if (verbose > 2) fprintf(stderr, "MapBuffer[%d] %s, %p\n",
			the_buffers.size()-1, fname, pdata);

	ba_hi = ba1 += bufferlen;
	assert(pdata != MAP_FAILED);
}

MapBuffer::~MapBuffer() {
	if (verbose) fprintf(stderr, "~MapBuffer() %d munmap %p\n", ibuf, pdata);
	munmap(pdata, bufferlen);
}

const char* MapBuffer::listBuffers(char* p0, char* p1, bool show_ba){
	char report[512];
	report[0] = '\0';
	int ibuf1 = -1;

	for(char* pn = p0; pn <= p1; pn += bufferlen){
		int ibuf = getBuffer(pn);
		if (ibuf != ibuf1){
			if (!show_ba){
				sprintf(report+strlen(report), strlen(report)? ",%d":"%d", ibuf);
			}else{
				char* ba = reinterpret_cast<MapBuffer*>(the_buffers[ibuf])->pdata;
				sprintf(report+strlen(report), strlen(report)? ",%p":"%p", ba);
			}
			ibuf1 = ibuf;
		}
	}
	char* ret = new char[strlen(report)+1]; // yes, this is a leak
	strcpy(ret, report);
	return ret;
}

int MapBuffer::writeBuffer(int out_fd, int b_opts, unsigned start_off, unsigned len)
/**< all offsets in bytes */
{
	if (start_off > buffer_len) {
		return 0;
	}else{
		if (buffer_len - start_off < len){
			len = buffer_len - start_off;
		}
	}
	return write(out_fd, pdata+start_off, len);
}

int MapBuffer::writeBuffer(int out_fd, int b_opts) {
	return write(out_fd, pdata, buffer_len);
}
int MapBuffer::copyBuffer(void* dest) {
	memcpy(dest, pdata, buffer_len);
	return buffer_len;
}


int Buffer::verbose;
int Buffer::checkiten = 1;
unsigned Buffer::bufferlen;
unsigned Buffer::nbuffers;
unsigned Buffer::sample_size = 1;

unsigned Buffer::last_buf;
vector<Buffer*> Buffer::the_buffers;

#define MAPBUFFER_BA0     (char*)(0x40000000)
char* MapBuffer::ba0   = MAPBUFFER_BA0;		/* const */
char* MapBuffer::ba1   = MAPBUFFER_BA0;		/* initial value increments to top */
char* MapBuffer::ba_lo = MAPBUFFER_BA0;		/* initial value, may move by reserve count */
char* MapBuffer::ba_hi;				/* tracks ba1 @@todo may be redundant */
int MapBuffer::buffer_0_reserved;

class BufferInitializer {
public:
	BufferInitializer() {
		const char* vs = getenv("BufferVerbose");
		vs && (Buffer::verbose = atoi(vs));
		const char* checkit = getenv("BufferCheckit");
		checkit && (Buffer::checkiten = atoi(checkit));
	}
};

static BufferInitializer bi;
