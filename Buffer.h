/* ------------------------------------------------------------------------- */
/* Buffer.h  D-TACQ ACQ400 FMC  DRIVER                                   
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

#ifndef BUFFER_H_
#define BUFFER_H_


class Buffer {

protected:
	int fd;
	const char* fname;
	const unsigned ibuf;
	unsigned buffer_len;

protected:
	char *pdata;
public:
	static unsigned last_buf;
	static int verbose;
	static int checkiten;
	static unsigned bufferlen;
	static unsigned nbuffers;
	static unsigned sample_size;

	enum BUFFER_OPTS { BO_NONE, BO_START=0x1, BO_FINISH=0x2 };

	int getLen() { return buffer_len; }

	virtual unsigned getItem(int ii) { return 0; }
	virtual unsigned getSizeofItem() { return 1; }

	virtual int writeBuffer(int out_fd, int b_opts) = 0;
	virtual int writeBuffer(int out_fd, int b_opts, int start_off, int len)
	{
		return -1;
	}

	virtual int copyBuffer(void* dest) { return -1; }

	Buffer(const char* _fname, int _buffer_len);
	Buffer(Buffer* cpy);
	virtual ~Buffer() {
		close(fd);
	}

	static int create(const char* root, int _buffer_len);
	static vector<Buffer*> the_buffers;

	const char* getName() {
		return fname;
	}

	virtual int pred() {
		return ibuf == 0? last_buf-1: ibuf-1;
	}
	virtual int succ() {
		return ibuf >= last_buf-1? 0: ibuf+1;
	}
	int ib() {
		return ibuf;
	}
	virtual char* getBase() { return pdata; }

	char *getEnd() {
		return getBase() + getLen();
	}

	static int samples_per_buffer() {
		return bufferlen/sample_size;
	}
};



class NullBuffer: public Buffer {
public:
	NullBuffer(const char* _fname, int _buffer_len):
		Buffer(_fname, _buffer_len)
	{
		static int report;
		if (!report &&verbose){
			fprintf(stderr, "NullBuffer()\n");
			++report;
		}
	}
	virtual int writeBuffer(int out_fd, int b_opts) {
		return buffer_len;
	}
};
class MapBuffer: public Buffer {
protected:
	static char* ba0;		/* low end of mapping */
	static char* ba1;		/* hi end of mapping */
	static char* ba_lo;		/* lo end of data set */
	static char* ba_hi;		/* hi end of data set likely == ba1 */
	static int buffer_0_reserved;

public:
	virtual int writeBuffer(int out_fd, int b_opts, unsigned start_off, unsigned len);

	virtual int writeBuffer(int out_fd, int b_opts);
	virtual int copyBuffer(void* dest);

	static char* get_ba0()	 { return ba0;   }
	static char* get_ba_lo() { return ba_lo; }
	static char* get_ba_hi() { return ba_hi; }


	int includes(void *cursor);

	MapBuffer(const char* _fname, int _buffer_len);
	virtual ~MapBuffer();
	static unsigned getBuffer(char* pb) {
		if (pb < ba0){
			return -1;
		}else if (pb > ba1){
			return -2;
		}else{
			return (pb-ba0)/bufferlen;
		}
	}
	static const char* listBuffers(char* p0, char* p1, bool show_ba = false);
	static void reserve() {
		ba_lo = ba0 + 2*bufferlen;
		buffer_0_reserved = 2;
	}
	static int hasReserved() {
		return buffer_0_reserved;
	}
	virtual int pred() {
		unsigned first_buf = buffer_0_reserved;
		return ibuf == first_buf? last_buf-1: ibuf-1;
	}
	virtual int succ() {
		int first_buf = buffer_0_reserved;
		return ibuf >= last_buf-1? first_buf: ibuf+1;
	}

	static char* ba(int ibuf){
		return reinterpret_cast<MapBuffer*>(Buffer::the_buffers[ibuf])->pdata;
	}
};




#endif /* BUFFER_H_ */
