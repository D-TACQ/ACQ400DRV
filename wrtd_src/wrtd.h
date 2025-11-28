/* wrtd.h : White Rabbit Time Distribution                  	 	     */
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
 * wrtd.h : White Rabbit Time Distribution
 *
 *  Created on: 27 Nov 2025
 *      Author: cph
 *
 */
#ifndef WRTD_H_
#define WRTD_H_

#include <cstdio>
#include "Multicast.h"
#include "wrtd_message.h"

namespace wrtd_defaults {
    constexpr const char* DEV_TS = "/dev/acq400.0.wr_ts";	// blocking device returns TIMESTAMP
    constexpr const char* DEV_CUR = "/dev/acq400.0.wr_cur";	// noblock device returns current TAI in 7:ticks format
    constexpr const char* DEV_TAI = "/dev/acq400.0.wr_tai";	// noblock device returns current TAI in s
    constexpr const char* DEV_TRG0 = "/dev/acq400.0.wr_trg0"; // write trigger0 definition here
    constexpr const char* DEV_TRG1 = "/dev/acq400.0.wr_trg1"; // write trigger1 definition here
    constexpr int LOCAL_CLKDIV_AUTO = 77777777;
}

namespace wrtd_ns {
    inline unsigned delta_ns = 40 * wrtd_TS_defaults::M1;       // delta nsec
    inline unsigned local_clkdiv;                               // Site 1 clock divider, set at star
    inline unsigned local_clkoffset;                            // local_clk_offset eg 2 x 50nsec for ACQ42x
    inline bool max_tx_specified = false;                       // TRUE if UI changed max_tx
    inline int rt_prio = 0;
    inline int delay01;                                         // tr==2? trg0 at time t, trg1 at t+delay01
    inline const char* dev_ts = wrtd_defaults::DEV_TS;
    inline unsigned site;
    inline int ons;                                             // on next second
    inline MC_FACTORY* mc_factory;
    inline unsigned REPORT_THRESHOLD = delta_ns/4;
}

class ACQ400Receiver: public Receiver {
protected:
	const int ntriggers;
	const long dms;
	FILE **fp_trg;
	FILE *fp_cur;
	char* report_fname;
	char* report;
	ACQ400Receiver(int _ntriggers = 2);
	virtual void onAction(TS& ts, TS& ts_adj) override;
        void deferredAction(TS& ts, int nrx = 0);
public:
	virtual ~ACQ400Receiver(); 
	virtual void action(TS& ts, int nrx = 0);
	friend class Receiver;
};


class TIGA_Receiver: public ACQ400Receiver {
protected:
	TIGA_Receiver();
	friend class Receiver;
};

class Transmitter {
    FILE* fp;
    const int sleep_us;
public:
    Transmitter(const char* dev, int _sleep_us = 0);
    virtual ~Transmitter();
    int event_loop(TSCaster& comms, Receiver* local_rx);
};


class Acq400Txa : public Txa {
	static int WRTD_TXA_AGGRESSIVE;
protected:
	TS txa_validate_rel(unsigned sec, unsigned ns);
	TS txa_validate_abs(unsigned sec, unsigned ns);
};

bool is_tiga();
const char* ui_get_cmd_name(const char* path);
TS _adjust_ts(TS& ts0);
TS adjust_ts(TS& ts0);
void _write_trg(FILE* fp, TS ts);
void get_local_env();
int sleep_if_notenabled(const char* key);
int rx();
int tx();
int txi();
int txq();

#endif /* WRTD_H_ */
