/* ------------------------------------------------------------------------- */
/* connectto.c - dt100 network attached satellite DAQ                        */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2003 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file connectto.cpp socket client utils. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "connect_to.h"

char *program_name;

#define EXIT _exit

/* error - print a diagnostic and optionally exit */
void error( int status, int err, const char *fmt, ... )
{
	va_list ap;

	va_start( ap, fmt );
	fprintf( stderr, "%s: ", program_name );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
	if ( err )
		fprintf( stderr, ": %s (%d)\n", strerror( err ), err );
	if ( status )
		EXIT( status );
}

/* set_address - fill in a sockaddr_in structure */
void set_address(const char *hname, const char *sname,
		  struct sockaddr_in *sap, const char *protocol )
{
	struct servent *sp;
	struct hostent *hp;
	char *endptr;
	short port;

	bzero( sap, sizeof( *sap ) );
	sap->sin_family = AF_INET;
	if ( hname != NULL ){
		if ( !inet_aton( hname, &sap->sin_addr ) ){
			hp = gethostbyname( hname );
			if ( !hp ){
				error( 1, 0, "unknown host: %s\n", hname );
			}
			sap->sin_addr = *( struct in_addr * )hp->h_addr;
		}
	}
	else{
		sap->sin_addr.s_addr = htonl( INADDR_ANY );
	}
	port = strtol( sname, &endptr, 0 );
	if ( *endptr == '\0' ){
		sap->sin_port = htons( port );
	}else{
		sp = getservbyname( sname, protocol );
		if ( !sp ){
			error( 1, 0, "unknown service: %s\n", sname );
		}
		sap->sin_port = sp->s_port;
	}
}

/* readline - read a newline terminated record */
int readline( SOCKET fd, char *bufptr, size_t len )
{
	char *bufx = bufptr;
	static char *bp;
	static int cnt = 0;
	static char b[1];
	char c;

	while ( --len > 0 )
	{
		if ( --cnt <= 0 )
		{
			cnt = read( fd, b, sizeof( b ));
			if ( cnt < 0 )
			{
				fprintf(stderr, "read err %d\n", errno);

				if ( errno == EINTR )
					continue;
				return -1;
			}
			if ( cnt == 0 )
				return 0;
			bp = b;
		}
		c = *bp++;
		*bufptr++ = c;
		if ( c == '\n' )
		{
			*bufptr = '\0';
			return bufptr - bufx;
		}
	}
	errno = EMSGSIZE;
	return -1;
}

void connect_to(
        SOCKET sock, const char *remotehost, const char *remoteport)
{
        struct sockaddr_in peer;

        set_address(remotehost, remoteport, &peer, "tcp" );

        if (connect(sock, (struct sockaddr *)&peer, sizeof(peer))){
                error(1, errno, "connect failed SOCK" );
        }
}

FILE* connect_to_stream(const char *remotehost, const char *remoteport, const char* mode)
{
	int sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1){
	        fprintf(stderr, "Could not create socket");
	        exit(1);
	}
	connect_to(sock, remotehost, remoteport);
	FILE *fp = fdopen(sock, mode);
	if (fp == 0){
		perror("fdopen() failed");
		exit(1);
	}

	return fp;
}


