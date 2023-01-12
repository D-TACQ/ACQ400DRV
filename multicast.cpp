/*
 * multicast.cpp
 *
 *  Created on: 18 Aug 2020
 *      Author: pgm
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glob.h>
#include <libgen.h>

#include <assert.h>

#include "split2.h"

#include "popt.h"

#include "local.h"
#include "Env.h"
#include "File.h"
#include "Knob.h"
#include "Multicast.h"


namespace G {
	const char* group;
	int port;
}

struct poptOption opt_table[] = {
	{
		"group", 0, POPT_ARG_STRING, &G::group, 0, "group address"
	},
	{
		"port", 0, POPT_ARG_INT, &G::port, 0, "port"
	},
	{
		"rx", 0, POPT_ARG_NONE, 0, 'r', "receive a line"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};


int receive()
{
       	char* aline = new char[1024];
        int rc = MultiCast::factory(G::group, G::port, MultiCast::MC_RECEIVER).recvfrom(aline, 1024);
        aline[rc] = '\0';
        rc = strcspn(aline, "\r\n");
        aline[rc] = '\0';
        printf("%s\n", aline);
        delete [] aline;
        return 0;
}

int send(const char* message)
{
	return MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER).sendto(message, strlen(message));
}

int main(int argc, const char* argv[])
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
        while ((rc = poptGetNextOpt( opt_context )) >= 0 ){
                switch(rc){
                case 'r':
                	return receive();
                default:
                        ;
                }
        }
        const char *message = poptGetArg(opt_context);

        if (message != 0){
        	return send(message);
        }else{
        	fprintf(stderr, "ERROR: no message to send\n");
        	return(-1);
        }
}

