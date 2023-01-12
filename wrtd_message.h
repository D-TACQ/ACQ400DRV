/*
 * wrtd_message.h
 *
 *  Created on: 5 Nov 2021
 *      Author: pgm
 */

#ifndef WRTD_MESSAGE_H_
#define WRTD_MESSAGE_H_

#include <math.h>

namespace G {
	const char* group = "224.0.23.159";
	int port = 5044;

	int verbose = 0;
        int trg = 0;					// trg 0 or 1, 2, decoded from message
        const char* tx_id;				// transmit id
        unsigned tx_mask;

        unsigned max_tx = 1;				// send max this many trigs
        const char* tx_at;					// send message at +s[.nsec] or @secs-since-epoch[.nsec]

}


class TSCaster {
protected:
	MultiCast& mc;
	TSCaster(MultiCast& _mc) : mc(_mc)
	{}
	TS ts;
public:
	virtual void sendto(const TS& ts) {
		mc.sendto(&ts, sizeof(unsigned));
	}
	virtual void sendraw(unsigned raw) {
		mc.sendto(&raw, sizeof(unsigned));
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

class MessageFilter {
public:
	virtual bool operator () (struct wrtd_message& msg) = 0;

	static MessageFilter& factory();
};

class Acq2106DefaultMessageFilter : public MessageFilter {
public:
	virtual bool operator() (struct wrtd_message& msg) {
		if (strncmp((char*)msg.event_id, "acq2106", 7) == 0){
			return true;
		}else{
			fprintf(stderr, "HELP! non acq2106 message received\n");
			return false;
		}
	}
};






class MultipleMatchFilter : public MessageFilter {
	std::vector<VS> matches;

	void append_match(const char* mx)
	{
		VS* _mx = new VS;
		if (mx){
			split2<VS>(mx, *_mx, ',');

		}
		matches.push_back(*_mx);
	}
public:
	MultipleMatchFilter(const char* m0, const char* m1, const char* m2){
		append_match(m0);
		append_match(m1);
		append_match(m2);
	}
	virtual bool operator() (struct wrtd_message& msg) {
		for (unsigned ii = 0; ii < matches.size(); ++ii){
			for (std::string ss : matches[ii]){
				if (strncmp(ss.c_str(), (char*)msg.event_id, WRTD_ID_LEN) == 0){
					G::trg = ii;
					return true;
				}
			}
		}
		return false;
	}
};


MessageFilter& MessageFilter::factory() {
	const char* matches = getenv("WRTD_RX_MATCHES");
	if (matches){
		return * new MultipleMatchFilter(matches, getenv("WRTD_RX_MATCHES1"), getenv("WRTD_RX_DOUBLETAP"));
	}
	return * new Acq2106DefaultMessageFilter();
}

class WrtdCaster : public TSCaster {
	int* seq;
	char hn[13];			// acq2106_[n]nnn
	int fd;

	MessageFilter& is_for_us;
	struct wrtd_message msg;


	void sendcommon(){
		msg.hw_detect[0] = 'L';
		msg.hw_detect[1] = 'X';
		msg.hw_detect[2] = 'I';
		msg.seq = ++*seq;

	        mc.sendto(&msg, sizeof(msg));
	        if (G::verbose) printLast();
	}
	void map_seq(void){
		fd = shm_open("wrtd.seq", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
		if (fd == -1){
			perror("shm_open");
			exit(1);
		}
		if (ftruncate(fd, sizeof(uint32_t)) == -1){
			perror("ftruncate");
			exit(1);
		}
		seq = (int*)mmap(NULL, sizeof(uint32_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		if (seq == MAP_FAILED){
			perror("mmap");
			exit(1);
		}
	}
protected:
	WrtdCaster(MultiCast& _mc, MessageFilter& filter) : TSCaster(_mc), is_for_us(filter)
	{
		memset(&msg, 0, sizeof(msg));
		if (G::tx_id){
			strncpy((char*)msg.event_id, G::tx_id, WRTD_ID_LEN-1);
		}else{
			gethostname(hn, sizeof(hn));
			snprintf((char*)msg.event_id, WRTD_ID_LEN, "%s.%c", hn, '0');
		}
		map_seq();
	}
	virtual ~WrtdCaster() {
		close(fd);
	}
	friend class TSCaster;

	virtual void sendraw(unsigned raw) {
		msg.ts_sec = 0;
		msg.ts_ns = raw;
		//msg.event_id is pre-cooked, all other fields are zero
		msg.event_id[IMASK()] = G::tx_mask;	// use global default, NOT member ts ..

		sendcommon();
	}
	virtual void sendto(const TS& ts) {
	        msg.ts_sec = ts.secs();			// truncated at 7.. worktodo use TAI
	        msg.ts_ns = ts.nsec();
	        //msg.event_id is pre-cooked, all other fields are zero
	        msg.event_id[IMASK()] = ts.mask;

	        sendcommon();
	}
	virtual int printLast(const char* pfx = "printLast():") {
		return printf("%s %s %16s mask=%x seq=%u sec=%u ns=%u\n",
				pfx, msg.hw_detect, msg.event_id,
				     msg.event_id[IMASK()], msg.seq, msg.ts_sec, msg.ts_ns);
	}
	virtual TS recvfrom() {
		while(true){
			if (mc.recvfrom(&msg, sizeof(msg)) != sizeof(msg)){
				perror("closedown");
				exit(1);
			}
			if (is_for_us(msg)){
				if (G::verbose){
					printLast("FOR US:");
				}
				if (msg.ts_ns == TS_QUICK){
					TS ts(TS_QUICK);
					ts.mask = msg.event_id[IMASK()];
					if (G::verbose){
						fprintf(stderr, "%s TS_QUICK ts:%s mask:%x\n", PFN, ts.toStr(), ts.mask);
					}
					return ts;
				}else{
					TS ts(msg.ts_sec, msg.ts_ns/G::ns_per_tick);
					ts.mask = msg.event_id[IMASK()];
					if (G::verbose){
						fprintf(stderr, "%s TS TIME ts:%s mask:%x tai_s:%u\n",
								PFN, ts.toStr(), ts.mask, ts.tai_s);
					}
					return ts;
				}
			}else{
				if (G::verbose){
					printLast("NOT FOR US:");
				}
			}
		}
	}
public:
	static const int IMASK(){
		return WRTD_ID_LEN-1;
	}
};
TSCaster& TSCaster::factory(MultiCast& _mc) {
	if (Env::getenv("WRTD_FULLMESSAGE", 1)){
		return * new WrtdCaster(_mc, MessageFilter::factory());
	}else{
		return * new TSCaster(_mc);
	}
}


class Receiver {
	bool chatty;
public:
	Receiver(): chatty(false)
	{}
	virtual ~Receiver()
	{}
	virtual void onAction(TS& ts, TS& ts_adj)
	{}

	virtual void action(TS& ts, int nrx = 0) {
		fprintf(stderr, "%s ts:%s mask:%x\n", PFN, ts.toStr(), ts.mask);
	}
	virtual int event_loop(TSCaster& comms) {
		for (unsigned nrx = 0;; ++nrx){
			TS ts = comms.recvfrom();
			if (G::verbose > 1) fprintf(stderr, "%s() TS:%s %08x\n", PFN, ts.toStr(), ts.raw);
			action(ts, nrx);
			if (chatty) comms.printLast();
		}
		return 0;
	}

	static Receiver* instance(bool chatty = false);
};


class Txa {

protected:
	virtual TS txa_validate_rel(unsigned sec, unsigned ns) = 0;

	virtual TS txa_validate_abs(unsigned sec, unsigned ns) = 0;

	TS txa_validate() {
		if (G::max_tx != 1){
			fprintf(stderr, "ERROR: max_tx must be 1\n");
			exit(1);
		}else if (G::tx_at == 0){
			fprintf(stderr, "ERROR: tx_at not set. please set either +s[.ns] or @abs[.ns]\n");
			exit(1);
		}else{
			char mode;
			unsigned sec;
			unsigned nsec = 0;

			switch (sscanf(G::tx_at, "%c%u:%u", &mode, &sec, &nsec)){
			case 3:
				break;
			default:
				float fsec;
				switch (sscanf(G::tx_at, "%c%u.%F", &mode, &sec, &fsec)){
				case 3:
					if (sscanf(G::tx_at+1, "%F", &fsec) == 1){
						double int_part;
						double fract_part = modf(fsec, &int_part);
						nsec = static_cast<unsigned>(NSPS * fract_part);
						assert(sec == static_cast<unsigned>(int_part));
					}else{
						fprintf(stderr, "ERROR: failed to scan \"%s\" %d\n", G::tx_at, __LINE__);
						exit(1);
					}
					break;
				case 2:
					nsec = 0;
					break;
				default:
					fprintf(stderr, "ERROR: failed to scan \"%s\"\n", G::tx_at);
					exit(1);
				}
			}
			switch(mode){
			case '+':
				return txa_validate_rel(sec, nsec);		// time relative.
			case 'T':
				return txa_validate_abs(sec, nsec);		// time TAI
			case 'U':
				return txa_validate_abs(sec+37, nsec);		// time UTC
			default:
				fprintf(stderr, "ERROR: bad mode \"%s\" : \'%c\' wanted \'[+@]\'\n", G::tx_at, mode);
				exit(1);
			}
		}
		// not reached, but keeping compiler happy..
		return TS();
	}
public:
	Txa()
	{}
	virtual ~Txa()
	{}

	int operator() () {
		if (G::verbose){
			fprintf(stderr, "%s trigger at [--at=@abs or --at=+rel]\n", PFN);
		}
		TSCaster& comms = TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER));

		comms.sendto(txa_validate());
		return 0;
	}
	static Txa& factory();
};





#endif /* WRTD_MESSAGE_H_ */
