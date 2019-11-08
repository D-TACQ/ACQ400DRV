/* ------------------------------------------------------------------------- */
/* atd.cpp  D-TACQ ACQ400 comb memory for ATD result
 * field the event, locate in memory, extract and save
 * Project: ACQ420_FMC
 * Created: 17 August 2019  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2019 Peter Milne, D-TACQ Solutions Ltd         *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO
 * TODO
\* ------------------------------------------------------------------------- */



#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "popt.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <libgen.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>

#include <sched.h>

#include <syslog.h>

#define VERID	"B1000"

#define NCHAN	4

#include <semaphore.h>
#include <syslog.h>


#include "local.h"		/* chomp() hopefully, not a lot of other garbage */
#include "knobs.h"

#include "File.h"


#define WR_MEM_PA	     	0x80080000
#define WR_MEM_LEN	     	0x00040000

#define WR_FPGA_MAGIC_OFFSET 	0x00020410
#define WR_CODE_MAGIC_OFFSET 	0x00000080

#define WR_FPGA_MAGIC_LE    	0x44314143
#define WR_FPGA_MAGIC_BE     	0x43413144

#define WR_CODE_MAGIC_LE     	0x57525043
#define WR_CODE_MAGIC_BE     	0x43505257



#include "Env.h"

using namespace std;

#define MAC_PFX "00:21:54:33"


int get_mac(char* mac, int max_mac)
{
	string _mac;
	Env env("/tmp/u-boot_env");
	if (!env("eth2addr").empty()){
		cerr << "eth2addr found " << env("eth2addr") << endl;
		_mac = env("eth2addr");
	}else if (!env("ethaddr").empty()){
		string m0 = env("ethaddr");
		_mac = m0.replace(0, strlen(MAC_PFX), MAC_PFX, strlen(MAC_PFX));
	}else{
		fprintf(stderr, "ERROR no valid mac address\n");
	}
	strncpy(mac, _mac.c_str(), max_mac);
	return 0;
}

void* mmap_wr(void)
{
        int fd = open("/dev/mem", O_RDWR);
        unsigned *va = (unsigned*)mmap(0, WR_MEM_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, fd, WR_MEM_PA);

        if (va == MAP_FAILED){
                perror("MAP_FAILED");
                exit(1);
        }
	return va;
}

void wr_load(char* wrbase, const char* fname)
{
	FILE* fp = fopen(fname, "r");
	if (fp == 0){
		perror(fname);
		exit(1);
	}
	char* buf = new char[0x20000];
	int nr = fread(buf, 1, 0x20000, fp);
	fclose(fp);

	for (int ii = 0; ii < nr; ++ii){
		wrbase[ii] = buf[ii];
		sched_yield();
	}
}

void set_mac(char* wrbase)
{
	char* wrprams = wrbase + 0x1f000;
	char mac_buf[80];
	strcpy(mac_buf, "ac:ac:ac:ac:");
	get_mac(mac_buf+strlen(mac_buf), 80-strlen(mac_buf));
	printf("mac_buf:%s\n", mac_buf);

	char* token;
	char* rest = mac_buf;
	/* fit weird backwards load in LM32 (LM is BE, A9 is LE, but bus interface is supports LE longwords) */
	int ii = 0; int jj = 3;
	while((token = strtok_r(rest, ":", &rest))){
		wrprams[ii+jj] = strtoul(token, 0, 16);
		if (--jj < 0){
			ii += 4;
			jj = 3;
		}
	}
}

void reset(char* wrbase, bool reset)
{
	unsigned * wr_reset = (unsigned*)(wrbase+0x20400);
	*wr_reset = reset? 0x1deadbee: 0x0deadbee;
}

void set_hwid(char* wrbase)
{
	unsigned * wr_hwid  = (unsigned*)(wrbase+0x20414);
	*wr_hwid  = 0x41435134;				// tell ACQ400 we are done.
}

namespace G {
	int verbose;
	int save_cal;
	int is_wr_present;
};



int is_wr_present(char* wrbase)
{
	unsigned *wr_magic = (unsigned*)(wrbase + WR_FPGA_MAGIC_OFFSET);
	unsigned *wr_code  = (unsigned*)(wrbase + WR_CODE_MAGIC_OFFSET);
	union MAGIC {
		unsigned w;
		unsigned char b[4];
	} m;

	if ((m.w = *wr_magic) == WR_FPGA_MAGIC_LE){
		printf("is_wr_present() D-TACQ WR LOGIC detected 0x%08x = %c%c%c%c\n",
				WR_FPGA_MAGIC_OFFSET, m.b[0], m.b[1], m.b[2], m.b[3]);
		if ((m.w = *wr_code) == WR_CODE_MAGIC_LE){
			printf("is_wr_present() D-TACQ WR CODE detected 0x%08x = %c%c%c%c\n",
				WR_CODE_MAGIC_OFFSET, m.b[0], m.b[1], m.b[2], m.b[3]);
			return 0;
		}else{
			if (m.w == WR_CODE_MAGIC_BE){
				printf("is_wr_present() WR_CODE_MAGIC_BE detected, contact D-TACQ\n");
			}else{
				printf("is_wr_present() BAD CODE MAGIC 0x%08x %c%c%c%c\n",
						m.w, m.b[0], m.b[1], m.b[2], m.b[3]);
			}
		}
	}else if (m.w == WR_FPGA_MAGIC_LE){
		printf("is_wr_present() WR_FPGA_MAGIC_BE detected, contact D-TACQ\n");
	}else{
		printf("is_wr_present() BAD FPGA MAGIC 0x%08x %c%c%c%c\n",
				m.w, m.b[0], m.b[1], m.b[2], m.b[3]);
	}
	return -1;
}
int save_cal(char* wrbase)
{
	unsigned* sfp_deltaTx = (unsigned*)(wrbase + 0x1f020);
	unsigned* sfp_deltaRx = (unsigned*)(wrbase + 0x1f024);

	FILE *fp = fopen("/mnt/local/wr_cal", "w");
	fprintf(fp, "delays %u %u\n", *sfp_deltaTx, *sfp_deltaRx);
	fclose(fp);
	fprintf(stdout, "delays %u %u\n", *sfp_deltaTx, *sfp_deltaRx);
	return 0;
}
int restore_cal(char* wrbase)
{
	unsigned* sfp_deltaTx = (unsigned*)(wrbase + 0x1f020);
	unsigned* sfp_deltaRx = (unsigned*)(wrbase + 0x1f024);
	FILE *fp = fopen("/mnt/local/wr_cal", "r");
	if (fp){
		unsigned tx, rx;
		if (fscanf(fp, "delays %u %u", &tx, &rx) == 2){
			printf("setting delays %u %u\n", tx, rx);
			*sfp_deltaTx = tx;
			*sfp_deltaRx = rx;
			return 0;
		}
	}
	return -1;
}
struct poptOption opt_table[] = {
	{ "save_cal", 's', POPT_ARG_INT, &G::save_cal, 's', "save calibration and quit" },
	{ "is_wr_present", 'w', POPT_ARG_INT, &G::is_wr_present, 'w', "report if WRC present in systemi and quit" },
	{
	  "verbose", 'v', POPT_ARG_INT, &G::verbose, 0, "debug"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

const char* ui(int argc, const char** argv)
{
        poptContext opt_context =
                        poptGetContext(argv[0], argc, argv, opt_table, 0);
        int rc;
        while ((rc = poptGetNextOpt( opt_context )) >= 0 ){
                switch(rc){
                case 's':

                default:
                        ;
                }
        }
        return poptGetArg(opt_context);
}

#define DEF_IMAGE	"/mnt/local/wrc.le.bin"

int main(int argc, const char** argv)
{
	const char* image_file = ui(argc, argv);
	char* wrbase = (char*)mmap_wr();

	if (G::is_wr_present){
		return is_wr_present(wrbase);
	}
	if (G::save_cal){
		return save_cal(wrbase);
	}
	reset(wrbase, 1);
	if (image_file == 0) {
		fprintf(stderr, "try default wrc image file %s\n", DEF_IMAGE);
		image_file = DEF_IMAGE;
	}
	wr_load(wrbase, image_file);
	set_mac(wrbase);
	restore_cal(wrbase);
	reset(wrbase, 0);

	return 0;
}

