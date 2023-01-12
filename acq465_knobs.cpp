/*
 * acq465_knobs.cpp
 *
 *  Created on: 30 Sep 2021
 *      Author: pgm
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vector>
#include <libgen.h>
#include <unistd.h>

#include <assert.h>
#include <math.h>
#include <algorithm>    // std::sort

#include <ctype.h>
#include <vector>

using namespace std;

#include "popt.h"
#include "acq465_ioctl.h"

int G_verbose = 0;
int G_terse = 0;

#define PRSEP	 putchar(G_terse? ' ': '\n')

void die(const char *fmt)
{
	fprintf(stderr, "ERROR: %s", fmt);
	exit(1);
}

struct Command;

#define CAL_DISABLE 	0xffffffff

class Ad7134 {
public:
	unsigned char* regs;

	enum REGS {
		DIGITAL_INTERFACE_CONFIG = 0x12,

		SCRATCHPAD = 0x0a,	// 1 byte

		DEVICE_STATUS = 0x15,	 // 1 byte
		ODR_VAL_INT_LSB = 0x16,	 // 3 bytes
		ODR_VAL_FLT_LSB = 0x19,  // 4 bytes

		CHAN_DIG_FILTER_SEL = 0x1e, // 1 byte

		CH0_GAIN_LSB 	= 0x27,  // 3 bytes
		CH0_OFFSET_LSB 	= 0x2a,  // 3 bytes
		CH1_GAIN_LSB 	= 0x2d,  // 3 bytes
		CH1_OFFSET_LSB 	= 0x30,  // 3 bytes
		CH2_GAIN_LSB 	= 0x33,  // 3 bytes
		CH2_OFFSET_LSB 	= 0x36,  // 3 bytes
		CH3_GAIN_LSB 	= 0x39,  // 3 bytes
		CH3_OFFSET_LSB 	= 0x3c,  // 3 bytes
		MCLK_COUNTER    = 0x3f,  // 1 byte
		DIG_FILTER_OFUF = 0x40,  // 1 byte
		DIG_FILTER_SETTLED = 0x41, // 1 byte
		INTERNAL_ERROR  = 0x42,  // 1 byte
		AIN_OR_ERROR    = 0x48   // 1 byte
	};

	static const unsigned char ch_gain_lut[4];
	static const unsigned char ch_offset_lut[4];
	Ad7134(unsigned char* _regs): regs(_regs)
	{}

	void offset(unsigned ch, int offset) {
		assert(ch >=0 && ch < 4);

		if ((unsigned)offset == CAL_DISABLE){
			regs[ch_offset_lut[ch]+0] =
			regs[ch_offset_lut[ch]+1] =
			regs[ch_offset_lut[ch]+2] = 0;
		}else{
			regs[ch_offset_lut[ch]+0] =   offset	 & 0x00ff;		// LSB
			regs[ch_offset_lut[ch]+1] =  (offset>>=8)& 0x00ff;              // MID
			regs[ch_offset_lut[ch]+2] = ((offset>>=8)& 0x007f) | 1<<7;      // MSB+CAL_EN
		}
	}
	int offset(unsigned ch) {
		assert(ch >=0 && ch < 4);
		int offset = 0;
		offset |= regs[ch_offset_lut[ch]+0];
		offset |= regs[ch_offset_lut[ch]+1]<<8;
		offset |= (regs[ch_offset_lut[ch]+2]&0x7f)<<16;
		if (offset&0x400000){
			offset = -offset;
		}
		return offset;
	}
	void gain(unsigned ch, int gain) {
		assert(ch >=0 && ch < 4);

		if ((unsigned)gain == CAL_DISABLE){
			regs[ch_gain_lut[ch]+0] =
			regs[ch_gain_lut[ch]+1] =
			regs[ch_gain_lut[ch]+2] = 0;
		}else{
			regs[ch_gain_lut[ch]+0] =   gain     & 0x00ff;          // LSB
			regs[ch_gain_lut[ch]+1] =  (gain>>=8)& 0x00ff;          // MID
			regs[ch_gain_lut[ch]+2] = ((gain>>=8)& 0x000f) | 1<<4;  // MSB+CAL_EN
		}
	}
	int gain(unsigned ch) {
		assert(ch >=0 && ch < 4);
		int gain = 0;
		gain |= regs[ch_gain_lut[ch]+0];
		gain |= regs[ch_gain_lut[ch]+1]<<8;
		gain |= (regs[ch_gain_lut[ch]+2]&0x0f)<<16;
		return gain;
	}

	void ODR(double dr){
		double intpart;
		double frac = modf(dr, &intpart);

		unsigned val_int = (unsigned)intpart;
		unsigned val_flt = (unsigned)(frac*0x100000000ULL);

		regs[ODR_VAL_INT_LSB+0] = val_int	& 0x00ff;
		regs[ODR_VAL_INT_LSB+1] = (val_int>>=8)	& 0x00ff;
		regs[ODR_VAL_INT_LSB+2] = (val_int>>=8) & 0x00ff;

		regs[ODR_VAL_FLT_LSB+0] = val_flt	& 0x00ff;
		regs[ODR_VAL_FLT_LSB+1] = (val_flt>>=8)	& 0x00ff;
		regs[ODR_VAL_FLT_LSB+2] = (val_flt>>=8)	& 0x00ff;
		regs[ODR_VAL_FLT_LSB+3] = (val_flt>>=8)	& 0x00ff;
	}
};

const unsigned char Ad7134::ch_gain_lut[4] = {
	CH0_GAIN_LSB, CH1_GAIN_LSB, CH2_GAIN_LSB, CH3_GAIN_LSB
};
const unsigned char Ad7134::ch_offset_lut[4] = {
	CH0_OFFSET_LSB, CH1_OFFSET_LSB, CH2_OFFSET_LSB, CH3_OFFSET_LSB
};
class Acq465ELF {
	unsigned char* clibuf;

	static vector<Command*> commands;

	void init_commands();

public:
	FILE* fp;
	int site;
	unsigned lcs;				// local chip select
	const char* chips;

	Acq465ELF(int _site, const char* _chips) : site(_site), chips(_chips)
	{
		char fname[80];
		snprintf(fname, 80, "/dev/acq465.%d", site);
		fp = fopen(fname, "r+");
		if (!fp){
			perror(fname);
			exit(1);
		}
		clibuf = (unsigned char*)mmap(NULL, TOTAL_SPI_BUFFER_LEN,
				PROT_READ|PROT_WRITE, MAP_SHARED, fileno(fp), 0);
		if (clibuf == MAP_FAILED){
			die("MAP_FAILED");
		}
		init_commands();
	}
	virtual ~Acq465ELF()
	{
		munmap(clibuf, TOTAL_SPI_BUFFER_LEN);
		fclose(fp);
	}

	int operator() (int argc, const char** argv);

	int invalidate(unsigned chip) {
		return ioctl(fileno(fp), ACQ465_CACHE_INVALIDATE, chip);
	}
	int invalidate(const char chip) {
			return ioctl(fileno(fp), ACQ465_CACHE_INVALIDATE, chip-'A');
	}
	int invalidate() {
		return invalidate(lcs);
	}
	int dig_if_reset(unsigned doit){
		return ioctl(fileno(fp), ACQ465_DIG_IF_RESET, doit);
	}
	int flush(unsigned chip) {
		return ioctl(fileno(fp), ACQ465_CACHE_FLUSH, chip);
	}
	int flush(const char chip) {
		return ioctl(fileno(fp), ACQ465_CACHE_FLUSH, chip-'A');
	}
	int flush() {
		unsigned chx;
		int rc;
		for (chx = 0; chx < NCHIPS; ++chx){
			rc = flush(chx);
			if (rc != 0){
				return rc;
			}
		}
		return 0;
	}
	int reset(unsigned chx) {
		return ioctl(fileno(fp), ACQ465_RESET, chx);
	}
	int reset() {
		return reset(lcs);
	}

	unsigned char* cache() {
		return clibuf + (site-1)*MODULE_SPI_BUFFER_LEN + lcs*REGS_LEN;
	}
	unsigned char* cache(const char chip) {
			return clibuf + (site-1)*MODULE_SPI_BUFFER_LEN + (chip-'A')*REGS_LEN;
	}
	unsigned char* cache(int chip) {
		return clibuf + (site-1)*MODULE_SPI_BUFFER_LEN + chip*REGS_LEN;
	}

	friend class HelpCommand;
	friend class MakeLinksCommand;

	static const unsigned char cmap[8][4];

	Ad7134* chip()
	{
		return new Ad7134(cache());
	}
	Ad7134* chip(int channel, unsigned& chx){
		assert(channel >=1 && channel <= 32);
		for (int chip = 0; chip < 8; ++chip){
			for (int _chx = 0; _chx < 4; ++_chx){
				if (channel == cmap[chip][_chx]){
					chx = _chx;
					return new Ad7134(cache(chip));
				}
			}
		}
		assert(0);
		return 0;
	}
};

const unsigned char Acq465ELF::cmap[8][4] = {
	/*A*/ { 16, 15, 17, 18 },
	/*B*/ { 14, 13, 19, 20 },
	/*C*/ { 12, 11, 21, 22 },
	/*D*/ { 10,  9, 23, 24 },
	/*E*/ {  8,  7, 25, 26 },
	/*F*/ {  6,  5, 27, 28 },
	/*G*/ {  4,  3, 29, 30 },
	/*H*/ {  2,  1, 31, 32 }
};


enum COMMAND {
	COMMAND_ERR = -1,
	COMMAND_OK = 0,
	COMMAND_FLUSH = 1,
	COMMAND_QUIT = 2,
};
struct Command {
	const char* cmd;
	const char* args_help;
	bool isChannelCommand;

	char* _help;
	virtual int operator() (class Acq465ELF& module, int argc, const char** argv) = 0;
	/* return > 0 if flush recommended */

	Command(const char* _cmd, const char* _args_help = "") :
		cmd(_cmd), args_help(_args_help), isChannelCommand(false) {
		_help = new char[25 + strlen(args_help) + 2];
		sprintf(_help, "%-25s %s", cmd, args_help);
	}

	const char* help() {
		return _help;
	}
};



typedef vector<Command*>::iterator VCI;

class HelpCommand: public Command {
public:
	HelpCommand() : Command("help") {}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		for (VCI it = module.commands.begin(); it != module.commands.end(); ++it){
			printf("%s\n", (*it)->help());
		}
		return COMMAND_QUIT;
	}
};


class MakeLinksCommand: public Command {
public:
	MakeLinksCommand() : Command("makeLinks") {}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		for (VCI it = module.commands.begin(); it != module.commands.end(); ++it){
			if ((*it)->cmd != cmd){
				printf("ln -s %s acq465_%s\n", "/usr/local/bin/acq465_knobs", (*it)->cmd);
			}
		}
		return COMMAND_QUIT;
	}
};
class ResetCommand: public Command {
public:
	ResetCommand() : Command("reset", "reset") {}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		return module.reset();
	}
};

class ReadAllCommand: public Command {
public:
	ReadAllCommand() : Command("readall", "readall") {}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		int rc = module.invalidate();
		printf("%d ", rc);
		return rc;
	}
};
class FlushCommand: public Command {
public:
	FlushCommand() : Command("flush", "flush :: flushes all dirty data") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		return module.flush();
	}
};


class GainCommand: public Command {
public:
	GainCommand() :
		Command("gain", "CH [VALUE]") { isChannelCommand = true; }
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		if (argc < 2) die(help());
		unsigned ch = strtoul(argv[1], 0, 10);

		if (ch < 1 || ch > 32){
			return -1;
		}

		unsigned chx;
		Ad7134 *ad7134 = module.chip(ch, chx);

		if (argc == 2){
			printf("%02d=%d\n", ch, ad7134->gain(chx));
		}else{
			ad7134->gain(chx, strtol(argv[2], 0, 0));
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}
};

class OffsetCommand: public Command {
public:
	OffsetCommand() :
		Command("offset", "CH [VALUE]") { isChannelCommand = true; }
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		if (argc < 2) die(help());
		unsigned ch = strtoul(argv[1], 0, 10);

		if (ch < 1 || ch > 32){
			return -1;
		}

		unsigned chx;
		Ad7134 *ad7134 = module.chip(ch, chx);

		//printf("ch:%d chx set:%u\n", ch,chx);

		if (argc == 2){
			printf("%02d=%d\n", ch, ad7134->offset(chx));
		}else{
			ad7134->offset(chx, strtol(argv[2], 0, 0));
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}
};

class DclkFreqCommand: public Command {
public:
	DclkFreqCommand() :
		Command("dclkFreq", "[sel 0x0 .. 0xf ]") {}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		unsigned char reg = module.cache()[Ad7134::DIGITAL_INTERFACE_CONFIG];

		if (argc == 1){
			printf("%x\n", reg&0x0f);
		}else{
			unsigned sel = strtoul(argv[1], 0, 16);
			if (sel > 0xf){
				return -1;
			}

			reg &=~ 0x0f;
			reg |= sel;

			module.cache()[Ad7134::DIGITAL_INTERFACE_CONFIG] = reg;
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}
};

class WordSizeCommand: public Command {
public:
	WordSizeCommand() :
		Command("wordsize", "[16|32 [CRC]]") {}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		unsigned char reg = module.cache()[Ad7134::DIGITAL_INTERFACE_CONFIG];

		if (argc == 1){
			printf("%x\n", reg>>4 & 0x3);
		}else{
			unsigned ws = strtoul(argv[1], 0, 16);
			unsigned sel = 0;

			if (ws == 32){
				sel = 2;
			}
			if (argc > 2 && strcmp(argv[2], "CRC") == 0){
				sel |= 1;
			}

			reg &=~ 0x03 <<4;
			reg |= sel << 4;

			module.cache()[Ad7134::DIGITAL_INTERFACE_CONFIG] = reg;
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}
};

class FilterCommand: public Command {
public:
	FilterCommand() :
		Command("filter", "[0,1,2,3] : all WB, S6, S3, S3R all chans or 0x02x for individual chan select")
	{}

	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		unsigned char reg = module.cache()[Ad7134::CHAN_DIG_FILTER_SEL];

		if (argc == 1){
			bool all_for_one = true;
			for (int shl = 2; shl <= 6; shl += 2){
				if ((reg&3) != (reg&(3<<shl))){
					all_for_one = false;
					break;
				}
			}
			if (all_for_one){
				printf("%d", reg & 0x3); PRSEP;
			}else{
				printf("0x%02x", reg); 	 PRSEP;
			}
		}else if (strlen(argv[1]) == 1 && isdigit(argv[1][0])){
			unsigned sel = argv[1][0] - '0';
			reg = sel | sel<<2 | sel<<4 | sel<<6;
			module.cache()[Ad7134::CHAN_DIG_FILTER_SEL] = reg;
			return COMMAND_FLUSH;
		}else{
			reg = strtoul(argv[1], 0, 16);
			module.cache()[Ad7134::CHAN_DIG_FILTER_SEL] = reg;
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}

};

#define MCLK 	24000

class ODR_Command: public Command {
public:
	ODR_Command():
		Command("ODR", "[n.nn kHz]") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		if (argc == 1){
			printf("%.4f kHz\n", 3.1415926);
		}else{
			double odr = strtod(argv[1], 0);
			double dr = MCLK/odr;
			module.chip()->ODR(dr);
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}
};

class ScratchpadTest: public Command {
public:
	ScratchpadTest():
		Command("scratchpad", "[N]") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		int nloops = 1;
		if (argc > 1){
			nloops = atoi(argv[1]);
		}

		for (int loop = 0; loop++ < nloops; ){
			fprintf(stderr, "scratchpad:loop %d/%d\n", loop, nloops);

			for (int test = 0; test < 256; ++test){
				for (const char* pc = module.chips; *pc; ++pc){
					module.cache(*pc)[Ad7134::SCRATCHPAD] = (test+(pc-module.chips))&0x0ff;
					module.flush(*pc);
					module.cache(*pc)[Ad7134::SCRATCHPAD] = ~(test+(pc-module.chips))&0x0ff;
				}

				for (const char* pc = module.chips; *pc; ++pc){
					module.invalidate(*pc);
					if (module.cache(*pc)[Ad7134::SCRATCHPAD] != ((test+(pc-module.chips))&0x0ff)){
						fprintf(stderr, "scratchpad %c.%d.%d fail %02x != %0x2\n",
								*pc, loop, test,
								module.cache(*pc)[Ad7134::SCRATCHPAD],
								(test+(pc-module.chips))&0x0ff);
					}
				}
			}
		}

		return COMMAND_OK;
	}
};

class MCLK_Monitor: public Command {
public:
	MCLK_Monitor():
		Command("mclkmon", "[seconds]") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		int sec = 1;

		if (argc > 1){
			sec = atoi(argv[1]);
		}

		struct MCM mcm;
		mcm.lcs = module.lcs;
		mcm.sec = sec;
		mcm.count = 0;

		int rc = ioctl(fileno(module.fp), ACQ465_MCLK_MONITOR, &mcm);
		if (rc != 0){
			fprintf(stderr, "%s ERROR ioctl fail %d\n", __FUNCTION__, rc);
			exit(rc);
		}
		printf("MCLK: chip=%c count=%u freq=%.3e\n", module.lcs+'A', mcm.count, (double)mcm.count*12000/sec);
		return COMMAND_OK;
	}
};

class SetReg: public Command {
public:
	SetReg() :
		Command("reg", "REG [VALUE]") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		if (argc < 2) die(help());
		unsigned reg = strtoul(argv[1], 0, 0);
		unsigned regval;
		bool setter = false;

		int pair = sscanf(argv[1], "%x=%x", &reg, &regval);
		switch(pair){
		case 2:
			setter = true;
		case 1:
			break;
		case 0:
			reg = strtoul(argv[1], 0, 0);
			break;
		default:
			assert(0);
		}
		if (reg >= REGS_LEN){
			return -1;
		}
		if (argc > 2){
			regval = strtoul(argv[2], 0, 0);
			setter = true;
		}
		if (!setter){
			regval = module.cache()[reg];
			printf("%02x=%04x", reg, regval); PRSEP;
		}else{
			if (regval >= 256){
				return -2;
			}
			if (G_verbose){
				printf("%02x:=%04x", reg, regval); PRSEP;
			}
			module.cache()[reg] = regval;
			return COMMAND_FLUSH;
		}
		return COMMAND_OK;
	}
};

class DeviceStatusQuery: public Command {
public:
	DeviceStatusQuery():
		Command("status", "read from cache readall first..") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		printf("%x ", module.cache()[Ad7134::DEVICE_STATUS]);
		return COMMAND_OK;
	}
};

class InternalErrorQuery: public Command {
public:
	InternalErrorQuery():
		Command("error", "read from cache readall first..") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		printf("%x ", module.cache()[Ad7134::INTERNAL_ERROR]);
		return COMMAND_OK;
	}
};

class FilterSettledQuery: public Command {
public:
	FilterSettledQuery():
		Command("filter_settled", "read from cache readall first..") { isChannelCommand = true; }

	/*
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		unsigned reg = module.cache()[Ad7134::AIN_OR_ERROR];
		printf("%d %d %d %d ", (reg&0x1)!=0, (reg&0x2)!=0, (reg&0x4)!=0, (reg&0x8)!=0);
		return COMMAND_OK;
	}
	*/
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		if (argc < 2) die(help());
		unsigned ch = strtoul(argv[1], 0, 0);

		if (ch < 1 || ch > 32){
			return -COMMAND_ERR;
		}

		unsigned chx;
		Ad7134 *ad7134 = module.chip(ch, chx);
		unsigned reg = ad7134->regs[Ad7134::DIG_FILTER_SETTLED];
		unsigned mask = 1<<chx;

		printf("%d ", (reg&mask) != 0);
		return COMMAND_OK;
	}
};

class OvervoltageQuery: public Command {
public:
	OvervoltageQuery():
		Command("overvoltage", "read from cache readall first..") { isChannelCommand = true; }

	/*
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		unsigned reg = module.cache()[Ad7134::AIN_OR_ERROR];
		printf("%d %d %d %d ", (reg&0x1)!=0, (reg&0x2)!=0, (reg&0x4)!=0, (reg&0x8)!=0);
		return COMMAND_OK;
	}
	*/
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		if (argc < 2) die(help());
		unsigned ch = strtoul(argv[1], 0, 0);

		if (ch < 1 || ch > 32){
			return -COMMAND_ERR;
		}

		unsigned chx;
		Ad7134 *ad7134 = module.chip(ch, chx);
		unsigned reg = ad7134->regs[Ad7134::AIN_OR_ERROR];
		unsigned mask = 1<<chx;

		printf("%d ", (reg&mask) != 0);
		return COMMAND_OK;
	}
};

class HeartbeatQuery: public Command {
public:
	HeartbeatQuery():
		Command("heartbeat", "read from cache readall first..") {}
	int operator() (class Acq465ELF& module, int argc, const char** argv) {
		printf("%u ", module.cache()[Ad7134::MCLK_COUNTER]);
		return COMMAND_OK;
	}
};

struct CompareCommands {
	bool operator() (Command* a, Command *b) {
		return strcmp(a->cmd, b->cmd) < 0;
	}
} compareCommands;


void Acq465ELF::init_commands()
{
	std::sort(commands.begin(), commands.end(), compareCommands);
}

vector<Command*> Acq465ELF::commands = {
	new MCLK_Monitor,
	new ODR_Command,
	new FilterCommand,
	new WordSizeCommand,
	new DclkFreqCommand,
	new GainCommand,
	new OffsetCommand,
	new SetReg,
	new FlushCommand,
	new ReadAllCommand,
	new ResetCommand,
	new ScratchpadTest,

	new HelpCommand,
	new MakeLinksCommand,

	new DeviceStatusQuery,
	new InternalErrorQuery,
	new FilterSettledQuery,
	new OvervoltageQuery,
	new HeartbeatQuery
};

int  Acq465ELF::operator() (int argc, const char** argv)
{
	char argv0[80];
	strncpy(argv0, argv[0], 80);
	const char* verb = basename(argv0);

	if (strcmp(verb, "acq465_knobs") == 0){
		argv = &argv[1];
		verb = argv[0];
		argc--;
	}
	bool is_query = argc == 1;


	if (argc == 0){
		printf("usage: acq465_knobs command [acq465_help]\n");
		return 0;
	}

	/* handle busy box style verbs */
	if (strncmp(verb, "acq465_", 7) == 0){
		verb += 7;
	}

	/* channel commands: set: MUST be for single ch at argv[1], but bulk get possible */


	for (VCI it = commands.begin(); it != commands.end(); ++it){
		Command &command = *(*it);
		if (strcmp(verb, command.cmd) == 0){
			if (command.isChannelCommand && !is_query){		// first arg assumed ch 1..32
				int rc = command(*this, argc, argv);
				switch(rc){
				default:
					fprintf(stderr, "ERROR:%d\n", rc);
					return -1;
				case COMMAND_OK:
					break;
				case COMMAND_FLUSH:
					flush();
				break;
				case COMMAND_QUIT:
					return 0;
				}
			}else if (command.isChannelCommand){
				const char** ch_argv = new const char* [argc+1];    // new set of args with ch at ch_argv[1]
				char chid[4];					    // string with channel number 1..32
				ch_argv[0] = argv[0];
				ch_argv[1] = chid;
				for (int ii = 1; ii < argc; ++ii){
					ch_argv[ii+1] = argv[ii];
				}
				argc += 1;
				for (int ch = 1; ch <= 32; ++ch){
					snprintf(chid, 4, "%d", ch);
					int rc = command(*this, argc, ch_argv);
					switch(rc){
						default:
						fprintf(stderr, "ERROR:%d\n", rc);
						return -1;
					case COMMAND_OK:
						break;
					case COMMAND_FLUSH:
						flush();
						break;
					case COMMAND_QUIT:
						return 0;
					}
				}
				delete [] ch_argv;
			}else{
				for (const char* cursor = chips; *cursor; ++cursor){
					lcs = *cursor - 'A';
					int rc = command(*this, argc, argv);
					switch(rc){
					default:
						fprintf(stderr, "ERROR:%d\n", rc);
						return -1;
					case COMMAND_OK:
						break;
					case COMMAND_FLUSH:
						flush();
						break;
					case COMMAND_QUIT:
						return 0;
					}
				}
			}
			return 0;
		}
	}
	fprintf(stderr, "ERROR: command \"%s\" not found\n", verb);
	return -1;
}




int dig_if_reset(const char* sites)
/* sent a broadcast DIG_IF_RESET to all chips (and, in future, all sites) */
{
	std::vector<Acq465ELF*> modules;

	printf("%s 01\n", __FUNCTION__);
	for (const char* cursor = sites; *cursor; ++cursor){
		printf("%s 01 %c\n", __FUNCTION__, *cursor);
		if (cursor[0] >= '1' && cursor[0] <= '6'){
			int site = cursor[0] - '0';
			printf("%s 01 site=%d\n", __FUNCTION__, site);
			Acq465ELF* module = new Acq465ELF(site, 0);
			modules.push_back(module);
			unsigned doit = cursor[1] == ','? 0: 1;
			if (G_verbose > 1){
				fprintf(stderr, "waiting for go, doit=%d\n", doit);
				getchar();
			}
			module->dig_if_reset(doit);
			if (G_verbose > 1){
				fprintf(stderr, "waiting for continue\n");
				getchar();
			}
		}
	}

	for (Acq465ELF* module: modules){
		delete module;
	}
	return 0;
}

const char* G_chips = "ABCDEFGH";
const char* G_sites;


struct poptOption opt_table[] = {
	{ "all", 	'A', POPT_ARG_NONE, 	0, 		'A', 	"access all chips"    			},
	{ "chips", 	'c', POPT_ARG_STRING, 	&G_chips, 	0, 	"access selected chips [ABCDEFG]"	},
	{ "verbose",   	'v', POPT_ARG_INT, 	&G_verbose, 	0,	"set verbosity"	    			},
	{ "terse",      't', POPT_ARG_INT,      &G_terse,       0,      "limit output lines"                    },
	{ "dig_if_reset",'r', POPT_ARG_STRING,  &G_sites,       'r',    "digital interface reset, all chips, all sites" },
	POPT_AUTOHELP
	POPT_TABLEEND
};

int main(int argc, const char** argv)
{
	int site = 1;
	if (getenv("SITE")){
		site = atoi(getenv("SITE"));
	}
	if (getenv("LCS")){
		fprintf(stderr, "SORRY, LCS has been dropped, use --chips=[ABCDEFG] or --all\n");
		exit(1);
	}
	poptContext opt_context = poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		case 'r':
			return dig_if_reset(G_sites);
		case 'A':
			G_chips = "ABCDEFGH";
			break;
		}
	}

	int argc2;
	const char** argv2 = new const char*[argc+1];  // it will be no bigger and perhaps smaller. Add one for a null
	argv2[0] = argv[0];
	for (argc2 = 1; (argv2[argc2] = poptGetArg(opt_context)); ++argc2){
		;
	}
	argv2[argc2] = 0;

#if 0
	printf("chips: %s\n", G_chips);
	for (int ii = 0; ii < argc2; ++ii){
		printf("%d: %s\n", ii, argv2[ii]);
	}
	//exit(0);
#endif

	Acq465ELF module(site, G_chips);


	return module(argc2, argv2);
}
