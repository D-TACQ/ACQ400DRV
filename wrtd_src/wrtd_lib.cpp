/* wrtd_lib.cpp : White Rabbit Time Distribution Helper Library 	     */
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
 * wrtd_lib.cpp : White Rabbit Time Distribution Library
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */
#include "wrtd.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glob.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "Env.h"
#include "File.h"
#include "Knob.h"

using namespace wrtd_defaults;

using namespace wrtd_ns;


bool is_tiga()
{
	Knob k(0, "wr_tai_trg_s1");
	return k.exists();
}

TS _adjust_ts(TS& ts0)
{
	int rem = ts0.ticks() % wrtd_ns::local_clkdiv;
	unsigned ticks = ts0.ticks();


	if (rem != 0){
		ticks += wrtd_ns::local_clkdiv - rem;
	}
	if (ticks > wrtd_ns::local_clkoffset){
		ticks -= wrtd_ns::local_clkoffset;
	}

	if (wrtd_message_ns::verbose > 1) fprintf(stderr, "adjust_ts: ts0 %u div %u rem %u off %u adj %u\n",
			ts0.ticks(), wrtd_ns::local_clkdiv, rem, wrtd_ns::local_clkoffset, ticks);

	return TS(ts0.secs(), ticks);
}
TS adjust_ts(TS& ts0)
{
	if (ts0 != TS::ts_quick && (wrtd_ns::local_clkdiv > 1 || wrtd_ns::local_clkoffset != 0)){
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

ACQ400Receiver::ACQ400Receiver(int _ntriggers) :  ntriggers(_ntriggers), dms(wrtd_ns::delta_ns/M1), report_fname(new char[80]), report(new char[256]) {
        sprintf(report_fname, "/etc/acq400/%d/WRTD_REPORT", wrtd_ns::site);
        fp_trg = new FILE* [ntriggers];
        memset(fp_trg, 0, ntriggers*sizeof(FILE*));
        fp_trg[0] = fopen_safe(DEV_TRG0, "w");
        fp_trg[1] = fopen_safe(DEV_TRG1, "w");
        fp_cur = fopen_safe(DEV_CUR, "r");
}

void ACQ400Receiver::onAction(TS& ts, TS& ts_adj){
        if (wrtd_message_ns::verbose){
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
        }else if (wrtd_message_ns::trg < 2){
                _write_trg(fp_trg[wrtd_message_ns::trg], ts_adj);
        }else{							/* DOUBLE TAP */
                if (ts_adj != TS::ts_quick){
                        TS ts2 = ts + wrtd_ns::delay01;
                        _write_trg(fp_trg[0], ts_adj);
                        _write_trg(fp_trg[1], adjust_ts(ts2));
                }else{
                        _write_trg(fp_trg[0], TS_QUICK);
                        usleep(wrtd_ns::delay01*wrtd_TS_ns::ns_per_tick/1000);
                        _write_trg(fp_trg[1], TS_QUICK);
                }
        }

}

void ACQ400Receiver::deferredAction(TS& ts, int nrx)
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

ACQ400Receiver::~ACQ400Receiver() {
        fclose(fp_trg[0]);
        fclose(fp_trg[1]);
        fclose(fp_cur);
        delete [] report;
        delete [] report_fname;
}

void ACQ400Receiver::action(TS& ts, int nrx){
        if (wrtd_message_ns::verbose > 1) fprintf(stderr, "%s() TS:%s %08x\n", PFN, ts.toStr(), ts.raw);
        if (ts.is_abs_tai()){
                return deferredAction(ts, nrx);
        }
        TS ts_adj = adjust_ts(ts);
        onAction(ts, ts_adj);
        TS ts_cur;
        fread(&ts_cur.raw, sizeof(unsigned), 1, fp_cur);

        long dt = ts.diff(ts_cur);

        snprintf(report, 256, "Receiver:%d nrx:%u cur:%s ts:%s adj:%s diff:%ld %s\n",
                         wrtd_message_ns::trg, nrx, ts_cur.toStr(), ts.toStr(), ts_adj.toStr(), dt, dt<0? "ERROR": "OK");

        FILE *fp_report = fopen(report_fname, "w");
        fprintf(fp_report, report);
        fclose(fp_report);
        if (wrtd_message_ns::verbose > 1){
                fprintf(stderr, report);
        }


        if (dt < 0){
                fprintf(stderr, "wrtd rx ERROR missed ts by %ld msec\n", dt/M1);
        }else if (dt < (long)REPORT_THRESHOLD){
                fprintf(stderr, "wrtd rx WARNING threshold %ld msec under limit %ld\n", dt/M1, dms);
        }
}

TIGA_Receiver::TIGA_Receiver() : ACQ400Receiver(8) {
        wrtd_ns::local_clkdiv = wrtd_ns::local_clkoffset = 0;		// stub clock adjust
        if (wrtd_message_ns::verbose){
                fprintf(stderr, "TIGA_Receiver()\n");
        }

        glob_t globbuf;
        glob("/dev/acq400.0.wr_tiga_tt_s?", 0, NULL, &globbuf);
        for (unsigned ii = 0; ii < globbuf.gl_pathc; ++ii){
                const char* fn = globbuf.gl_pathv[ii];
                int site = fn[strlen(fn)-1]-'0';

                if (wrtd_message_ns::verbose){
                        fprintf(stderr, "TIGA_Receiver() fn:\"%s\" site:%d\n", fn, site);
                }

                if (site >= 1 && site <= 6){
                        fp_trg[site+1] = fopen_safe(fn, "w");		/* site1 => [2] */
                }
        }
        globfree(&globbuf);
}



Transmitter::Transmitter(const char* dev, int _sleep_us) :
		fp(::fopen_safe(dev)), sleep_us(_sleep_us)
	{
	}
Transmitter::~Transmitter(){
		fclose(fp);
	}

int Transmitter::event_loop(TSCaster& comms, Receiver* local_rx) {
        if (wrtd_message_ns::max_tx == 0){
                return 0;
        }
        TS ts;
        for (unsigned ntx = 0; fread(&ts.raw, sizeof(unsigned), 1, fp) == 1; ++ntx){
                TS ts_tx = wrtd_ns::ons? ts.next_second(): ts + wrtd_TS_ns::delta_ticks;
                ts_tx.mask = wrtd_message_ns::tx_mask;
                comms.sendto(ts_tx);
                local_rx->action(ts_tx, ntx);
                if (wrtd_message_ns::verbose > 1) fprintf(stderr, "sender:ntx:%u ts:%s ts_tx:%s\n", ntx, ts.toStr(), ts_tx.toStr());
                if (wrtd_message_ns::max_tx != MAX_TX_INF && ntx >= wrtd_message_ns::max_tx){
                        break;
                }else if (sleep_us){
                        usleep(sleep_us);
                }
        }
        return 0;
}

void get_local_env(void)
{
	wrtd_message_ns::verbose = Env::getenv("WRTD_VERBOSE", 0);
	wrtd_ns::site = Env::getenv("SITE", 11);
	char envname[80];
	sprintf(envname, "/dev/shm/wr%d.sh", wrtd_ns::site);
	get_local_env(envname, wrtd_message_ns::verbose);

	int use_wrs = Env::getenv("WRTD_USE_WRS", 0);
	wrtd_ns::mc_factory = use_wrs? WrsCast::factory: MultiCast::factory;
}

int sleep_if_notenabled(const char* key)
{
	if (Env::getenv(key, 0) == 0){
		if (wrtd_message_ns::verbose){
			fprintf(stderr, "%s==0, sleep(9999)\n",key);
		}
		sleep(9999);
		return 1;
	}else{
		return 0;
	}
}


TS Acq400Txa::txa_validate_rel(unsigned sec, unsigned ns) {
        unsigned tai_sec = getvalue<unsigned>(DEV_TAI, "r") + 1; // round up to next second

        // .. default is add one to ensure up rounding, then add another 1 to ensure we have enough slack
        return TS(tai_sec+(WRTD_TXA_AGGRESSIVE==0)+sec, ns/wrtd_TS_ns::ns_per_tick);
}

TS Acq400Txa::txa_validate_abs(unsigned sec, unsigned ns) {
        unsigned tai_sec = getvalue<unsigned>(DEV_TAI, "r");

        if (sec < tai_sec){
                fprintf(stderr, "ERROR: specified time @%u is less than current TAI @%u\n", sec, tai_sec);
                exit(1);
        }
        return TS(sec, ns/wrtd_TS_ns::ns_per_tick);
}

int Acq400Txa::WRTD_TXA_AGGRESSIVE = Env::getenv("WRTD_TXA_AGGRESSIVE", 0);


