/* ------------------------------------------------------------------------- */
/* iclient3.h - dt100 network attached satellite DAQ                         */
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

#define SOCKET int
void error( int status, int err, char *fmt, ... );
void set_address(const char *hname, const char *sname,
			 struct sockaddr_in *sap, const char *protocol );
int readline( SOCKET fd, char *bufptr, size_t len );

void connect_to(
        SOCKET sock, const char *remotehost, const char *remoteport);


FILE* connect_to_stream(const char *remotehost, const char *remoteport, const char* mode = "r");

#define EPEERDISCONNECT  3700
