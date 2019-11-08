/*
 * wrtd.cpp : White Rabbit Time Distribution
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popt.h"

#include "Multicast.h"

#define M1 1000000
#define NSPS	(1000*M1)		// nanoseconds per second



namespace G {
	const char* group = "224.0.23.159";
	int port = 5044;
	const char* fname = "/dev/acq400.0.wr_ts";
	const char* current = "/dev/acq400.0.wr_cur";
	int delta_ticks;
        unsigned ticks_per_sec = 80000000;
        int verbose = 0;
        unsigned dns = 40*M1;				// delta nsec
        unsigned ns_per_tick = 50;			// ticks per nsec
}

#define REPORT_THRESHOLD (G::dns/4)

#define SECONDS_SHL	28
#define SECONDS_MASK	0x7
#define TICKS_MASK	0x0fffffff
#define TS_EN		(1<<31)

struct TS {
	unsigned raw;
	char repr[16];

	TS(unsigned _raw = 0): raw(_raw) {}
	TS(unsigned _secs, unsigned _ticks, bool en=true) {
		raw = (_secs&SECONDS_MASK) << SECONDS_SHL | (_ticks&TICKS_MASK) | (en?TS_EN:0);
	}

	unsigned secs() const { return (raw& ~TS_EN) >> SECONDS_SHL; }
	unsigned ticks() const { return raw&TICKS_MASK; }
	unsigned nsec() const { return ticks() * G::ns_per_tick; }

	TS add (unsigned dsecs, unsigned dticks = 0) {
		unsigned ss = secs();
		unsigned tt = ticks();
		tt += dticks;
		if (tt > G::ticks_per_sec){
			tt -= G::ticks_per_sec;
			ss += 1;
		}
		ss += dsecs;
		return TS(ss, tt);
	}
	TS operator+ (unsigned dticks) {
		return add(0, dticks);
	}
	long diff(TS& ts2){
		unsigned _ticks = ticks();
		unsigned _secs = secs();
		if (ts2.ticks() > ticks()){
			_ticks += G::ticks_per_sec;
			_ticks -= ts2.ticks();
			_secs -= 1;
		}else{
			_ticks -= ts2.ticks();
		}
		return (_secs - ts2.secs())*NSPS + _ticks*G::ns_per_tick;
	}

	const char* toStr(void) {
		sprintf(repr, "%d:%07d", secs(), ticks());
		return repr;
	}
};
struct poptOption opt_table[] = {
	{
	  "tickns", 0, POPT_ARG_INT, &G::ns_per_tick, 0, "tick size nsec"
	},
	{
	  "dns", 'd', POPT_ARG_INT, &G::dns, 0, "nsec to add to current time"
	},
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

const char* ui(int argc, const char** argv)
{
        poptContext opt_context =
                        poptGetContext(argv[0], argc, argv, opt_table, 0);
        int rc;
        while ((rc = poptGetNextOpt( opt_context )) >= 0 ){
                switch(rc){
                case 's':

                default:
                        ;
                }
        }
        G::ticks_per_sec = NSPS / G::ns_per_tick;
        G::delta_ticks = G::dns / G::ns_per_tick;

        if (G::verbose) fprintf(stderr, "ns per tick: %u ticks per s: %u delta_ticks %u\n",
        		G::ns_per_tick, G::ticks_per_sec, G::delta_ticks);

        return poptGetArg(opt_context);
}



class TSCaster {
protected:
	MultiCast& mc;
	TSCaster(MultiCast& _mc) : mc(_mc)
	{}
	TS ts;
public:
	virtual void sendto(TS& ts) {
		mc.sendto(&ts, sizeof(unsigned));
	}
	virtual TS recvfrom() {
		if (mc.recvfrom(&ts.raw, sizeof(ts.raw)) != sizeof(ts.raw)){
			perror("closedown");
			exit(1);
		}
		return ts;
	}
	virtual int printLast() {
		return printf("%08x\n", ts.raw);
	}
	static TSCaster& factory(MultiCast& _mc);
};

#include <stdint.h>
#include "wrtd-common.h"
#include <unistd.h>

class WrtdCaster : public TSCaster {
	int seq;
	char hn[13];			// acq2106_[n]nnn

	bool is_for_us(struct wrtd_message& msg) {
		if (strncmp((char*)msg.event_id, "acq2106", 7) == 0){
			return true;
		}else{
			fprintf(stderr, "HELP! non acq2106 message received\n");
			return false;
		}
	}
	struct wrtd_message msg;
protected:
	WrtdCaster(MultiCast& _mc) : TSCaster(_mc), seq(0)
	{
		gethostname(hn, sizeof(hn));
	}
	friend class TSCaster;

	virtual void sendto(TS& ts) {
	        msg.hw_detect[0] = 'L';
	        msg.hw_detect[1] = 'X';
	        msg.hw_detect[2] = 'I';
	        msg.domain = 0;

	        snprintf((char*)msg.event_id, WRTD_ID_LEN, "%s.%c", hn, '0');

	        msg.seq = seq++;
	        msg.ts_sec = ts.secs();			// truncated at 7.. worktodo use TAI
	        msg.ts_ns = ts.nsec();
	        msg.ts_frac = 0;
	        msg.ts_hi_sec = 0;
	        msg.flags = 0;
	        msg.zero[0] = 0;
	        msg.zero[1] = 0;
	        msg.pad[0] = 0;
	        mc.sendto(&msg, sizeof(msg));
	}
	virtual int printLast() {
		return printf("%s %16s %u %u %u\n", msg.hw_detect, msg.event_id, msg.seq, msg.ts_sec, msg.ts_ns);
	}
	virtual TS recvfrom() {
		while(true){
			if (mc.recvfrom(&msg, sizeof(msg)) != sizeof(msg)){
				perror("closedown");
				exit(1);
			}
			if (is_for_us(msg)){
				TS ts(msg.ts_sec, msg.ts_ns/G::ns_per_tick);
				return ts;
			}
		}
	}
};
TSCaster& TSCaster::factory(MultiCast& _mc) {
	int use_wrtd_fullmessage = 1;
	const char* value = getenv("WRTD_FULLMESSAGE");
	if (value){
		use_wrtd_fullmessage = atoi(value);
	}
	if (use_wrtd_fullmessage){
		return * new WrtdCaster(_mc);
	}else{
		return * new TSCaster(_mc);
	}
}



int sender(TSCaster& comms)
{
	FILE *fp = fopen(G::fname, "r");
	TS ts;
	while(fread(&ts.raw, sizeof(unsigned), 1, fp) == 1){
		if (G::verbose) fprintf(stderr, "sender:ts:%s\n", ts.toStr());
		ts = ts + G::delta_ticks;
		comms.sendto(ts);
	}
	return 0;
}

int receiver(TSCaster& comms)
{
	FILE *fp = fopen(G::fname, "w");
	FILE *fp_cur = fopen(G::current, "r");
	long dms = G::dns/M1;

	while(true){
		TS ts = comms.recvfrom();
		TS ts_cur;
		if (G::verbose > 1) fprintf(stderr, "receiver:ts:%s\n", ts.toStr());
		int rc = fwrite(&ts.raw, sizeof(unsigned), 1, fp);
		if (rc < 1){
			perror("fwrite");
		}
		fflush(fp);
		fread(&ts_cur.raw, sizeof(unsigned), 1, fp_cur);

		if (G::verbose) comms.printLast();

		long dt = ts.diff(ts_cur);
		if (dt < 0){
			fprintf(stderr, "wrtd rx ERROR missed ts by %ld msec\n", dt/M1);
		}else if (dt < (long)REPORT_THRESHOLD){
			fprintf(stderr, "wrtd rx WARNING threshold %ld msec under limit %ld\n", dt/M1, dms);
		}
	}
	return 0;
}
int main(int argc, const char* argv[])
{
	const char* mode = ui(argc, argv);

	if (strcmp(mode, "tx") == 0){
		return sender(TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER)));
	}else{
		return receiver(TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_RECEIVER)));
	}
}

