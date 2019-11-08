/*
 * Multicast.h
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */

#ifndef MULTICAST_H_
#define MULTICAST_H_


class MultiCast {

public:
	virtual ~MultiCast() {}

	virtual int sendto(const void* message, int len) = 0;
	virtual int recvfrom(void* message, int len) = 0;

	enum MC { MC_SENDER, MC_RECEIVER };
	static MultiCast& factory(const char* group, int port, enum MC mode);
};


#endif /* MULTICAST_H_ */
