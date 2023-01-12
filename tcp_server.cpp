/*
 * tcp_server.cpp
 *
 *  Created on: 16 Aug 2014
 *      Author: pgm
 */

/* include includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tcp_server.h"

typedef int SOCKET;

#define isvalidsock(s)	((s) >= 0)

#define NLISTEN 10	/* max concurrent threads ... loads */

static void set_address(
        const char *hname, const char *sname,
        struct sockaddr_in *sap, const char *protocol )
/* set_address - fill in a sockaddr_in structure */
{
        struct servent *sp;
        struct hostent *hp;
        char *endptr;
        short port;

        printf("set_address 01\n");

        bzero(sap, sizeof( *sap ));
        sap->sin_family = AF_INET;
        if ( hname != NULL ) {
                if ( !inet_aton( hname, &sap->sin_addr ) ) {
                        hp = gethostbyname( hname );
                        if (hp == NULL){
                        	printf("unknown host %s\n", hname);
                                perror("unknown host");
                                exit(1);
                        }
                        sap->sin_addr = *( struct in_addr * )hp->h_addr;
                }
        }else{
                sap->sin_addr.s_addr = htonl( INADDR_ANY );
        }
        port = strtol( sname, &endptr, 0 );

        if ( *endptr == '\0' ){
                sap->sin_port = htons( port );
        }else{
                sp = getservbyname( sname, protocol );
                if ( sp == NULL ){
                	printf("unknown service %s\n", sname);
                        perror("unknown service");
                	exit(1);
        	}
                sap->sin_port = sp->s_port;
        }
        printf("set_address 99\n");
}

static ServerInfo null_callback;
static ServerInfo* G_server_info = &null_callback;

static void reaper(int sig)
{
        int ws, rc;

        while ((rc = waitpid(-1, &ws, WNOHANG)) > 0){
        	G_server_info->onReap(rc);
        }
}


static SOCKET build_socket(void)
{
        const int on = 1;

        SOCKET s = socket( AF_INET, SOCK_STREAM, 0 );
        if (!isvalidsock(s)){
        	perror("socket call failed"); exit(1);
        }

        if (setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on)){
                perror("setsockopt failed"); exit(1);
        }
/*
        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                       (char*)&S_BUFLEN, sizeof(S_BUFLEN)) ){
                error(1, errno, "setsockopt() SO_RCVBUF failed\n");
        }
        if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
                       (char*)&S_BUFLEN, sizeof(S_BUFLEN)) ){
                error(1, errno, "setsockopt() SO_SNDBUF failed\n");
        }
*/
        return s;
}

int inetd_tcp_wait(int (*interpreter)(FILE* fin, FILE* fout))
{
	struct sockaddr_in peer;
	socklen_t peerlen = sizeof peer;

	printf("accept\n");

	SOCKET s1 = accept(0, (struct sockaddr *)&peer, &peerlen);

	if (!isvalidsock( s1 )){
	       perror("accept failed");
	}

	interpreter(fdopen(s1, "r"), fdopen(s1, "w"));
	shutdown(s1, SHUT_RDWR);
	exit(0);
}

int tcp_server(const char* host, const char* port,
		int (*interpreter)(FILE* fin, FILE* fout),
		ServerInfo* serverInfo)
{
	printf("tcp_server %s\n", port);
	struct sockaddr_in local;
	SOCKET skt;

	if (serverInfo){
		G_server_info = serverInfo;
	}
	signal(SIGCHLD, reaper);
	set_address(host, port, &local, "tcp");
	skt = build_socket();

	//printf("bind\n");
        if (bind(skt, (struct sockaddr *)&local, sizeof(local))){
                perror("bind failed"); exit(1);
        }
        //printf("listen\n");
        if (listen(skt, NLISTEN )){
                perror("listen failed" ); exit(1);
        }

        while(1){
        	struct sockaddr_in peer;
        	socklen_t peerlen = sizeof peer;
        	pid_t child;

        	SOCKET s1 = accept(skt, (struct sockaddr *)&peer, &peerlen);
        	//printf("accept");

        	if (!isvalidsock( s1 )){
        	       perror("accept failed");
        	}

        	if ((child = fork()) == 0){
        		interpreter(fdopen(s1, "r"), fdopen(s1, "w"));
        		shutdown(s1, SHUT_RDWR);
        		exit(0);
        	}else{
        		G_server_info->onConnect(child, peer);

        		if (close(s1)){
        			perror("close");
        		}
        	}
        }
}





