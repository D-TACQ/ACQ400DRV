/*
 * slowmon.cpp : read slow mon data, aggregate and output
 *
 *  Created on: 18 Mar 2022
 *      Author: pgm
 */

#include "popt.h"

#include "local.h"
#include "Env.h"
#include "File.h"
#include "knobs.h"


#include "split2.h"

#include <assert.h>
#include <unistd.h>
#include <signal.h>


/* meta data front and back. no math! */
#define META1	sizeof(long)
#define META2	sizeof(long)

namespace G {
	unsigned usec = 0xffffffff;
	unsigned nacc = 1;
}

#define NSPAD	4
#define SPADLEN	(NSPAD*sizeof(unsigned))


class Sampler {
public:
	virtual void onSample(int sig) = 0;
	virtual ~Sampler() {}
	virtual void clear_sums() = 0;
};

Sampler* G_sampler;

void onSample(int sig){
	return G_sampler->onSample(sig);
}

template <class T>
class SamplerImpl: public Sampler {
	const int ndata;		// number of T elements in data
	T* data;			// ndata elements of T + SPAD
	long* sums;			// ndata elements of long;
	FILE* fp;
	unsigned isam;


	ssize_t len(){
		return (ndata+SPADLEN/sizeof(T))*sizeof(T);
	}
public:
	SamplerImpl(unsigned ssb): ndata(ssb/sizeof(T)), isam(0) {
		data = new T[ndata+SPADLEN/sizeof(T)];
		sums = new long[ndata];
		fp = fopen("/dev/acq400.0.subr", "r");
	}
	virtual ~SamplerImpl() {
		delete [] data;
		delete [] sums;
		fclose(fp);
	}
	virtual void clear_sums() {
		memset(sums, 0, ndata*sizeof(long));
		isam = 0;
	}
	void sumUp(bool copy_back, unsigned nacc) {
		for (int ii = 0; ii < ndata; ++ii){
			sums[ii] += data[ii];
			if (copy_back){
				data[ii] = sums[ii] / nacc;
			}
		}
	}

	void onSample(int sig)
	{
		ssize_t nread = read(fileno(fp), data, len());
		if (nread != len()){
			perror("read fail");
			exit(1);
		}

		if (G::nacc == 1){
			write(1,data, len());
		}else{
			bool copy_back = ++isam == G::nacc;
			sumUp(copy_back, G::nacc);

			if (copy_back){
				write(1, data, len());
				clear_sums();
			}
		}
	}
};

// specialized version of sumUp() no overflow up to 256x
template<> void SamplerImpl<int>::sumUp(bool copy_back, unsigned nacc) {
	for (int ii = 0; ii < ndata; ++ii){
		sums[ii] += data[ii] >> 8;
		if (copy_back){
			int inacc = static_cast<int>(nacc);  // because -signed/unsigned => garbage
			data[ii] = sums[ii]/inacc << 8 | (data[ii]&0x000000ff);
		}
	}
}


typedef std::vector<std::string> VS;

void init(int argc, const char* argv[])
{
	unsigned ssb = 0;
	bool data32 = false;
	char sites[16];
	VS sitelist;
	getKnob(0, "/etc/acq400/0/sites", sites);
	split2(sites, sitelist, ',');
	for (std::string st: sitelist){
		int site = atoi(st.c_str());
		unsigned nc;
		unsigned _data32;

		assert(getKnob(site, "active_chan", &nc) > 0);

		getKnob(site, "data32", &_data32);
		if (_data32){
			data32 = true;
		}
		ssb += nc * (_data32? 4: 2);
	}
	if (data32){
		G_sampler = new SamplerImpl<int>(ssb);
	}else{
		G_sampler = new SamplerImpl<short>(ssb);
	}
}

void ui()
{
	unsigned fs;
	unsigned fin;

	getKnob(0, "/etc/acq400/0/slowmon_fs", &fs);
	getKnob(0, "/etc/acq400/0/slowmon_fin", &fin);

	fs = std::max(fs, 1U);
	unsigned nacc = fin/fs;

	nacc = std::max(nacc, 1U);
	unsigned usec = 1000000/fs/nacc;

	if (G::nacc != nacc){
		G::nacc = nacc;
		G_sampler->clear_sums();
		setKnob(0, "/etc/acq400/0/slowmon_nacc", nacc);
	}

	if (usec != G::usec){
		G::usec = usec;
		ualarm(G::usec, G::usec); //alarm in a second, and every second after that.
		setKnob(0, "/etc/acq400/0/slowmon_us", usec);
	}
}

int main(int argc, const char* argv[])
{
	init(argc, argv);
	signal(SIGALRM, onSample);

	for(;;){
		ui();
		pause();
	}
	return 0;
}


