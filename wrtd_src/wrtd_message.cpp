/*
 * wrtd_message.cpp
 *
 *  Created on: 27 Nov 2025
 *      Author: cph
 */
#include "wrtd_message.h"

#include <unistd.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>     // For O_CREAT, O_RDWR
#include <sys/stat.h>  // For S_IRUSR, SIWUSR
#include "split2.h"
#include "Env.h"

namespace wrtd_message_ns {
	const char* group = "224.0.23.159";
	int port = 5044;

	int verbose = 0;
        int trg = 0;					// trg 0 or 1, 2, decoded from message
        const char* tx_id = nullptr;				// transmit id
        unsigned tx_mask = 0;

        unsigned max_tx = 1;				// send max this many trigs
        const char* tx_at = nullptr;					// send message at +s[.nsec] or @secs-since-epoch[.nsec]
        unsigned char channel_selection = '1';

}

TSCaster::TSCaster(MultiCast& _mc) : mc(_mc) {
}

void TSCaster::sendto(const TS& ts) {
        mc.sendto(&ts, sizeof(unsigned));
}

void TSCaster::sendraw(unsigned raw) {
        mc.sendto(&raw, sizeof(unsigned));
}

TS TSCaster::recvfrom() {
        if (mc.recvfrom(&ts.raw, sizeof(ts.raw)) != sizeof(ts.raw)){
                perror("closedown");
                exit(1);
        }
        return ts;
}

int TSCaster::printLast() {
        return printf("%08x\n", ts.raw);
}

TSCaster& TSCaster::factory(MultiCast& _mc) {
	if (Env::getenv("WRTD_FULLMESSAGE", 1)){
		return * new WrtdCaster(_mc, MessageFilter::factory());
	}else{
		return * new TSCaster(_mc);
	}
}

bool Acq2106DefaultMessageFilter::operator() (struct wrtd_message& msg) {
        if (strncmp((char*)msg.event_id, "acq2106", 7) == 0){
                return true;
        }else{
                fprintf(stderr, "HELP! non acq2106 message received\n");
                return false;
        }
}

void MultipleMatchFilter::append_match(const char* mx)
{
        VS* _mx = new VS;
        if (mx){
                split2<VS>(mx, *_mx, ',');

        }
        matches.push_back(*_mx);
}

MultipleMatchFilter::MultipleMatchFilter(const char* m0, const char* m1, const char* m2){
        append_match(m0);
        append_match(m1);
        append_match(m2);
}

bool MultipleMatchFilter::operator() (struct wrtd_message& msg) {
        for (unsigned ii = 0; ii < matches.size(); ++ii){
                for (std::string ss : matches[ii]){
                        if (strncmp(ss.c_str(), (char*)msg.event_id, WRTD_ID_LEN) == 0){
                                wrtd_message_ns::trg = ii;
                                return true;
                        }
                }
        }
        return false;
}

MessageFilter& MessageFilter::factory() {
	const char* matches = getenv("WRTD_RX_MATCHES");
	if (matches){
		return * new MultipleMatchFilter(matches, getenv("WRTD_RX_MATCHES1"), getenv("WRTD_RX_DOUBLETAP"));
	}
	return * new Acq2106DefaultMessageFilter();
}


void WrtdCaster::sendcommon(){
        msg.hw_detect[0] = 'L'; // LXI
        msg.hw_detect[1] = ts.channel_selection; //'S';  // stubbed this out for channel number prototype
        msg.hw_detect[2] = 'I';
        msg.seq = ++*seq;

        mc.sendto(&msg, sizeof(msg));
        if (wrtd_message_ns::verbose) printLast();
}

void WrtdCaster::map_seq(void){
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

WrtdCaster::WrtdCaster(MultiCast& _mc, MessageFilter& filter) : TSCaster(_mc), is_for_us(filter)
{
        memset(&msg, 0, sizeof(msg));
        if (wrtd_message_ns::tx_id){
                strncpy((char*)msg.event_id, wrtd_message_ns::tx_id, WRTD_ID_LEN-1);
        }else{
                gethostname(hn, sizeof(hn));
                snprintf((char*)msg.event_id, WRTD_ID_LEN, "%s.%c", hn, '0');
        }
        map_seq();
}

WrtdCaster::~WrtdCaster() {
        close(fd);
}


void WrtdCaster::sendraw(unsigned raw) {
        msg.ts_sec = 0;
        msg.ts_ns = raw;
        //msg.event_id is pre-cooked, all other fields are zero
        msg.event_id[IMASK()] = wrtd_message_ns::tx_mask;	// use global default, NOT member ts ..

        // sendcommon();

        msg.hw_detect[0] = 'Q'; // LXI
        msg.hw_detect[1] = ts.channel_selection; //'S';  // stubbed this out for donatella's channel number
        msg.hw_detect[2] = 'A';
        msg.seq = ++*seq;

        mc.sendto(&msg, sizeof(msg));
        if (wrtd_message_ns::verbose) printLast("hello");
}

/*
 * @brief final call for flexible elements of packets before they are sent on the wire.
 *
 */
void WrtdCaster::sendto(const TS& tstamp) {
    this->ts = tstamp;
        msg.ts_sec = ts.secs();			// truncated at 7.. worktodo use TAI
        msg.ts_ns = ts.nsec();
        //msg.event_id is pre-cooked, all other fields are zero
        msg.event_id[IMASK()] = ts.mask;

        // sendcommon(tstamp);

        msg.hw_detect[0] = 'L'; // LXI
        msg.hw_detect[1] = tstamp.channel_selection; //'S';  // stubbed this out for donatella's channel number
        msg.hw_detect[2] = 'I';
        msg.seq = ++*seq;

        mc.sendto(&msg, sizeof(msg));
        if (wrtd_message_ns::verbose) printLast();
}

int WrtdCaster::printLast(const char* pfx) {
        return printf("%s %s %16s mask=%x seq=%u sec=%u ns=%u\n",
                        pfx, msg.hw_detect, msg.event_id,
                             msg.event_id[IMASK()], msg.seq, msg.ts_sec, msg.ts_ns);
}

TS WrtdCaster::recvfrom() {
        while(true){
                if (mc.recvfrom(&msg, sizeof(msg)) != sizeof(msg)){
                        perror("closedown");
                        exit(1);
                }
                if (is_for_us(msg)){
                        if (wrtd_message_ns::verbose){
                                printLast("FOR US:");
                        }
                        if (msg.ts_ns == TS_QUICK){
                                TS ts(TS_QUICK);
                                ts.mask = msg.event_id[IMASK()];
                                if (wrtd_message_ns::verbose){
                                        fprintf(stderr, "%s TS_QUICK ts:%s mask:%x\n", __PRETTY_FUNCTION__, ts.toStr(), ts.mask);
                                }
                                return ts;
                        }else{
                                TS ts(msg.ts_sec, msg.ts_ns/wrtd_TS_ns::ns_per_tick);
                                ts.mask = msg.event_id[IMASK()];
                                if (wrtd_message_ns::verbose){
                                        fprintf(stderr, "%s TS TIME ts:%s mask:%x tai_s:%u\n",
                                                        __PRETTY_FUNCTION__, ts.toStr(), ts.mask, ts.tai_s);
                                }
                                return ts;
                        }
                }else{
                        if (wrtd_message_ns::verbose){
                                printLast("NOT FOR US:");
                        }
                }
        }
}

const int WrtdCaster::IMASK(){
        return WRTD_ID_LEN-1;
}



Receiver::Receiver(): chatty(false) {}

void Receiver::onAction(TS& ts, TS& ts_adj) {}

void Receiver::action(TS& ts, int nrx) {
        fprintf(stderr, "%s ts:%s mask:%x\n", __PRETTY_FUNCTION__, ts.toStr(), ts.mask);
}

int Receiver::event_loop(TSCaster& comms) {
        for (unsigned nrx = 0;; ++nrx){
                TS ts = comms.recvfrom();
                if (wrtd_message_ns::verbose > 1) fprintf(stderr, "%s() TS:%s %08x\n", __PRETTY_FUNCTION__, ts.toStr(), ts.raw);
                action(ts, nrx);
                if (chatty) comms.printLast();
        }
        return 0;
}

TS Txa::txa_validate() {
        if (wrtd_message_ns::max_tx != 1){
                fprintf(stderr, "ERROR: max_tx must be 1\n");
                exit(1);
        }else if (wrtd_message_ns::tx_at == 0){
                fprintf(stderr, "ERROR: tx_at not set. please set either +s[.ns] or @abs[.ns]\n");
                exit(1);
        }else{
                char mode;
                unsigned sec;
                unsigned nsec = 0;

                switch (sscanf(wrtd_message_ns::tx_at, "%c%u:%u", &mode, &sec, &nsec)){
                case 3:
                        break;
                default:
                        float fsec;
                        switch (sscanf(wrtd_message_ns::tx_at, "%c%u.%F", &mode, &sec, &fsec)){
                        case 3:
                                if (sscanf(wrtd_message_ns::tx_at+1, "%F", &fsec) == 1){
                                        double int_part;
                                        double fract_part = modf(fsec, &int_part);
                                        nsec = static_cast<unsigned>(NSPS * fract_part);
                                        assert(sec == static_cast<unsigned>(int_part));
                                }else{
                                        fprintf(stderr, "ERROR: failed to scan \"%s\" %d\n", wrtd_message_ns::tx_at, __LINE__);
                                        exit(1);
                                }
                                break;
                        case 2:
                                nsec = 0;
                                break;
                        default:
                                fprintf(stderr, "ERROR: failed to scan \"%s\"\n", wrtd_message_ns::tx_at);
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
                        fprintf(stderr, "ERROR: bad mode \"%s\" : \'%c\' wanted \'[+@]\'\n", wrtd_message_ns::tx_at, mode);
                        exit(1);
                }
        }
        // not reached, but keeping compiler happy..
        return TS();
}

int Txa::operator() () {
        if (wrtd_message_ns::verbose){
                fprintf(stderr, "%s trigger at [--at=@abs or --at=+rel]\n", __PRETTY_FUNCTION__);
        }
        TSCaster& comms = TSCaster::factory(MultiCast::factory(wrtd_message_ns::group, wrtd_message_ns::port, MultiCast::MC_SENDER));

        comms.sendto(txa_validate());
        return 0;
}

