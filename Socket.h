/*
 * Socket.h
 *
 *  Created on: 27 Jan 2019
 *      Author: pgm
 */

#ifndef SOCKET_H_
#define SOCKET_H_


class Socket {
public:
	virtual int send(const char* data, int len) = 0;
	virtual int readBuffer(char* data, int ndata) = 0;
	virtual ~Socket() {}


	static Socket* createIpSocket(const char *type, const char* host, const char * port);
	/** Factory. Valid types : "udp", "tcp" */
};



#endif /* SOCKET_H_ */
