/*
 * wrs_trigger.cpp: driver for WR streamers
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

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "wrs_trigger_int.h"
#include "wrs_trigger.h"
#include "WrsTriggerDrv.h"
#include "knobs.h"

int main(int argc, const char* argv[])
{
 //   WrsTriggerDrv drv;
    
    // outputs ST on every TS for interrupt latency eval
//    drv.set_wr_ts_drives_soft_trigger();
    
    u32 interrup = 0;
    int fd = open("/dev/acq400.0.wr_pkt_rx", O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open device" << std::endl;
        return 1;
    }

    std::cout << "Waiting for interrupt..." << std::endl;

    while (true) {
        read(fd, &interrup, sizeof(interrup));
        // immediately set soft trigger to zero
        //drv.pulse_soft_trigger();
        // RX side: dump memory 
        //drv.dump_rx();
        // std::cout << "Interrupt detected! interrup = " << interrup << std::endl;
    }
    close(fd);
    return 0;
}

