/* wrtd.cpp : White Rabbit Time Distribution                  	 	     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2019 pgm, D-TACQ Solutions Ltd                            *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Created on: 19 Sep 2019                                     *
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
 * wrtd.cpp : White Rabbit Time Distribution
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 *
 * - Usage
 *   wrtd tx
 *   	waits for incoming external trigger, sends wrtd trigger packet set for a [near] future time
 *   wrtx tx_immediate txi
 *   	sends trigger packet immediately (test mode) .. this is a soft trigger, really
 *   txq
 *   	"Quick packet: all receivers action immediately on receipt
 *   txa --at TSPEC
 *   	Trigger at time
 *   		+s[:ns]  : relative round up to coming second, add seconds [, nsec]
 *   		Ts[:ns]  : absolute time from epoch TAI
 *   		Us[:ns]  : absolute time from epoch UTC (for convenience)
 *
 *   		alt  [+UT]s.fractional_sec
 *   		eg
 *   			+10.5 => relative, +10s + 500000000 ns
 *
 *   		acq2106_319> date +%s
		1636025317
		acq2106_319> date @1636025317
		Thu Nov  4 11:28:37 UTC 2021

		wrtd_txa --at U$(($(date +%s)+10) 1      # trigger at calendar time now + 10s
		wrtd_txa --at U$(($(date +%s)+10) 1      # trigger at calendar time now + 10s
		wrtd_txa --at U$((1636025317) 1      	# trigger at calendar Thu Nov  4 11:28:37 UTC 2021
 *
 *   wrtd rx
 *   	receives network triggers and configures WRTT to fire at specified time
 *
 * -command line options
 *   try --help for full list
 * - environment
 *   WRTD_RX_MATCHES=m1[,m2,m3...]   # receiver matches on multiple strings, not just default ["acq2106"[7]], operates WRTT[0]
 *   WRTD_RX_MATCHES1=mx[my..]       # match strings operate WRTT1. WRTT0 has priority, strings MUST be unique.
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

#include "wrtd.h"
#include "wrtd_TS.h"


#include "acq-util.h"

#include "wrtd_message.h"

#define LOCAL_CLKDIV_AUTO	77777777

struct poptOption opt_table[] = {
	{ "tickns", 0, POPT_ARG_INT, &wrtd_TS_ns::ns_per_tick, 0, "tick size nsec" },
	{ "dns", 'd', POPT_ARG_INT, &wrtd_ns::dns, 0, "nsec to add to current time" },
	{ "delta_ns", 'd', POPT_ARG_INT, &wrtd_ns::dns, 0, "nsec to add to current time" },
	{ "rt_prio", 'p', POPT_ARG_INT, &wrtd_ns::rt_prio, 0, "real time priority" },
	{ "on_next_second", 'n', POPT_ARG_INT, &wrtd_ns::ons, 0, "trigger next second, on the second, for comparison with PPS" },
	{ "verbose", 'v', POPT_ARG_INT, &wrtd_message_ns::verbose, 0, "debug" },
	{ "local_clkdiv", 'l', POPT_ARG_INT, &wrtd_ns::local_clkdiv, 0, "local clock divider" },
	{ "local_clkoffset", 'L', POPT_ARG_INT, &wrtd_ns::local_clkoffset, 0, "local clock offset" },
	{ "max_tx", 0, POPT_ARG_INT, &wrtd_message_ns::max_tx, 'm', "maximum transmit count" },
	{ "tx_id", 0, POPT_ARG_STRING, &wrtd_message_ns::tx_id, 0, "txid: default is $(hostname)" },
	{ "at", 0, POPT_ARG_STRING, &wrtd_message_ns::tx_at, 0, "at [+UT]sss[:.]ttt\n"
	  "at: +: relative, U: absolute UTC T: absolute TAI\n"
	  "at: tx at +s[:nsec] or [UT]sec-since-epoch[:nsec]\n"
	  "at: tx at +s[.frac] or [UT]sec-since-epoch[.frac]\n"
	},
	{ "delay01", 0, POPT_ARG_INT, &wrtd_ns::delay01, 0, "in double tap, delay to second trigger" },
	{ "tx_mask", 0, POPT_ARG_INT, &wrtd_message_ns::tx_mask, 0, "mask for TIGA trigger tx" },
	{ "dev_ts", 0, POPT_ARG_STRING, &wrtd_ns::dev_ts, 0, "timestamp device eg may be a TIGA site.." },
	POPT_AUTOHELP
	POPT_TABLEEND
};

const char* ui_get_cmd_name(const char* path)
{
	char* cmd_name = new char[strlen(path)+1];
	strcpy(cmd_name, path);
	return basename(cmd_name);
}


const char* ui(int argc, const char** argv)
{
	const char* cmd_name = ui_get_cmd_name(argv[0]);

        poptContext opt_context =
                        poptGetContext(argv[0], argc, argv, opt_table, 0);
        int rc;

        wrtd_TS_ns::ns_per_tick 	= 	Env::getenv("WRTD_TICKNS", 	50.0	);
        wrtd_ns::dns 		= 	Env::getenv("WRTD_DELTA_NS", 	50000000);
        wrtd_message_ns::tx_id 	= 	Env::getenv("WRTD_ID", 	"WRTD0"	);
        wrtd_message_ns::verbose 	= 	Env::getenv("WRTD_VERBOSE", 	0	);
        wrtd_ns::rt_prio	= 	Env::getenv("WRTD_RTPRIO", 	0	);
        wrtd_ns::delay01	= 	Env::getenv("WRTD_DELAY01", 	1000000	);
        wrtd_message_ns::tx_mask	= 	Env::getenv("WRTD_TX_MASK", 	0	);
        wrtd_ns::dev_ts	= 	Env::getenv("WRTD_DEV_TS",    DEV_TS	);
        wrtd_ns::local_clkoffset = 	Env::getenv("WRTD_LOCAL_CLKOFFSET",	0);
        wrtd_ns::local_clkdiv = 	Env::getenv("WRTD_LOCAL_CLKDIV",    	LOCAL_CLKDIV_AUTO);

        const char* ip_multicast_if = ::getenv("WRTD_MULTICAST_IF");
        if (ip_multicast_if){
        	MultiCast::set_IP_MULTICAST_IF(ip_multicast_if);
        }
        if (!is_tiga() && wrtd_ns::local_clkdiv == LOCAL_CLKDIV_AUTO){
        	Knob clkdiv(1, "clkdiv");
        	Knob modname(1, "module_name");
        	if (strstr(modname(), "acq48")){
        		wrtd_ns::local_clkdiv = 1;
        		wrtd_ns::local_clkoffset = 0;
        	}else{
        		clkdiv.get((unsigned*)&wrtd_ns::local_clkdiv);
        		wrtd_ns::local_clkoffset = 2;
        	}
        }
        if (wrtd_ns::local_clkdiv == LOCAL_CLKDIV_AUTO){
        	wrtd_ns::local_clkdiv = 1;
        }
        while ((rc = poptGetNextOpt( opt_context )) >= 0 ){
                switch(rc){
                case 'm':
                	wrtd_ns::max_tx_specified = true;
                	break;
                default:
                        ;
                }
        }
        wrtd_TS_ns::ticks_per_sec = NSPS / wrtd_TS_ns::ns_per_tick;
        wrtd_TS_ns::delta_ticks = wrtd_ns::dns / wrtd_TS_ns::ns_per_tick;

        if (wrtd_message_ns::verbose) fprintf(stderr, "ns per tick: %.3f ticks per s: %u delta_ticks %u\n",
        		wrtd_TS_ns::ns_per_tick, wrtd_TS_ns::ticks_per_sec, wrtd_TS_ns::delta_ticks);

        const char* mode = "wrtd_rx";

        if (strcmp(cmd_name, "wrtd") == 0){
        	 mode = poptGetArg(opt_context);
        }
        if (strcmp(mode, "ts_diff") == 0){
        	TS::do_ts_diff(poptGetArg(opt_context), poptGetArg(opt_context));
        	exit(0);
        }

        const char* tx_id = poptGetArg(opt_context);
        if (tx_id){
        	if (isdigit(tx_id[0])){
        		wrtd_message_ns::max_tx = atoi(tx_id);		// args N TXID
        		wrtd_ns::max_tx_specified = true;
        		tx_id = poptGetArg(opt_context);
        		if (tx_id && !isdigit(tx_id[0])){
        			wrtd_message_ns::tx_id = tx_id;
        		}
        	}else{
        		wrtd_message_ns::tx_id = tx_id;			// args TXID
        	}
        }
        							// else use defaults
        wrtd_ns::delay01 /= wrtd_TS_ns::ns_per_tick;

	if (wrtd_ns::rt_prio){
		goRealTime(wrtd_ns::rt_prio);
	}
        return mode;
}

Receiver* Receiver::instance(bool chatty)
{
	static Receiver* _instance;

	if (!_instance){
		if (Env::getenv("WRTD_TIGA", 0)){
			_instance = new TIGA_Receiver;
		}else{

                    _instance = new ACQ400Receiver;
		}
		chatty = Env::getenv("WRTD_RX_CHATTY", 0);
		_instance->chatty = chatty;
	}
	return _instance;
}

int rx() {
       return ACQ400Receiver::instance()->event_loop(
                       TSCaster::factory(wrtd_ns::mc_factory(wrtd_message_ns::group, wrtd_message_ns::port, MultiCast::MC_RECEIVER)));
}

int tx() {
	if (!wrtd_ns::max_tx_specified){
		wrtd_message_ns::max_tx = MAX_TX_INF;
	}
	if (wrtd_message_ns::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	Transmitter t(wrtd_ns::dev_ts);
	Receiver* r = Env::getenv("WRTD_LOCAL_RX_ACTION", 0)? Receiver::instance(): 0;
	return t.event_loop(TSCaster::factory(wrtd_ns::mc_factory(wrtd_message_ns::group, wrtd_message_ns::port, MultiCast::MC_SENDER)), r);
}

int txi() {
	if (wrtd_message_ns::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	Transmitter t(DEV_CUR, 2*wrtd_ns::dns/1000);
	Receiver* r = Env::getenv("WRTD_LOCAL_RX_ACTION", 0)? Receiver::instance(): 0;
	return t.event_loop(TSCaster::factory(wrtd_ns::mc_factory(wrtd_message_ns::group, wrtd_message_ns::port, MultiCast::MC_SENDER)), r);
}

int txq() {
	if (wrtd_message_ns::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	TSCaster& comms = TSCaster::factory(wrtd_ns::mc_factory(wrtd_message_ns::group, wrtd_message_ns::port, MultiCast::MC_SENDER));
	comms.sendraw(TS_QUICK);
	return 0;
}

Txa& Txa::factory() {
	return *new Acq400Txa;
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
	}else if (strcmp(mode, "tx") == 0){
		return sleep_if_notenabled("WRTD_TX") || tx();
	}else if (strcmp(mode, "rx") == 0){
		return sleep_if_notenabled("WRTD_RX") || rx();
	}else{
		printf("Invalid argument to wrtd. WRTD_TX or WRTD_RX must be set as env var.");
		printf("Or program must be called as wrtd_txq, wrtd_txi or wrtd_txa.");
		exit(1);
	}
}

