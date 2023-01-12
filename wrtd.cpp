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

		wrtd_txi --at U$(($(date +%s)+10) 1      # trigger at calendar time now + 10s
		wrtd_txi --at U$(($(date +%s)+10) 1      # trigger at calendar time now + 10s
		wrtd_txi --at U$((1636025317) 1      	# trigger at calendar Thu Nov  4 11:28:37 UTC 2021
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

#include "wrtd_TS.h"


#include "acq-util.h"

#include "wrtd_message.h"



#define DEV_TS		"/dev/acq400.0.wr_ts"	// blocking device returns TIMESTAMP
#define DEV_CUR		"/dev/acq400.0.wr_cur"	// noblock device returns current TAI in 7:ticks format
#define DEV_TAI		"/dev/acq400.0.wr_tai"	// noblock device returns current TAI in s
#define DEV_TRG0	"/dev/acq400.0.wr_trg0" // write trigger0 definition here
#define DEV_TRG1	"/dev/acq400.0.wr_trg1" // write trigger1 definition here

namespace G {
        unsigned dns = 40*M1;				// delta nsec
        unsigned local_clkdiv;				// Site 1 clock divider, set at start
        unsigned local_clkoffset;			// local_clk_offset eg 2 x 50nsec for ACQ42x

        bool max_tx_specified;				// TRUE if UI changed max_tx

        int rt_prio = 0;

        int delay01;					// tr==2? trg0 at time t, trg1 at t+delay01

        const char* dev_ts = DEV_TS;
        unsigned site;
        int ons;					// on next second

}

#define REPORT_THRESHOLD (G::dns/4)


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
	  "local_clkdiv", 'l', POPT_ARG_INT, &G::local_clkdiv, 0, "local clock divider"
	},
	{
	  "local_clkoffset", 'L', POPT_ARG_INT, &G::local_clkoffset, 0, "local clock offset"
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
	{
	  "dev_ts", 0, POPT_ARG_STRING, &G::dev_ts, 0, "timestamp device eg may be a TIGA site.."
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};


bool is_tiga()
{
	Knob k(0, "wr_tai_trg_s1");
	return k.exists();
}

const char* ui_get_cmd_name(const char* path)
{
	char* cmd_name = new char[strlen(path)+1];
	strcpy(cmd_name, path);
	return basename(cmd_name);
}

#define LOCAL_CLKDIV_AUTO	77777777

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
        G::dev_ts	= 	Env::getenv("WRTD_DEV_TS",    DEV_TS	);
        G::local_clkoffset = 	Env::getenv("WRTD_LOCAL_CLKOFFSET",	0);
        G::local_clkdiv = 	Env::getenv("WRTD_LOCAL_CLKDIV",    	LOCAL_CLKDIV_AUTO);

        const char* ip_multicast_if = ::getenv("WRTD_MULTICAST_IF");
        if (ip_multicast_if){
        	MultiCast::set_IP_MULTICAST_IF(ip_multicast_if);
        }
        if (!is_tiga() && G::local_clkdiv == LOCAL_CLKDIV_AUTO){
        	Knob clkdiv(1, "clkdiv");
        	Knob modname(1, "module_name");
        	if (strstr(modname(), "acq48")){
        		G::local_clkdiv = 1;
        		G::local_clkoffset = 0;
        	}else{
        		clkdiv.get((unsigned*)&G::local_clkdiv);
        		G::local_clkoffset = 2;
        	}
        }
        if (G::local_clkdiv == LOCAL_CLKDIV_AUTO){
        	G::local_clkdiv = 1;
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
        G::ticks_per_sec = NSPS / G::ns_per_tick;
        G::delta_ticks = G::dns / G::ns_per_tick;

        if (G::verbose) fprintf(stderr, "ns per tick: %.3f ticks per s: %u delta_ticks %u\n",
        		G::ns_per_tick, G::ticks_per_sec, G::delta_ticks);

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
        G::delay01 /= G::ns_per_tick;

	if (G::rt_prio){
		goRealTime(G::rt_prio);
	}
        return mode;
}






TS _adjust_ts(TS& ts0)
{
	int rem = ts0.ticks() % G::local_clkdiv;
	unsigned ticks = ts0.ticks();


	if (rem != 0){
		ticks += G::local_clkdiv - rem;
	}
	if (ticks > G::local_clkoffset){
		ticks -= G::local_clkoffset;
	}

	if (G::verbose > 1) fprintf(stderr, "adjust_ts: ts0 %u div %u rem %u off %u adj %u\n",
			ts0.ticks(), G::local_clkdiv, rem, G::local_clkoffset, ticks);

	return TS(ts0.secs(), ticks);
}
TS adjust_ts(TS& ts0)
{
	if (ts0 != TS::ts_quick && (G::local_clkdiv > 1 || G::local_clkoffset != 0)){
		return _adjust_ts(ts0);
	}else{
		return ts0;
	}
}

void _write_trg(FILE* fp, TS ts)
{
	int rc = fwrite(&ts.raw, sizeof(unsigned), 1, fp);
	if (rc < 1){
		perror("fwrite");
	}
	fflush(fp);
}



class ACQ400Receiver: public Receiver {

protected:
	const int ntriggers;
	const long dms;

	FILE **fp_trg;
	FILE *fp_cur;

	char* report_fname;
	char* report;

	ACQ400Receiver(int _ntriggers = 2) :  ntriggers(_ntriggers), dms(G::dns/M1), report_fname(new char[80]), report(new char[256]) {
		sprintf(report_fname, "/etc/acq400/%d/WRTD_REPORT", G::site);
		fp_trg = new FILE* [ntriggers];
		memset(fp_trg, 0, ntriggers*sizeof(FILE*));
		fp_trg[0] = fopen_safe(DEV_TRG0, "w");
		fp_trg[1] = fopen_safe(DEV_TRG1, "w");
		fp_cur = fopen_safe(DEV_CUR, "r");
	}
	virtual void onAction(TS& ts, TS& ts_adj){
		if (G::verbose){
			fprintf(stderr, "%s ts:%s ts_adj:%s mask:%x\n", PFN, ts.toStr(), ts_adj.toStr(), ts.mask);
		}
		if (ts.mask != 0){
			unsigned char mask = ts.mask;
			FILE *fp;

			for (int ii = 0; (ii < ntriggers) && mask; mask >>= 1, ++ii){
				if ((mask&1) && (fp = fp_trg[ii])){
					_write_trg(fp, ts_adj);
				}
			}
		}else if (G::trg < 2){
			_write_trg(fp_trg[G::trg], ts_adj);
		}else{							/* DOUBLE TAP */
			if (ts_adj != TS::ts_quick){
				TS ts2 = ts + G::delay01;
				_write_trg(fp_trg[0], ts_adj);
				_write_trg(fp_trg[1], adjust_ts(ts2));
			}else{
				_write_trg(fp_trg[0], TS_QUICK);
				usleep(G::delay01*G::ns_per_tick/1000);
				_write_trg(fp_trg[1], TS_QUICK);
			}
		}

	}

	void deferredAction(TS& ts, int nrx = 0)
	{
		if (fork() == 0){
			/* read wr_wait, tick up to within 1s, call action() */
			File tai_file(DEV_TAI);
			unsigned tai_sec;

			while (true){
				tai_sec = getvalue<unsigned>(tai_file);
				if (ts.secs() > tai_sec && ts.secs() - tai_sec < 7){
					ts.strip();
					action(ts, nrx);
					exit(0);
				}
				sleep(1);
			}
		}else{
			int status;
			/* reap any (previous) child */
			waitpid(-1, &status, WNOHANG);
		}
	}
public:
	virtual ~ACQ400Receiver() {
		fclose(fp_trg[0]);
		fclose(fp_trg[1]);
		fclose(fp_cur);
		delete [] report;
		delete [] report_fname;
	}
	virtual void action(TS& ts, int nrx = 0){
		if (G::verbose > 1) fprintf(stderr, "%s() TS:%s %08x\n", PFN, ts.toStr(), ts.raw);
		if (ts.is_abs_tai()){
			return deferredAction(ts, nrx);
		}
		TS ts_adj = adjust_ts(ts);
		onAction(ts, ts_adj);
		TS ts_cur;
		fread(&ts_cur.raw, sizeof(unsigned), 1, fp_cur);

		long dt = ts.diff(ts_cur);

		snprintf(report, 256, "Receiver:%d nrx:%u cur:%s ts:%s adj:%s diff:%ld %s\n",
				G::trg, nrx, ts_cur.toStr(), ts.toStr(), ts_adj.toStr(), dt, dt<0? "ERROR": "OK");

		FILE *fp_report = fopen(report_fname, "w");
		fprintf(fp_report, report);
		fclose(fp_report);
		if (G::verbose > 1){
			fprintf(stderr, report);
		}


		if (dt < 0){
			fprintf(stderr, "wrtd rx ERROR missed ts by %ld msec\n", dt/M1);
		}else if (dt < (long)REPORT_THRESHOLD){
			fprintf(stderr, "wrtd rx WARNING threshold %ld msec under limit %ld\n", dt/M1, dms);
		}
	}

	friend class Receiver;
};


class TIGA_Receiver: public ACQ400Receiver {

protected:
	TIGA_Receiver() : ACQ400Receiver(8)
	{
		G::local_clkdiv = G::local_clkoffset = 0;		// stub clock adjust
		if (G::verbose){
			fprintf(stderr, "TIGA_Receiver()\n");
		}

		glob_t globbuf;
		glob("/dev/acq400.0.wr_tiga_tt_s?", 0, NULL, &globbuf);
		for (unsigned ii = 0; ii < globbuf.gl_pathc; ++ii){
			const char* fn = globbuf.gl_pathv[ii];
			int site = fn[strlen(fn)-1]-'0';

			if (G::verbose){
				fprintf(stderr, "TIGA_Receiver() fn:\"%s\" site:%d\n", fn, site);
			}

			if (site >= 1 && site <= 6){
				fp_trg[site+1] = fopen_safe(fn, "w");		/* site1 => [2] */
			}
		}
		globfree(&globbuf);
	}

	friend class Receiver;
};

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
class Transmitter {
	FILE* fp;
	const int sleep_us;
public:
	Transmitter(const char* dev, int _sleep_us = 0) :
		fp(::fopen_safe(dev)), sleep_us(_sleep_us)
	{
	}
	virtual ~Transmitter(){
		fclose(fp);
	}
	int event_loop(TSCaster& comms, Receiver* local_rx) {
		if (G::max_tx == 0){
			return 0;
		}
		TS ts;
		for (unsigned ntx = 0; fread(&ts.raw, sizeof(unsigned), 1, fp) == 1; ++ntx){
			TS ts_tx = G::ons? ts.next_second(): ts + G::delta_ticks;
			ts_tx.mask = G::tx_mask;
			comms.sendto(ts_tx);
			if (local_rx){
				local_rx->action(ts_tx, ntx);
			}
			++ntx;
			if (G::verbose > 1) fprintf(stderr, "sender:ntx:%u ts:%s ts_tx:%s\n", ntx, ts.toStr(), ts_tx.toStr());
			if (G::max_tx != MAX_TX_INF && ntx >= G::max_tx){
				break;
			}else if (sleep_us){
				usleep(sleep_us);
			}
		}
		return 0;
	}
};

void get_local_env(void)
{
	G::verbose = Env::getenv("WRTD_VERBOSE", 0);
	G::site = Env::getenv("SITE", 11);
	char envname[80];
	sprintf(envname, "/dev/shm/wr%d.sh", G::site);
	get_local_env(envname, G::verbose);
}

int sleep_if_notenabled(const char* key)
{
	if (Env::getenv(key, 0) == 0){
		if (G::verbose){
			fprintf(stderr, "%s==0, sleep(9999)\n",key);
		}
		sleep(9999);
		return 1;
	}else{
		return 0;
	}
}


int rx() {
       return ACQ400Receiver::instance()->event_loop(
                       TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_RECEIVER)));
}



int tx() {
	if (!G::max_tx_specified){
		G::max_tx = MAX_TX_INF;
	}
	if (G::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	Transmitter t(G::dev_ts);
	Receiver* r = Env::getenv("WRTD_LOCAL_RX_ACTION", 0)? Receiver::instance(): 0;
	return t.event_loop(TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER)), r);
}

int txi() {
	if (G::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	Transmitter t(DEV_CUR, 2*G::dns/1000);
	Receiver* r = Env::getenv("WRTD_LOCAL_RX_ACTION", 0)? Receiver::instance(): 0;
	return t.event_loop(TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER)), r);
}

int txq() {
	if (G::verbose){
		fprintf(stderr, "%s\n", PFN);
	}
	TSCaster& comms = TSCaster::factory(MultiCast::factory(G::group, G::port, MultiCast::MC_SENDER));
	comms.sendraw(TS_QUICK);
	return 0;
}


class Acq400Txa : public Txa {
protected:
	TS txa_validate_rel(unsigned sec, unsigned ns)
	{
		unsigned tai_sec = getvalue<unsigned>(DEV_TAI, "r") + 1; // round up to next second

		// @todo .. added 1 twice ..
		return TS(tai_sec+1+sec, ns/G::ns_per_tick);
	}

	TS txa_validate_abs(unsigned sec, unsigned ns)
	{
		unsigned tai_sec = getvalue<unsigned>(DEV_TAI, "r");

		if (sec < tai_sec){
			fprintf(stderr, "ERROR: specified time @%u is less than current TAI @%u\n", sec, tai_sec);
			exit(1);
		}
		return TS(sec, ns/G::ns_per_tick);
	}
};

Txa& Txa::factory()
{
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
	}else{
		return sleep_if_notenabled("WRTD_RX") || rx();
	}
}

