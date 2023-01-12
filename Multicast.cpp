/*
 * Multicast.cpp
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */


#include "Multicast.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>



class MultiCastImpl : public MultiCast {

protected:
	const char *group;
	int port;
	struct sockaddr_in addr;
	int sock;
	socklen_t addrlen;
	int verbose;

public:
	MultiCastImpl(const char* _group, int _port):
		group(_group), port(_port), verbose(0)
	{
		  /* set up socket */
		   sock = socket(AF_INET, SOCK_DGRAM, 0);
		   if (sock < 0) {
		     perror("socket");
		     exit(1);
		   }

		   bzero((char *)&addr, sizeof(addr));
		   addr.sin_family = AF_INET;
		   addr.sin_addr.s_addr = htonl(INADDR_ANY);
		   addr.sin_port = htons(port);
		   addrlen = sizeof(addr);
		   if (getenv("MultiCastVerbose")){
			   verbose = atoi(getenv("MultiCastVerbose"));
		   }
		   if (multicast_if){
			   struct in_addr localInterface;
			   localInterface.s_addr = inet_addr(multicast_if);

			   if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
				   perror("ERROR setting local interface");
				   exit(1);
			   }else{
				   fprintf(stderr, "MultiCastImpl set IP_MULTICAST_IF %s\n", multicast_if);
			   }
		   }
	}
	virtual int sendto(const void* message, int len) {
		return -1;
	}
	virtual int recvfrom(void* message, int len) {
		return -1;
	}
};
const char* MultiCast::multicast_if;

class MultiCastSender : public MultiCastImpl {

public:
	MultiCastSender(const char* _group, int _port):
		MultiCastImpl(_group, _port)
	{}
	virtual int sendto(const void* message, int len) {
		addr.sin_addr.s_addr = inet_addr(group);
		int rc = ::sendto(sock, message, len, 0, (struct sockaddr *) &addr, addrlen);
		if (rc < 0) {
			perror("sendto");
			exit(1);
		}
		return rc;
	}
};

class MultiCastReceiver : public MultiCastImpl {
	struct ip_mreq mreq;

public:
	MultiCastReceiver(const char* _group, int _port):
		MultiCastImpl(_group, _port)
	{
		if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			perror("bind");
			exit(1);
		}
		mreq.imr_multiaddr.s_addr = inet_addr(group);
		if (multicast_if != 0){
			mreq.imr_interface.s_addr = inet_addr(multicast_if);
		}else{
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		}
		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				&mreq, sizeof(mreq)) < 0) {
			perror("setsockopt mreq");
			exit(1);
		}
		if (verbose) printf("MultiCastReceiver() 99\n");
	}

	virtual int recvfrom(void* message, int len) {

		if (verbose > 1 )printf("MultiCastReceiver()::recvfrom 01\n");
		int rc = ::recvfrom(sock, message, len, 0,
				(struct sockaddr *) &addr, &addrlen);
		if (rc < 0) {
			perror("recvfrom");
			exit(1);
		}
		if (verbose > 1) printf("MultiCastReceiver()::recvfrom 99 => %d\n", rc);
		return rc;
	}
};

MultiCast& MultiCast::factory(const char* group, int port, enum MC mode)
{
	switch(mode){
	case MC_SENDER:
		return *new MultiCastSender(group, port);
	case MC_RECEIVER:
	default:
		return *new MultiCastReceiver(group, port);
	}
}

