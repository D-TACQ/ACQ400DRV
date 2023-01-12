/*
 * Socket.cpp
 *
 *  Created on: 27 Jan 2019
 *      Author: pgm
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Socket.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

class IpSocket : public Socket {
protected:
	int sockfd;
	struct sockaddr_in servaddr;
	struct hostent *server;

	IpSocket(const char* host, const char * port, int skt) : sockfd(skt) {
		if (sockfd < 0) {
			perror("ERROR opening socket");
			exit(EXIT_FAILURE);
		}
		server = gethostbyname(host);
		if (server == NULL) {
			fprintf(stderr,"ERROR, host not found %s\n", host);
			exit(EXIT_FAILURE);
		}
		bzero((char *) &servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		memcpy((char *)&servaddr.sin_addr.s_addr,
				(char *)server->h_addr, server->h_length);
		servaddr.sin_port = htons(strtoul(port, 0, 10));
	}
	virtual ~IpSocket() {}
	virtual int readBuffer(char* data, int ndata) { return -1; }
};

class TcpSocket : public IpSocket {

	FILE* fp;
public:
	TcpSocket(const char *host, const char* port) :
		IpSocket(host, port, socket(AF_INET, SOCK_STREAM, 0)), fp(0)
	{
		if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			perror("connect error");
			exit(EXIT_FAILURE);
		}
	}
	virtual ~TcpSocket() {}

	virtual int send(const char* data, int len) {
		/* MSG_DONTWAIT is really NOT helpful ..*/
		return ::send(sockfd, data, len, 0);
	}
	virtual int readBuffer(char* data, int ndata) {
		if (!fp) fp = fdopen(sockfd, "r");
		return fread(data, 1, ndata, fp);
	}
};

class UdpSocket : public IpSocket {

public:
	UdpSocket(const char *host, const char* port) :
		IpSocket(host, port, socket(AF_INET, SOCK_DGRAM, 0))
	{

	}
	virtual ~UdpSocket() {}

	virtual int send(const char* data, int len) {
		return sendto(sockfd, data, len,  0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	}
};

class StdoutSocket: public Socket {
public:
	StdoutSocket() {}

	virtual ~StdoutSocket() {}

	virtual int send(const char* data, int len) {
		return fwrite(data, len, 1, stdout);
	}
	virtual int readBuffer(char* data, int ndata) { return -1; }
};

Socket* Socket::createIpSocket(const char *type, const char* host, const char * port)
{
	if (strcmp(type, "udp") == 0){
		return new UdpSocket(host, port);
	}else if (strcmp(type, "tcp") == 0){
		return new TcpSocket(host, port);
	}else if (strcmp(type, "stdout") == 0){
		return new StdoutSocket;
	}else{
		return 0;
	}
}
