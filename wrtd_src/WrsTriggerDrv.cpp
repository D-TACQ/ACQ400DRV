/*
 * WrsTriggerDrv.cpp: driver for WR streamers
 *
 *  Created on: 8 Jan 2025
 *      Author: cph 
 *
 *  Open and hold a mapping on SYSDEV (one page)
 *  Create structure pointers to TX, RX at WRS_PKT_BASE_TX,WRS_PKT_BASE_RX
 *  Block on WRS_DEV, read one u32.
 *  Time critical:Immediately, set soft_trigger to zero,
 *  Then print the TS from WRS_DEV and then
 *  Dump RX on RX dev receipt
 *  Toggle an output. Compare SOFT TRIGGER to AUX to find max delay in app layer.
 */
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string>

#include <cstring>
#include <fstream>
#include <iostream>

#include "Env.h"
#include "acq-util.h"
#include "wrs_trigger_int.h"
#include "WrsTriggerDrv.h"
#include "knobs.h"

#define PAGESIZE 400

WrsTriggerDrv::WrsTriggerDrv(const std::string& mode, int rtprio, int usleep, bool rxblock)
{


    //rx_target_count = rx_count = Env::getenv("RX", 0);
    // tx_count = Env::getenv("TX", 0);
    // usleep_time   = Env::getenv("US", 0);
    // rx_block = Env::getenv("RX_BLOCK", 1);
    //rt_prio = Env::getenv("RTPRIO", 0);
    if (mode == "rx") {
        rx_target_count = rx_count = 1;
    }
    else if (mode == "tx") {
	tx_count = 1;
    }
    usleep_time   = usleep; 
    rx_block = rxblock;
    rt_prio = rtprio;
    //const char* mode = "r+"; //rx_count&&tx_count ? "r+": tx_count ? "w": "r";
    const char* write_mode = "w";
    const char* read_mode = "r";
    assert(rx_count||tx_count);

    const char* mode_used;
    if (mode == "rx") {
        fp = fopen(WRS_DEV, read_mode);
        std::string mode_used = read_mode;
    }
    else if (mode == "tx") {
        fp = fopen(WRS_DEV, write_mode);
        std::string mode_used = write_mode;
    }
    else {
        printf("Must set mode to rx or tx");
    }

    if (!fp) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "got fp: %d from fopen()", fp);
	assert(fp);
	fd = fileno(fp);

/*
	if (rx_block == 0){
		int flags = fcntl(fd, F_GETFL, 0);
		int rc = fcntl(fd, F_SETFL, flags|O_NONBLOCK);
		assert(rc != -1);
	}

*/
	for (int ii = 0; ii < PKT_LW; ++ii){
		tx_pkt[ii] = 0xaabb0000|ii;
	}

    if (rt_prio) {
        goRealTime(rt_prio);
    }
}

WrsTriggerDrv::~WrsTriggerDrv() {
}


void WrsTriggerDrv::copy_integers(uint32_t* dest, const uint32_t* src, size_t count) {
    unsigned int microseconds = 10000;
    int sleepflag = 0;
    for (size_t i = 0; i < count; ++i) {
        dest[i] = src[i];
        printf("writing %d", i);
    }
}


void WrsTriggerDrv::set_wr_ts_drives_soft_trigger() {
    // soft trigger
    setKnob(0, "/sys/module/acq420fmc/parameters/wr_ts_drives_soft_trigger", 1);
}

void WrsTriggerDrv::pulse_soft_trigger() {
    setKnob(0, "/dev/acq400.0.knobs/soft_trigger", 1);
}

const char* WrsTriggerDrv::ui(int argc, const char** argv)
{
	
    rx_target_count = rx_count = Env::getenv("RX", 1);
	tx_count = Env::getenv("TX", 0);
	usleep_time   = Env::getenv("US", 0);
	rx_block = Env::getenv("RX_BLOCK", 1);
    rt_prio = Env::getenv("RTPRIO", 0);

	const char* mode = rx_count&&tx_count ? "r+": tx_count ? "w": "r";

	assert(rx_count||tx_count);

	fp = fopen(WRS_DEV, mode);
	assert(fp);
	fd = fileno(fp);

	if (rx_block == 0){
		int flags = fcntl(fd, F_GETFL, 0);
		int rc = fcntl(fd, F_SETFL, flags|O_NONBLOCK);
		assert(rc != -1);
	}


	for (int ii = 0; ii < PKT_LW; ++ii){
		tx_pkt[ii] = 0xaabb0000|ii;
	}

    if (rt_prio) {
        goRealTime(rt_prio);
    }
	return 0;
}

void WrsTriggerDrv::dump_pkt(u32* pkt, const char* id){
    printf("%4s: ", id);
	for (int ii = 0; ii < PKT_LW; ++ii){
		printf("%08x,", pkt[ii]);
	}
}

void WrsTriggerDrv::dump_ts(const char* id){
    printf("%4s: ", id);
	printf("%08x,", ts);
	
}

int WrsTriggerDrv::transmit(u32 *buf) {
    int rc = -1;
    u32 temp_write_buf[WRS_PKT_FULL_READ];
    // changed the write call to take buf instead of the test packet tx_pkt
    int bytes_written = write(fd, buf, sizeof(u32)*PKT_LW); 
    if (bytes_written == -1) {
	printf("Error on write() in WrsTriggerDrv::transmit()\nIs file open for writing?");
	exit(1);
    }
    assert(bytes_written == PKT_LW*sizeof(u32));
    if (verbose > 1) dump_pkt(buf, "TX"); 
    if (verbose > 1) printf("\n");
	
    if (bytes_written = 44) {
        rc = 40; //Caller expects rc to be bytes written = packet size (not packet + TS)
    }
    return rc;
}

int WrsTriggerDrv::receive(u32 *buf) {
    if (verbose) printf("Entering WrsTriggerDrv::receive()");
    int rc = -1;
    u32 temp_read_buf[WRS_PKT_FULL_READ];
	int bytes_read = read(fd, temp_read_buf, WRS_PKT_FULL_READ);
    if (bytes_read == WRS_PKT_FULL_READ) {
        u32 timestamp;
        memcpy(buf, &temp_read_buf[1], 10 * sizeof(u32));
        rx_pkt = buf;
        ts = temp_read_buf[0];
        rc = 40;
    }
    else {
        if (verbose) {
            perror("Error reading data");
        }
    }
    if (bytes_read == 44) {
        rc = 40;
    }
    return rc;
}

