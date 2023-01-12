/*
 * tcp_server.h
 *
 *  Created on: 16 Aug 2014
 *      Author: pgm
 */

#ifndef TCP_SERVER_H_
#define TCP_SERVER_H_

int inetd_tcp_wait(int (*interpreter)(FILE* fin, FILE* fout));

class ServerInfo {
public:
	virtual void onConnect(pid_t child, struct sockaddr_in& s_in) {}
	virtual void onReap(pid_t child) {}
};

int tcp_server(const char* host, const char* port,
		int (*interpreter)(FILE* fin, FILE* fout),
		ServerInfo* serverInfo = 0);



#endif /* TCP_SERVER_H_ */
