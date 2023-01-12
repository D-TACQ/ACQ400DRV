/* ------------------------------------------------------------------------- *
 * dawg.c++  	Digital AWG
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2013 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 4 Nov 2013  
 *    Author: pgm                                                         
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
 * AWG def a series of entries
 * delta-time ch01 ch02
  * delta-time begins 0, must be monotonic, increasing. time in msec.
  * ch01, ch02 : 0xHHHHHHHH numbers representing state of ch0x at time dt
  * dt is the START time, the duration of the state is defined by the start
  * time of the next state, Time 0 is instantaneous (no delay).

 eg
  0    0x0 0x0		# OPEN all channels
  100  0x3 0x0   	# equiv: close CH01.01,CH01.02
  200  0x6 0x0	 	# OPEN previous channels. close CH01.02,CH01,03
  300  			# OPEN previous channels

 * run in debug mode, do two cycles to see:
 * dawg -r 2 -D  -s /usr/local/CARE/dawg-test-pat-running-bit
 *
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "popt.h"
#include <list>
#include <time.h>


using namespace std;

typedef unsigned int u32;

#include "File.h"
#include "Knob.h"
#include "ScratchPad.h"

#define MAXCHAN	2
#define MAXSW	20

#define ALL_CLOSED_MASK  (((1<<MAXSW)<<1) - 1)

enum StateMode {
	UNTIL_STATE,
	STATE_WHILE,
};

namespace UI {
	const char* fname = "nowhere";
	bool dry_run;
	int verbose;
	int repeat_count = 1;
	int timescaler = 1;
	bool print_quit;
	int site = 5;
	int sched_fifo = 0;
	enum StateMode mode = UNTIL_STATE;
};

#define KNOBS	"./TEST"

#include <stdio.h>


class DawgEntry {
	static int last_serial;
	int serial;

	char* def;

protected:
	u32 chx[MAXCHAN];	/* should be const, but hard to init */


	DawgEntry(const char* _def, const u32 _abstime,
		const u32 _ch01, const u32 _ch02, DawgEntry* _prev);


public:
	static char* the_knob;
	const u32 abstime;
	DawgEntry* prev;

	void print();

	static DawgEntry *create(const char* _def, DawgEntry* _prev);
	static DawgEntry *create(const char* _def, const DawgEntry* clone, DawgEntry* _prev);
	static DawgEntry *createAllOpen(DawgEntry *prev);

	virtual void exec();
	/* never create an instance of DawgEntry */
	virtual bool is_usable() = 0;
};

class ScratchpadReportingDawgEntry: public DawgEntry {
	virtual void _exec();
public:
	ScratchpadReportingDawgEntry(const char* _def, const u32 _abstime,
			const u32 _ch01, const u32 _ch02, DawgEntry* _prev):
	DawgEntry(_def, _abstime, _ch01, _ch02, _prev)
	{}

	virtual void exec();
	virtual bool is_usable() { return true; };
};



int DawgEntry::last_serial;
char* DawgEntry::the_knob;

DawgEntry::DawgEntry(const char* _def, const u32 _abstime,
		const u32 _ch01, const u32 _ch02, DawgEntry* _prev) :
	abstime(_abstime), prev(_prev)
{
	def = new char[strlen(_def)+1];
	strcpy(def, _def);
	serial = last_serial++;
	chx[0] = _ch01;
	chx[1] = _ch02;

	if (!the_knob){
		the_knob = new char[128];
		snprintf(the_knob, 128, "/dev/acq400.%d.knobs/dac_mux_master", UI::site);
	}
}



void DawgEntry::exec()
{
	if (UI::verbose > 1){
		print();
		if (UI::dry_run){
			return;
		}
	}

	Knob knob(the_knob);
	knob.setX(chx[1]<<6 | chx[0]);
}


void DawgEntry::print()
{
	printf("DawgEntry [%d]: abstime:%d \"%s\"  prev:[%d] ch01:%02x ch02:%02x\n",
			serial, abstime, def, prev? prev->serial: 0, chx[0], chx[1]);
}

unsigned _strtoul(const char* nums)
/* pick hex or binary conversion */
{
	if (strstr(nums, "0x") != 0){
		return strtoul(nums, 0, 0);
	}else{
		return strtoul(nums, 0, 2);
	}
}

DawgEntry* DawgEntry::create(const char* _def, DawgEntry* _prev)
{
	static unsigned _abstime;
	const char* _ch01;
	const char* _ch02;
	int nscan;

	char f1[80];
	char f2[80];
	char f3[80];


	if (UI::verbose > 2){
		printf( "Scanning \"%s\"\n", _def);
	}

	if ((nscan = sscanf(_def, "%79s %79s %79s", f1, f2, f3)) < 3){
		printf( "ERROR in scan [%d] \"%s\"\n", nscan, _def);
		return 0;
	}

	char* tdef = UI::mode == UNTIL_STATE? f1: f3;
	if (tdef[0] == '+'){
		_abstime += atoi(tdef);
	}else{
		_abstime = atoi(tdef);

	}
	_ch01 = UI::mode == UNTIL_STATE? f2: f1;
	_ch02 = UI::mode == UNTIL_STATE? f3: f2;

	if (_prev == 0){
		if (_abstime != 0){
			printf( "ERROR: first entry abstime not zero\n");
			return 0;
		}
	}else{
		if (_abstime < _prev->abstime){
			printf( "ERROR: abstime not monotonic\n");
			return 0;
		}
	}

	DawgEntry* entry = new ScratchpadReportingDawgEntry(
				_def, _abstime,
				_strtoul(_ch01), _strtoul(_ch02), _prev);

	if (UI::verbose > 1){
		entry->print();
	}
	return entry;
}

DawgEntry* DawgEntry::create(const char* _def, const DawgEntry* clone, DawgEntry* _prev)
{
	DawgEntry* entry = new ScratchpadReportingDawgEntry(
		_def, clone->abstime, clone->chx[0], clone->chx[1], _prev);

	if (UI::verbose > 1){
		entry->print();
	}
	return entry;
}


DawgEntry *DawgEntry::createAllOpen(DawgEntry * prev)
{
	if (prev == 0){
		DawgEntry *allClosed = new ScratchpadReportingDawgEntry(
			"all closed", 0, ALL_CLOSED_MASK, ALL_CLOSED_MASK, 0);
		prev = allClosed;
	}
	DawgEntry *allOpen = new ScratchpadReportingDawgEntry(
		"all open",   0, 0, 0, prev);
	return allOpen;
}

void ScratchpadReportingDawgEntry::_exec()
{
	Scratchpad& sp(Scratchpad::instance());

	sp.set(Scratchpad::SP_MUX_STATUS, Scratchpad::SP_MUX_STATUS_BUSY);

	DawgEntry::exec();

	sp.set(Scratchpad::SP_MUX_CH01, chx[0]);
	sp.set(Scratchpad::SP_MUX_CH02, chx[1]);
	sp.set(Scratchpad::SP_MUX_STATUS, Scratchpad::SP_MUX_STATUS_DONE);
}

void ScratchpadReportingDawgEntry::exec()
{
	if (UI::dry_run){
		DawgEntry::exec();
		return;
	}else{
		_exec();
	}
}

#include <sched.h>

void set_hi_priority() {
	struct sched_param sp = {};
	sp.sched_priority = UI::sched_fifo;
	int rc = sched_setscheduler(0, SCHED_FIFO, &sp);
	if (rc != 0){
		perror("sched_setscheduler()");
	}else{
		printf( "SCHED_FIFO set\n");
	}
}

bool please_stop;		/* could be set by signal */

void waitUntil(unsigned deadline)
{
	static unsigned deadline0;
	unsigned ddt;

	if (deadline > deadline0){
		ddt = deadline - deadline0;
	}else{
		ddt = deadline;			// assume relative timing
	}

	if (UI::verbose > 1){
		printf("waitUntil %d actual delay:%d\n", deadline, ddt);
	}


	struct timespec ts = {};
	struct timespec rem;

	if (ddt >= 1000){
		ts.tv_sec = ddt/1000;
		ddt -= 1000*ts.tv_sec;
	}
	ts.tv_nsec = ddt*1000000;
	if (nanosleep(&ts, &rem)){
		nanosleep(&rem, &rem);	/* we had a signal .. continue */
	}

	deadline0 = deadline;
}

typedef list<DawgEntry*>::iterator DEI;

class SequenceExecutor {
public:
	virtual void exec(DEI& it) = 0;
};

class UntilStateSequenceExecutor: public SequenceExecutor {
public:
	virtual void exec(DEI& it) {
		waitUntil((*it)->abstime * UI::timescaler);
		(*it)->exec();
	}
};
class StateWhileSequenceExecutor: public SequenceExecutor {
public:
	virtual void exec(DEI& it) {
		(*it)->exec();
		waitUntil((*it)->abstime * UI::timescaler);
	}
};

class Sequence {
	list <DawgEntry*> instructions;
	DEI loop_first;
	DawgEntry* last_in_loop;
	DawgEntry* loop_first2;		/* use this entry first second time round */
	SequenceExecutor* executor;

	void getIteratorAt(DEI& it, DawgEntry *entry) {
		for (it = instructions.begin(); it != instructions.end(); ++it){
			if (*it == entry){
				return;
			}
		}
		assert(false);
	}

public:
	Sequence() : executor(new UntilStateSequenceExecutor) {}
	void build();
	void print();
	void run();
};



void Sequence::build(void)
{
	if (UI::verbose){
		printf( "\n\nbuild ----------------\n");
	}
	File file(UI::fname, "r");		/* self closing */
	FILE* fp = file();

	char* seq_line = new char[256];
	char* first_entry_def = 0;
	DawgEntry *prev = 0;
	DawgEntry *now;
	bool is_first_entry = true;
	char style[80];

	instructions.push_back(prev = DawgEntry::createAllOpen(prev));

	for (int nl = 0; fgets(seq_line, 255, fp); ++nl){
		if (strlen(seq_line) < 3){
			continue;
		}else if (seq_line[0] == '#'){
			continue;
		}else if (sscanf(seq_line, "style=%79s", style) == 1){
			if (strcmp(style, "STATE_WHILE") == 0){
				UI::mode = STATE_WHILE;
				executor = new StateWhileSequenceExecutor;
				printf( "set style STATE_WHILE\n");
			}else if (strcmp(style, "UNTIL_STATE") == 0){
				UI::mode = UNTIL_STATE;
				executor = new UntilStateSequenceExecutor;
				printf( "set style UNTIL_STATE\n");
			}else{
				printf( "ERROR: style choice STATE_WHILE or UNTIL_STATE\n");
				exit(1);
			}
		}else{
			now = DawgEntry::create(chomp(seq_line), prev);

			if (now){
				instructions.push_back(now);
				if (is_first_entry){
					first_entry_def = seq_line;
					seq_line = new char[256];
					getIteratorAt(loop_first, now);
					is_first_entry = false;
				}
				prev = now;
			}else{
				printf(
					"ERROR: parse failed at line %d\n", nl);
				exit(1);
			}
		}
	}

	last_in_loop = prev;
	loop_first2 = DawgEntry::create(strcat(first_entry_def, " loop_first2"), *loop_first, prev);

	instructions.push_back(DawgEntry::createAllOpen(prev));

	delete [] seq_line;
	if (first_entry_def) delete [] first_entry_def;
}


void Sequence::print(void)
{
	DEI it;

	for (it = instructions.begin(); it != instructions.end(); ++it){
		(*it)->print();
	}
}


void Sequence::run(void)
{
	if (UI::verbose){
		printf( "\n\nrun ----------------\n");
	}

	DEI it = instructions.begin();

	(*it)->exec();
	DEI start = ++it;


	for (int iter = 0; ++iter <= UI::repeat_count || UI::repeat_count == -1;
			(*start) = loop_first2){
		if (UI::verbose){
			printf( "\n\nloop: %d\n", iter);
		}
		for (it = start; it != instructions.end(); ++it){
			executor->exec(it);
			if ((*it) == last_in_loop){
				++it;
				break;
			}
		}

		if (please_stop){
			break;
		}
	}

	if (UI::verbose){
		printf( "finish up\n");
	}
	for (; it != instructions.end(); ++it){
		executor->exec(it);
	}
}

struct poptOption opt_table[] = {
	{ "dry-run", 'D', POPT_ARG_NONE, 0, 'D',
			"go through the motions, no action" 	},
	{ "verbose", 'v', POPT_ARG_INT, &UI::verbose, 0,
			"verbose" 				},
	{ "sequence", 's', POPT_ARG_STRING, &UI::fname, 0,
			"sequence definition file" 		},
	{ "print",    'p', POPT_ARG_NONE, 0, 'p'		},
	{ "repeat",   'r', POPT_ARG_INT,  &UI::repeat_count, 0,
			"repeat count [1] -1=FOREVER"			},
	{ "slow",       0, POPT_ARG_INT,  &UI::timescaler, 0,
			"slow down by factor N [1]"		},
	{ "site",        'S', POPT_ARG_INT, &UI::site, 0,
			"site of AO421"				},
	{ "realtime", 'R',  POPT_ARG_INT, &UI::sched_fifo, 'R',
			"set real time priority" 		},
	POPT_AUTOHELP
	POPT_TABLEEND
};
void ui(int argc, const char* argv[])
{
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);

	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) > 0 ){
		switch(rc){
		case 'p':
			UI::print_quit = 1; break;
		case 'D':
			UI::dry_run = 1;
			UI::verbose += 2;
			printf( "Dry Run, verbose set: %d\n", UI::verbose);
			break;
		case 'R':
			set_hi_priority();
			break;
		default:
			;
		}
	}

	if (UI::verbose > 1){
		printf("command line was:");
		for (int ii = 0; ii < argc; ++ii){
			printf("%s ", argv[ii]);
		}
		printf("\n");
	}
}

int main(int argc, const char* argv[])
{
	ui(argc, argv);

	Sequence seq;
	seq.build();

	if (UI::print_quit){
		seq.print();
	}else{
		seq.run();
	}
	return 0;
}


