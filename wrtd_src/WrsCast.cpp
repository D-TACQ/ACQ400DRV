/*
 * WrsCast.cpp
 *
 *  Created on: 10 Jan 2025
 *      Author: pgm
 */

#include "Multicast.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <iostream>
#include <string>

#include "knobs.h"
#include "wrs_trigger_int.h"
#include "WrsTriggerDrv.h"


class WrsCastImpl : public WrsCast {

protected:
	const char *group;
	int port;
	struct sockaddr_in addr;
	int sock;
	socklen_t addrlen;
	int verbose;

public:
    int fd;
    u32 interrup = 0;
	WrsCastImpl(const char* _group, int _port):
		group(_group), port(_port), verbose(0)
	{
        if (getenv("WrsCastVerbose")) {
            verbose = atoi(getenv("WrsCastVerbose"));
        }

        if (verbose > 1) printf("WrsCastImpl()\n");

        // outputs ST on every TS for interrupt latency eval
      
    }

	virtual int sendto(const void* message, int len) {
		if (verbose > 1) printf("WrsCastImpl::sendto()\n");
        return -1;
	}
	virtual int recvfrom(void* message, int len) {
        if (verbose > 1) printf("WrsCastImpl::recvfrom()\n");
		return -1;
	}
};

class WrsCastSender : public WrsCastImpl {

public:
    WrsTriggerDrv drv;
	WrsCastSender(const char* _group, int _port):
	    WrsCastImpl(_group, _port), drv("tx", 15, 0, false)
	{
	    drv.verbose = verbose;
	}
	virtual int sendto(const void* message, int len) {
        if (verbose > 1) printf("WrsCastSender()::sendto 99\n");
	u32 *tx_message_u32 = (u32*)message;
        int rc = this->drv.transmit(tx_message_u32);
	// decrement drv.tx_count
        if (verbose > 1) printf("sent a message\n Also got rc = %d", rc);
        return 0;
	}
};


class WrsCastReceiver : public WrsCastImpl {
public:
    	WrsTriggerDrv drv;
	WrsCastReceiver(const char* _group, int _port):
	    WrsCastImpl(_group, _port), drv("rx", 15, 0, true)
	{
		if (verbose > 1) printf("WrsCastReceiver() 99\n");
		drv.verbose = verbose;
	}

	virtual int recvfrom(void* message, int len) {
        if (verbose > 1) printf("WrsCastReceiver::recvfrom() 01\n");
        int rc = -1;
        
        
        // RX side: dump memory 
        u32 *message_u32 = (u32*)message;
        rc = drv.receive(message_u32);
        if (verbose > 1) drv.dump_pkt(drv.rx_pkt, "RX"); 
        if (verbose > 1) drv.dump_ts("TS");
        if (verbose > 1) printf("\n");
        if (verbose > 1) printf("got a message\n Also got rc = %d", rc);

        if (rc < 0) {
            perror("recvfrom");
            exit(1);
        }
        
        
        if (rc == 1) {
            perror("recvfrom returns 1");
            exit(1);
        }

        if (verbose > 1) printf("WrsCastReceiver()::recvfrom 99, rc = %d\n", rc);
		return rc;
	}
};

MultiCast& WrsCast::factory(const char* group, int port, enum MC mode)
{
	switch(mode){
	case MC_SENDER:
		return *new WrsCastSender(group, port);
	case MC_RECEIVER:
	default:
		return *new WrsCastReceiver(group, port);
	}
}
