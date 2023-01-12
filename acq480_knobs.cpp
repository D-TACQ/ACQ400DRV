/*
 * acq480_knobs.cpp
 *
 *  Created on: 15 Feb 2015
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

#include <algorithm>    // std::sort


using namespace std;

#include "ads5294.h"
#include "acq480_ioctl.h"

#include "Knob.h"

struct Command;

class Acq480FMC {

	int site;

	static vector<Command*> commands;
	FILE* fp;

	static bool is_acq482(int _site) {
		unsigned _is_acq482;
		Knob k = Knob(_site, "acq482_cmap");
		if (k.get(&_is_acq482) == 1){
			return _is_acq482;
		}else{
			return 0;
		}
	}
	static const int cmap_acq480[9];
	static const int cmap_acq482[9];

	void init_commands();
	const int* cmap;
public:
	Ads5294 chip;


	Acq480FMC(int _site) : site(_site), cmap(is_acq482(_site)? cmap_acq482: cmap_acq480) {
		char fname[80];
		snprintf(fname, 80, "/dev/acq480.%d", site);
		fp = fopen(fname, "r+");
		if (!fp){
			perror(fname);
			exit(1);
		}
		chip.regs = new Ads5294Regs;
		chip.regs->regs = static_cast<Reg *>(
				mmap(NULL, ADS5294_SPI_MIRROR_SZ,
						PROT_READ|PROT_WRITE,
						MAP_SHARED,
						fileno(fp), 0));
		init_commands();
	}

	Ads5294::Chan pchan(int lchan){
		if (!(lchan >= 0 && lchan <= 8)){
			fprintf(stderr, "ERROR: CH %d not in range 0..8\n", lchan);
			exit(1);
		}
		return static_cast<Ads5294::Chan>(cmap[lchan]);
	}
	~Acq480FMC() {
		flush();
		/*
		munmap(chip.regs->regs, ADS5294_SPI_MIRROR_SZ);
		printf("about to close\n");
		fclose(fp);
		printf("closed\n");
		*/
	}
	int operator() (int argc, char* argv[]);


	Reg* regs() {
		return chip.regs->regs;
	}
	int invalidate() {
		return ioctl(fileno(fp), ACQ480_CACHE_INVALIDATE);
	}
	int flush() {
		return ioctl(fileno(fp), ACQ480_CACHE_FLUSH);
	}
	int reset() {
		ioctl(fileno(fp), ACQ480_RESET);
		sleep(1);
		return ioctl(fileno(fp), ACQ480_CACHE_INVALIDATE);
	}
	friend class HelpCommand;
	friend class MakeLinksCommand;


	void recommendRetrain() {
		char fname[80];
		snprintf(fname, 80, "%s/acq480.%d.retrain_requested",
				"/dev/shm", site);
		FILE* fp = fopen(fname, "w");
		if (fp != 0){
			fputs("1", fp);
			fclose(fp);
		}else{
			perror(fname);
		}
	}
};

const int Acq480FMC::cmap_acq480[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
const int Acq480FMC::cmap_acq482[9] = { 0, 8, 7, 6, 5, 4, 3, 2, 1 };


struct Command {
	const char* cmd;
	const char* args_help;

	char* _help;
	virtual int operator() (class Acq480FMC module, int argc, char* argv[]) = 0;
	/* return > 0 if flush recommended */

	Command(const char* _cmd, const char* _args_help = "") :
		cmd(_cmd), args_help(_args_help) {
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

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		for (VCI it = module.commands.begin(); it != module.commands.end(); ++it){
			printf("%s\n", (*it)->help());
		}
		return 0;
	}
};





class MakeLinksCommand: public Command {
public:
	MakeLinksCommand() : Command("makeLinks") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		for (VCI it = module.commands.begin(); it != module.commands.end(); ++it){
			printf("ln -s %s acq480_%s\n", "/usr/local/bin/acq480_knobs", (*it)->cmd);
		}
		return 0;
	}
};
class ResetCommand: public Command {
public:
	ResetCommand() : Command("reset") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		return module.reset();
	}
};

class ReadAllCommand: public Command {
public:
	ReadAllCommand() : Command("readall") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		return module.invalidate();
	}
};
class FlushCommand: public Command {
public:
	FlushCommand() : Command("flush") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		return 1;
	}
};

class DumpCommand: public Command {
public:
	DumpCommand() : Command("dump") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		Reg *r0 = module.regs();
		for (int ii = 0; ii < NREGS; ++ii){
			if (ii%16 == 0){
				printf("%02x:", ii);
			}
			printf("%04x%c", r0[ii], (ii+1)%16==0? '\n': ' ');
		}
		return 0;
	}
};

class SetGainCommand: public Command {
public:
	SetGainCommand() : Command("setGain", "CHAN GAIN") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 3) die(help());

		int rc = module.chip.setGain(
				module.pchan(atoi(argv[1])),
				static_cast<Ads5294::Gain>(atoi(argv[2])));
		if (rc != 0) die("setGain failed");
		return 1;
	}
};

class GetGainCommand: public Command {
public:
	GetGainCommand() : Command("getGain", "CHAN") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		printf("%d\n", module.chip.getGain(
				module.pchan(atoi(argv[1]))));
		return 0;
	}

};

class SetDecimationFilterCommand: public Command {
public:
	SetDecimationFilterCommand() :
		Command("setDecimationFilter", "CHAN FILTER [odd]") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 3) die(help());
		int rc = module.chip.setDecimationFilter(
				module.pchan(atoi(argv[1])),
				static_cast<Ads5294::Filter>(atoi(argv[2])),
				argc<4? false: atoi(argv[3]));
		if (rc != 0) die("setGain failed");
		module.recommendRetrain();
		return 1;
	}
};

class SetFilterCoefficientsCommand: public Command {
	static void setCoeffs(short coeffs[], int argc, char** argv) {
		if (argc > NTAPS) argc = NTAPS;
		for (int ii = 0; ii < argc; ++ii){
			coeffs[ii] = strtol(argv[ii], 0, 0);
		}
	}
public:
	SetFilterCoefficientsCommand() :
		Command("setFilterCoefficients", "CHAN [coeff [coeff]]") {}

	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		short coeffs[NTAPS] = {};
		if (argc > 2){
			setCoeffs(coeffs, argc-2, argv+2);
		}

		Ads5294::Chan chan = module.pchan(atoi(argv[1]));
		if (chan == Ads5294::CHX){
			for (int ch = 1; ch <= 8; ch = ch+1){
				if (module.chip.setCustomCoefficients(
					module.pchan(ch), argc > 2? coeffs: 0) != 0){
					return -1;
				}
			}
		}else{
			if (module.chip.setCustomCoefficients(
						chan, argc > 2? coeffs: 0) != 0){
				return -1;
			}
		}
		return 1;
	}
};

class SetHiPassFilterCommand: public Command {
public:
	SetHiPassFilterCommand() :
		Command("setHiPassFilter", "CHAN [enable hpfun]") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());

		module.chip.setHiPassFilter(
				module.pchan(atoi(argv[1])),
				argc > 2? atoi(argv[2]): false,
				argc > 3? strtoul(argv[3], 0, 0): 0
				);
		return 0;
	}
};

class SetDataRateCommand: public Command {
public:
	SetDataRateCommand() :
		Command("setDataRate", "RATE") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.setDataRate(
			static_cast<Ads5294::DataRate>(atoi(argv[1])));
		return 1;
	}
};

class SetAverageSelectCommand: public Command {
public:
	SetAverageSelectCommand() :
		Command("setAverageSelect", "CHAN [ENABLE RATE]") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.setAverageSelect(
			module.pchan(atoi(argv[1])),
			argc>2? atoi(argv[2]): false,
			argc>3? strtoul(argv[3], 0, 0): 0
		);
		return 1;
	}
};

class SetInvertCommand: public Command {
public:
	SetInvertCommand() :
		Command("setInvert", "CHAN [disable]") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.setInvert(
				module.pchan(atoi(argv[1])),
				argc>2? atoi(argv[2]): true);
		return 1;
	}
};
class SetLFNSCommand: public Command {
public:
	SetLFNSCommand() :
		Command("setLFNS", "CHAN [disable]") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.setLFNS(
			module.pchan(atoi(argv[1])),
			argc>2? atoi(argv[2]): true
		);
		return 1;
	}
};

class SetLvdsTestPatRamp: public Command {
public:
	SetLvdsTestPatRamp() :
		Command("setLvdsTestPatRamp", "ENABLE") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.SetLvdsTestPatRamp(atoi(argv[1]));
		return 1;
	}
};

class SetLvdsTestPatDeskew: public Command {
public:
	SetLvdsTestPatDeskew() :
		Command("setLvdsTestPatDeskew", "ENABLE") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.SetLvdsTestPatDeskew(atoi(argv[1]));
		return 1;
	}
};

class SetPatDeskew: public Command {
public:
	SetPatDeskew() :
		Command("setPatDeskew", "ENABLE") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.setPatDeskew(atoi(argv[1]));
		return 1;
	}
};

class SetPatSync: public Command {
public:
	SetPatSync() :
		Command("setPatSync", "ENABLE") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		module.chip.setPatSync(atoi(argv[1]));
		return 1;
	}
};

class SetDataPattern: public Command {
public:
	SetDataPattern() :
		Command("setDataPattern", "[HEXCODE | KEY | HELP]") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 1) die(help());
		if (argc == 2){
			unsigned regval;

			if (sscanf(argv[1], "0x%x", &regval) == 1 ||
			    sscanf(argv[1], "0X%x", &regval) == 1 ||
			    sscanf(argv[1], "%u", &regval) == 1		){
				    return module.chip.setDataPattern(regval);
			}
		}
		if (argc == 1){
			return module.chip.getDataPattern();
		}else{
			return module.chip.setDataPattern(--argc, &argv[1]);
		}
	}
};
class SetReg: public Command {
public:
	SetReg() :
		Command("reg", "REG [VALUE]") {}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		if (argc < 2) die(help());
		unsigned reg = strtoul(argv[1], 0, 0);
		unsigned regval;
		int rc;

		if (argc == 2){
			rc = module.chip.getReg(reg, regval);
			printf("%02x=%04x\n", reg, regval);
		}else{
			regval = strtoul(argv[2], 0, 0);
			rc = module.chip.setReg(reg, regval);
		}
		return rc;
	}
};

class MappingCommand: public Command {
	int mapHelp() {
		printf("enter channel number to set, 0 or omit to clear\n");
		Ads5294::printMapHelp("acq480_knobs map");
		return 0;
	}
public:
	MappingCommand():
		Command("map", "{help|MAP_CHwxyz_TO_OUTab} [ chx[ 2]]")
	{}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		int chx = 0, two_bit = 0;

		switch(argc){
		case 4:
			two_bit = atoi(argv[3]);	// fall thru
		case 3:
			chx = atoi(argv[2]);
			return module.chip.setMap(argv[1], chx, two_bit);
		case 1:
			return module.chip.getMap(MAP_ALL);
		default:
		case 2:
			if (strcmp(argv[1], "help") == 0){
				return mapHelp();
			}else{
				return module.chip.getMap(argv[1]);
			}
		}
	}
};

class PllCommand: public Command {
public:
	PllCommand() :
		Command("PLL", "[FSMSPS DECIM]")
	{}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		switch(argc){
		case 1:
			return module.chip.getPLL();
		case 3:
			return module.chip.setPLL(atoi(argv[1]), atoi(argv[2]));
		default:
			die(help());
			return -1;		// not gonna happen
		}
	}
};

class SetTwoWireMode: public Command {
public:
	SetTwoWireMode() :
		Command ("setTwoWireMode", "0|1")
	{}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		bool enable = true;
		switch(argc){
		case 2:
			enable = atoi(argv[1]); // fall thru
		case 1:
			return module.chip.setTwoWireMode(enable);
		default:
			die(help());
			return -1;		// not gonna happen
		}
	}
};

class SetClkHardSync: public Command {
public:
	SetClkHardSync() :
		Command ("setClkHardSync", "0|1")
	{}
	int operator() (class Acq480FMC module, int argc, char* argv[]) {
		bool enable = true;
		switch(argc){
		case 2:
			enable = atoi(argv[1]); // fall thru
		case 1:
			return module.chip.setClkHardSync(enable);
		default:
			die(help());
			return -1;		// not gonna happen
		}
	}
};

struct CompareCommands {
	bool operator() (Command* a, Command *b) {
		return strcmp(a->cmd, b->cmd) < 0;
	}
} compareCommands;


void Acq480FMC::init_commands()
{
	std::sort(commands.begin(), commands.end(), compareCommands);
}

vector<Command*> Acq480FMC::commands = {
	new SetInvertCommand,
	new SetLFNSCommand,
	new SetAverageSelectCommand,
	new SetDataRateCommand,
	new SetHiPassFilterCommand,
	new SetFilterCoefficientsCommand,
	new SetDecimationFilterCommand,
	new SetGainCommand,
	new GetGainCommand,
	new SetLvdsTestPatRamp,
	new SetLvdsTestPatDeskew,
	new SetPatDeskew,
	new SetPatSync,
	new SetDataPattern,
	new SetTwoWireMode,
	new SetClkHardSync,
	new MappingCommand,
	new PllCommand,
	new SetReg,
	new DumpCommand,
	new FlushCommand,
	new ReadAllCommand,
	new ResetCommand,
	new HelpCommand,
	new MakeLinksCommand
};


int  Acq480FMC::operator() (int argc, char* argv[])
{
	char** arg0 = argv;
	char* verb = basename(argv[0]);

	if (strcmp(verb, "acq480_knobs") == 0){
		arg0 = &argv[1];
		verb = arg0[0];
		argc--;
	}

	if (argc == 0){
		printf("usage: acq480_knobs command [acq480_help]\n");
		return 0;
	}

	/* handle busy box style verbs */
	if (strncmp(verb, "acq480_", 7) == 0){
		verb += 7;
	}

	for (VCI it = commands.begin(); it != commands.end(); ++it){
		Command &command = *(*it);
		if (strcmp(verb, command.cmd) == 0){
			if (command(*this, argc, arg0) > 0){
				flush();
			}
			break;
		}
	}
	return 0;
}
int main(int argc, char* argv[])
{
	int site = 1;
	if (getenv("SITE")){
		site = atoi(getenv("SITE"));
	}

	Acq480FMC module(site);

	return module(argc, argv);
}
