/* soft_wrtd.cpp : White Rabbit Time Distribution from non WR host           */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2021 pgm, D-TACQ Solutions Ltd                            *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Created on: 5 Nov 2021                                   *
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * wrtd_txi
 * wrtd_txq
 * wrtd_txa
 *
 * wrtd_rx
 *
 * As per wrtd.cpp, but with exceptions:
 * #1 Host does NOT have WR, use regular UTC/ntp time
 * #2 rx is a message log only, no actual hardware triggers are created
 *
 * Use Case:
 * A top level controller can control and monitor a hard WR WRTD system
 * As a "soft controller", this can be any PC on the same subnet.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glob.h>
#include <libgen.h>

#include <assert.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "split2.h"

#include "popt.h"

#include "local.h"
#include "Env.h"
#include "File.h"
#include "Knob.h"
#include "Multicast.h"

#include "wrtd_TS.h"


#include "acq-util.h"

#include "wrtd_message.h"

namespace G {
        unsigned dns = 40*M1;				// delta nsec
        bool max_tx_specified;				// TRUE if UI changed max_tx

        int rt_prio = 0;

        int delay01;					// tr==2? trg0 at time t, trg1 at t+delay01
        int ons;					// on next second
}

const char* ui_get_cmd_name(const char* path)
{
	char* cmd_name = new char[strlen(path)+1];
	strcpy(cmd_name, path);
	return basename(cmd_name);
}

struct poptOption opt_table[] = {
	{
	  "tickns", 0, POPT_ARG_INT, &G::ns_per_tick, 0, "tick size nsec"
	},
	{
	  "dns", 'd', POPT_ARG_INT, &G::dns, 0, "nsec to add to current time"
	},
	{
	  "delta_ns", 'd', POPT_ARG_INT, &G::dns, 0, "nsec to add to current time"
	},
	{
	  "rt_prio", 'p', POPT_ARG_INT, &G::rt_prio, 0, "real time priority"
	},
	{
	  "on_next_second", 'n', POPT_ARG_INT, &G::ons, 0, "trigger next second, on the second, for comparison with PPS"
	},
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	{
	  "max_tx", 0, POPT_ARG_INT, &G::max_tx, 'm', "maximum transmit count"
	},
	{
          "tx_id", 0, POPT_ARG_STRING, &G::tx_id, 0, "txid: default is $(hostname)"
	},
	{
	  "at", 0, POPT_ARG_STRING, &G::tx_at, 0, "at [+UT]sss[:.]ttt\n"
	  "at: +: relative, U: absolute UTC T: absolute TAI\n"
	  "at: tx at +s[:nsec] or [UT]sec-since-epoch[:nsec]\n"
	  "at: tx at +s[.frac] or [UT]sec-since-epoch[.frac]\n"
	},
	{
	  "delay01", 0, POPT_ARG_INT, &G::delay01, 0, "in double tap, delay to second trigger"
	},
	{
	  "tx_mask", 0, POPT_ARG_INT, &G::tx_mask, 0, "mask for TIGA trigger tx"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};


const char* ui(int argc, const char** argv)
{
	const char* cmd_name = ui_get_cmd_name(argv[0]);

        poptContext opt_context =
                        poptGetContext(argv[0], argc, argv, opt_table, 0);
        int rc;

        G::ns_per_tick 	= 	Env::getenv("WRTD_TICKNS", 	50.0	);
        G::dns 		= 	Env::getenv("WRTD_DELTA_NS", 	50000000);
        G::tx_id 	= 	Env::getenv("WRTD_ID", 	"WRTD0"	);
        G::verbose 	= 	Env::getenv("WRTD_VERBOSE", 	0	);
        G::rt_prio	= 	Env::getenv("WRTD_RTPRIO", 	0	);
        G::delay01	= 	Env::getenv("WRTD_DELAY01", 	1000000	);
        G::tx_mask	= 	Env::getenv("WRTD_TX_MASK", 	0	);


        if (! ::getenv("WRTD_FULL_MESSAGE")){
        	::setenv("WRTD_FULL_MESSAGE", "1", 1);
        }
        const char* ip_multicast_if = ::getenv("WRTD_MULTICAST_IF");
        if (ip_multicast_if){
        	MultiCast::set_IP_MULTICAST_IF(ip_multicast_if);
        }

        while ((rc = poptGetNextOpt( opt_context )) >= 0 ){
                switch(rc){
                case 'm':
                	G::max_tx_specified = true;
                	break;
                default:
                        ;
                }
        }

        const char* mode = "wrtd_rx";

        if (strncmp(cmd_name, "soft_wrtd", 9) == 0){
        	 mode = poptGetArg(opt_context);
        	 if (mode == 0){
        		 fprintf(stderr, "setting default txa\n");
        		 mode = "txa";
        	 }
        }
        if (strcmp(mode, "ts_diff") == 0){
        	TS::do_ts_diff(poptGetArg(opt_context), poptGetArg(opt_context));
        	exit(0);
        }

        const char* tx_id = poptGetArg(opt_context);
        if (tx_id){
        	if (isdigit(tx_id[0])){
        		G::max_tx = atoi(tx_id);		// args N TXID
        		G::max_tx_specified = true;
        		tx_id = poptGetArg(opt_context);
        		if (tx_id && !isdigit(tx_id[0])){
        			G::tx_id = tx_id;
        		}
        	}else{
        		G::tx_id = tx_id;			// args TXID
        	}
        }
        							// else use defaults
        return mode;
}


Receiver* Receiver::instance(bool chatty)
{
	static Receiver* _instance;

	if (!_instance){
		_instance = new Receiver;
		_instance->chatty = true;
	}
	return _instance;
}

void get_local_env(void)
{
	G::verbose = Env::getenv("WRTD_VERBOSE", 0);
}

int txq() {
	if (G::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	TSCaster& comms = TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER));
	comms.sendraw(TS_QUICK);
	return 0;
}

int rx() {
       return Receiver::instance()->event_loop(
                       TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_RECEIVER)));
}

#include <time.h>


unsigned get_tai()
{
	time_t utc_sec = time(0);
	return utc_sec + 37;
}

int txi() {
	TSCaster& comms = TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER));
	comms.sendto(TS(get_tai()+1, 0));
	return 0;
}

class SoftTxa : public Txa {
protected:
	TS txa_validate_rel(unsigned sec, unsigned ns)
	{
		return TS(get_tai()+1+sec, ns/G::ns_per_tick);
	}

	TS txa_validate_abs(unsigned sec, unsigned ns)
	{
		unsigned tai_sec = get_tai() ;

		if (sec < tai_sec){
			fprintf(stderr, "ERROR: specified time @%u is less than current TAI @%u\n", sec, tai_sec);
			exit(1);
		}
		return TS(sec, ns/G::ns_per_tick);
	}
};

Txa& Txa::factory()
{
	return *new SoftTxa;
}


int main(int argc, const char* argv[])
{
	get_local_env();
	const char* mode = ui(argc, argv);
	const char* bn = basename((char*)argv[0]);

	if (strcmp(bn, "wrtd_txq") == 0 || strcmp(mode, "txq") == 0){
		return txq();
	}else if (strcmp(bn, "wrtd_txi") == 0 || strcmp(mode, "tx_immediate") == 0 || strcmp(mode, "txi") == 0){
		return txi();
	}else if (strcmp(bn, "wrtd_txa") == 0 || strcmp(mode, "txa") == 0){
		return Txa::factory()();
	}else{
		return rx();
	}
}



