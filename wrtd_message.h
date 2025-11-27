/*
 * wrtd_message.h
 *
 *  Created on: 5 Nov 2021
 *      Author: pgm
 */

#ifndef WRTD_MESSAGE_H_
#define WRTD_MESSAGE_H_

#include <math.h>
#include <stdint.h>
#include "wrtd-common.h"
#include <unistd.h>
#include "wrtd_TS.h"
#include "Multicast.h"

namespace wrtd_message_ns {
	extern const char* group;
	extern int port;

	extern int verbose;
        extern int trg;					// trg 0 or 1, 2, decoded from message
        extern const char* tx_id;				// transmit id
        extern unsigned tx_mask;

        extern unsigned max_tx;				// send max this many trigs
        extern const char* tx_at;				// send message at +s[.nsec] or @secs-since-epoch[.nsec]

}


class TSCaster {
protected:
	MultiCast& mc;
	TSCaster(MultiCast& _mc);
	TS ts;
public:
        virtual ~TSCaster() = default;
	virtual void sendto(const TS& ts);
	virtual void sendraw(unsigned raw);
	virtual TS recvfrom();
	virtual int printLast();
	static TSCaster& factory(MultiCast& _mc);
};


class MessageFilter {
public:
        virtual ~MessageFilter() = default;
	virtual bool operator () (struct wrtd_message& msg) = 0;
	static MessageFilter& factory();
};

class Acq2106DefaultMessageFilter : public MessageFilter {
public:
	virtual bool operator() (struct wrtd_message& msg) override; 
};

class MultipleMatchFilter : public MessageFilter {
	std::vector<VS> matches;

	void append_match(const char* mx);
public:
	MultipleMatchFilter(const char* m0, const char* m1, const char* m2);
	virtual bool operator() (struct wrtd_message& msg) override; 
};



class WrtdCaster : public TSCaster {
	int* seq;
	char hn[13];			// acq2106_[n]nnn
	int fd;
	MessageFilter& is_for_us;
	struct wrtd_message msg;


	void sendcommon();
	void map_seq(void);
protected:
	WrtdCaster(MultiCast& _mc, MessageFilter& filter);
	friend class TSCaster;

public:
	virtual ~WrtdCaster();
	virtual void sendraw(unsigned raw);
	virtual void sendto(const TS& ts); 
	virtual int printLast(const char* pfx = "printLast():"); 
	virtual TS recvfrom() override; 
	static const int IMASK();
};



class Receiver {
	bool chatty;
public:
	Receiver();
	virtual ~Receiver();
	virtual void onAction(TS& ts, TS& ts_adj);
	virtual void action(TS& ts, int nrx); 
	virtual int event_loop(TSCaster& comms); 

	static Receiver* instance(bool chatty = false);
};


class Txa {
protected:
	virtual TS txa_validate_rel(unsigned sec, unsigned ns) = 0;
	virtual TS txa_validate_abs(unsigned sec, unsigned ns) = 0;

	TS txa_validate();
public:
	Txa();
	virtual ~Txa();

	int operator() (); 
	static Txa& factory();
};

#endif /* WRTD_MESSAGE_H_ */
