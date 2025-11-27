/*
 * wrs_testharness.cpp
 *
 *  Created on: 7 Jan 2025
 *      Author: pgm
 *
 *  args: block rx tx
 *  params:
 */

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


#include "Env.h"
#include "acq-util.h"

//#include "wrs_trigger.h"

#define WRS_DEV	"/dev/acq400.0.wr_pkt_rx"    // bad name, receives and transmits

#define PKT_LW	10
#define WRS_PKT_LW		10
#define WRS_PKT_FULL_READ	((1+WRS_PKT_LW)*sizeof(u32)) /* full count in bytes */


typedef unsigned u32;

namespace wrs_testharness_ns {
	int rx_target_count;
	int rx_count;
	int tx_count;
	int usleep;
	int rx_block;
	FILE *fp;
	int fd;
	int rt_prio = 0;

	u32 tx_pkt[PKT_LW];
	u32 read_data[PKT_LW+1];
	u32* rx_pkt;

};

void get_status(int sig){
	fprintf(stderr, "rx_count to %d left out of %d\n", wrs_testharness_ns::rx_count, wrs_testharness_ns::rx_target_count);
}

const char* ui(int argc, const char** argv)
{
	wrs_testharness_ns::rx_target_count = wrs_testharness_ns::rx_count = Env::getenv("RX", 1);
	wrs_testharness_ns::tx_count = Env::getenv("TX", 0);
	wrs_testharness_ns::usleep   = Env::getenv("US", 0);
	wrs_testharness_ns::rx_block = Env::getenv("RX_BLOCK", 1);
	wrs_testharness_ns::rt_prio  = Env::getenv("RTPRIO", 0);

	const char* mode = wrs_testharness_ns::rx_count&&wrs_testharness_ns::tx_count? "r+": wrs_testharness_ns::tx_count? "w": "r";

	assert(wrs_testharness_ns::rx_count||wrs_testharness_ns::tx_count);

	signal(SIGINT, get_status);

	wrs_testharness_ns::fp = fopen(WRS_DEV, mode);
	assert(wrs_testharness_ns::fp);
	wrs_testharness_ns::fd = fileno(wrs_testharness_ns::fp);

	if (wrs_testharness_ns::rx_block == 0){
		int flags = fcntl(wrs_testharness_ns::fd, F_GETFL, 0);
		int rc = fcntl(wrs_testharness_ns::fd, F_SETFL, flags|O_NONBLOCK);
		assert(rc != -1);
	}
	wrs_testharness_ns::rx_pkt = wrs_testharness_ns::read_data+1;    // first word is TS.

	for (int ii = 0; ii < PKT_LW; ++ii){
		wrs_testharness_ns::tx_pkt[ii] = 0xaabb0000|ii;
	}

	if (wrs_testharness_ns::rt_prio){
		goRealTime(wrs_testharness_ns::rt_prio);
	}
	return 0;
}

void dump_pkt(u32* pkt, const char* id){
	printf("%4s: ", id);
	for (int ii = 0; ii < PKT_LW; ++ii){
		printf("%08x,", pkt[ii]);
	}
}
void tx() {
	int rc = write(wrs_testharness_ns::fd, wrs_testharness_ns::tx_pkt, sizeof(u32)*PKT_LW);
	assert(rc == sizeof(u32)*PKT_LW);
	dump_pkt(wrs_testharness_ns::tx_pkt, "TX"); printf("\n");
	wrs_testharness_ns::tx_pkt[PKT_LW-1] += 1;
	wrs_testharness_ns::tx_pkt[0] = (wrs_testharness_ns::tx_pkt[0]&~0x00ff00) | ((wrs_testharness_ns::tx_pkt[0]&0x0ff00)+(1<<8));
}

void rx() {
	int rc = read(wrs_testharness_ns::fd, wrs_testharness_ns::read_data, WRS_PKT_FULL_READ);
	assert(rc == WRS_PKT_FULL_READ);
	dump_pkt(wrs_testharness_ns::rx_pkt, "RX"); printf("TS:%08x", wrs_testharness_ns::read_data[0]); printf("\n");
}


int main(int argc, const char* argv[])
{
	ui(argc, argv);
//	WRS_Trigger* trigger = WRS_Trigger::factory(wrs_testharness_ns::site)l

	while (wrs_testharness_ns::tx_count || wrs_testharness_ns::rx_count){
		if (wrs_testharness_ns::tx_count){
			tx();
			--wrs_testharness_ns::tx_count;
			if (wrs_testharness_ns::usleep){
				usleep(wrs_testharness_ns::usleep);
			}
		}
		if (wrs_testharness_ns::rx_count){
			rx();
			--wrs_testharness_ns::rx_count;
		}
	}
	return 0;
}


