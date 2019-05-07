/* ------------------------------------------------------------------------- *
 * acq400_knobs.cpp  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2013 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 30 Dec 2013  
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
What does acq400_knobs do?.

- r : readable
- w : writable
- x : executes it
- wildcard query
- help
- help2

- ranges wbn too.

- load the directory at start. precompute all status.

- .ranges file?.
*/

/* fixes error stat: Value too large for defined data type */
#define _FILE_OFFSET_BITS 64

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>    // std::find
#include <vector>
#include <string>
#include <sstream>

#include <glob.h>
#include "popt.h"

#include "tcp_server.h"

const char* pattern = "";

using namespace std;

#define VERID "acq400_knobs B1006"

bool err;
char* host = 0;		/* server allows connect from host (any) */
char* port = 0;

int site;

int verbose = 0;

#define MAXOUTBUF	16384

#define VPRINTF	if (verbose) printf

class Knob;
vector<Knob*> KNOBS;
typedef vector<Knob*>::iterator VKI;


char* chomp(char *str) {
	char* cursor = str + strlen(str)-1;
	while (std::isspace(*cursor) && cursor >= str){
		*cursor-- = '\0';
	}
	return str;
}



inline std::vector<std::string> glob(const std::string& pat){
    using namespace std;
    glob_t glob_result;
    int rc = glob(pat.c_str(), 0, NULL, &glob_result);
    if (rc != 0){
	    printf("ERROR: cwd %s glob %s returns %d\n",
			    get_current_dir_name(), pat.c_str(), rc);
    }
    vector<string> ret;
    for(unsigned int i=0; i < glob_result.gl_pathc; ++i){
        ret.push_back(string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
    return ret;
}



class File {
public:
	FILE *fp;
	File(const char* name, const char* mode = "r") {
		fp = fopen(name, mode);
	}
	virtual ~File() {
		if (fp) fclose(fp);
	}
};

class Pipe {
public:
	FILE* fp;
	Pipe(const char* name, const char* mode = "r") {
		fp = popen(name, mode);
	}
	int close() {
		int rc = 0;
		if (fp) {
			rc = pclose(fp);
			fp = 0;
		}
		return rc;
	}
	virtual ~Pipe() {
		if (close() == -1){
			;
			//perror("pclose");
		}
	}
};

class Validator {
public:
	virtual ~Validator() {}
	virtual bool isValid(char* buf, int maxbuf, const char* args) = 0;
};

class NullValidator : public Validator
/* singleton */
{
	NullValidator() {}
public:
	virtual bool isValid(char* buf, int maxbuf, const char* args) {
		return true;
	}

	static Validator* instance() {
		static Validator* _instance;
		if (!_instance){
			return _instance = new NullValidator;
		}else{
			return _instance;
		}
	}
};

class NumericValidator : public Validator {
	int rmin, rmax;
	NumericValidator(int _rmin, int _rmax) : rmin(_rmin), rmax(_rmax) {

	}
public:
	virtual bool isValid(char* buf, int maxbuf, const char* args) {
		int tv;
		if (sscanf(args, "%d", &tv) != 1 ){
			snprintf(buf, maxbuf,
				"ERROR: NumericValidator %s not numeric", args);
			return false;
		}else{
			bool ok = tv >= rmin && tv <= rmax;
			if (!ok){
				snprintf(buf, maxbuf,
				"ERROR: NumericValidator %d not in range %d,%d",
					tv, rmin, rmax);
			}
			return ok;
		}
	}

	static Validator* create(const char* def){
		const char* ndef;
		if ((ndef = strstr(def, "numeric=")) != 0){
			int _rmin, _rmax;
			if (sscanf(ndef, "numeric=%d,%d", &_rmin, &_rmax) == 2){
				return new NumericValidator(_rmin, _rmax);
			}
		}

		return 0;
	}
};
class Knob {

protected:
	Knob(const char* _name) {
		name = new char[strlen(_name)+1];
		strcpy(name, _name);
		validator = NullValidator::instance();
	}
	char* name;
	Validator *validator;

	void cprint(const char* ktype) {
		printf("%8s %s\n", ktype, name);
	}
	bool isValid(char* buf, int maxbuf, const char* args){
		return validator->isValid(buf, maxbuf, args);
	}
	virtual int _set(char* buf, int maxbuf, const char* args) = 0;
public:
	virtual ~Knob() {
		delete [] name;
	}

	vector<Knob*> peers;

	char* getName() { return name; }
	virtual const char* getAttr() {
		return "";
	}
	/* return >0 on success, <0 on fail */
	virtual int set(char* buf, int maxbuf, const char* args) {
		vector<Knob*>::iterator it;
		for (it = peers.begin(); it != peers.end(); ++it){
			(*it)->_set(buf, maxbuf, args);
		}
		return _set(buf, maxbuf, args);
	}
	virtual int get(char* buf, int maxbuf) = 0;
	virtual void print(void) { cprint("Knob"); }


	static Knob* create(const string _name, mode_t mode);

	static int match(const char* name, const char* key) {
		if (strcmp(name, key) == 0){
			return 1;
		}else if (fnmatch(key, name, 0) == 0){
			return -1;
		}else{
			return 0;
		}
	}
};

class KnobRO : public Knob {
protected:
	virtual int _set(char* buf, int maxbuf, const char* args) {
		return -snprintf(buf, maxbuf, "ERROR: \"%s\" is read-only", name);
	}
public:
	KnobRO(const char* _name) : Knob(_name) {}


	virtual int get(char* buf, int maxbuf) {
		File knob(name, "r");
		if (knob.fp == NULL) {
			return snprintf(buf, maxbuf,
				"ERROR: %d \"%s\" failed to open \"%s\"\n",
				errno, strerror(errno), name);
		}else{
			return fgets(buf, maxbuf, knob.fp) != NULL;
		}
	}
	virtual void print(void) { cprint("KnobRO"); }
	virtual const char* getAttr() {
		return "r";
	}
};

class KnobRW : public KnobRO {
protected:
	virtual int _set(char* buf, int maxbuf, const char* args) {
		if (!isValid(buf, maxbuf, args)){
			return -1;
		}
		File knob(name, "w");
		if (knob.fp == NULL){
			return -snprintf(buf, maxbuf, "ERROR: failed to open \"%s\"\n", name);
		}else if (fputs(args, knob.fp) < 0){
			return -snprintf(buf, maxbuf, "ERROR:");
		}else{
			return snprintf(buf, maxbuf, "\n");
		}
	}
public:
	KnobRW(const char* _name) : KnobRO(_name) {
	}


	virtual void print(void) { cprint ("KnobRW"); }
	virtual const char* getAttr() {
			return "rw";
	}
};


class KnobX : public Knob {
	int runcmd(const char* cmd, char* buf, int maxbuf);
	char attr[4];
protected:
	virtual int _set(char* buf, int maxbuf, const char* args) {
		if (!isValid(buf, maxbuf, args)){
			return -1;
		}
		char cmd[128];
		snprintf(cmd, 128, "%s %s", name, args);
		return runcmd(cmd, buf, maxbuf);

	}
public:
	KnobX(const char* _name) : Knob(_name) {
		struct stat sb;
		int ic = 0; attr[ic] = '\0';
		if (stat(name, &sb) != -1){
			if (sb.st_mode&(S_IRUSR|S_IRGRP|S_IROTH)) attr[ic++] = 'r';
			if (sb.st_mode&(S_IWUSR|S_IWGRP|S_IWOTH)) attr[ic++] = 'w';
			if (sb.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH)){
				attr[ic++] = 'x';
			}else{
				fprintf(stderr, "ERROR: KnobX \"%s\" is not executable\n", _name);
			}
			attr[ic] = '\0';
		}else{
			fprintf(stderr, "ERROR: KnobX \"%s\" does not exist\n", _name);
		}
	}

	virtual int get(char* buf, int maxbuf) {
		char cmd[128];
		snprintf(cmd, 128, "%s ", name);		// @@todo no trailing space, no work
		return runcmd(cmd, buf, maxbuf);
	}
	virtual void print(void) { cprint("KnobX"); }
	virtual const char* getAttr() {
		return attr;
	}
};

int KnobX::runcmd(const char* cmd, char* buf, int maxbuf){
	Pipe knob(cmd, "r");
	if (knob.fp == NULL) {
		return -snprintf(buf, maxbuf,
				"ERROR: failed to open \"%s\"\n", name);
	}
	char* cursor = buf;
	for (; (cursor = fgets(cursor, maxbuf-(cursor-buf), knob.fp)) != NULL;
		cursor += strlen(cursor)){
		;
	}

	return cursor-buf;

}



#define HASX(mode) 	(((mode)&(S_IXUSR|S_IXGRP|S_IXOTH)) != 0)
#define HASW(mode)	(((mode)&(S_IWUSR|S_IWGRP|S_IWOTH)) != 0)



std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
	    elems.push_back(item);
    }
    return elems;
}

class PeerFinder {
	vector<int> peers;
	vector<string> knobs;

	static PeerFinder *_instance;

	PeerFinder() {

	}

	void fillPeerNames(vector<string>* peer_names, string knob) {
		vector<int>::iterator it;
		for (it = peers.begin(); it != peers.end(); ++it){
			if (*it != site){
				char name[80];
				sprintf(name, "../%d/%s", *it, knob.c_str());
				struct stat sb;
				if (stat(name, &sb) != -1){
					peer_names->push_back(*new string(name));
				}
			}
		}
	}

	void initKnobs(vector<string> kdef) {
		knobs = kdef;

	}
	void initPeers(vector<string> pdef) {
		vector<string>::iterator it;
		for(it = pdef.begin(); it != pdef.end(); ++it){
			peers.push_back(atoi((*it).c_str()));
		}
	}
public:
	static PeerFinder* instance() {
		if (_instance == 0){
			_instance = new PeerFinder;
		}
		return _instance;
	}
	static PeerFinder* create(string def_file);

	bool hasPeers(string knob){
		return peers.size() > 0 &&
		     std::find(knobs.begin(), knobs.end(), knob) != knobs.end();
	}

	vector<string>* getPeernames(string knob){
		// peer_names belongs to client on return
		vector<string>* peer_names = new vector<string>();

		if (hasPeers(knob)){
			VPRINTF("fillPeerNames() %d\n", peer_names->size());
			fillPeerNames(peer_names, knob);
		}
		return peer_names;
	}
};

PeerFinder* PeerFinder::_instance;

#define PKEY "PEERS="
#define KKEY "KNOBS="

#define KEYLEN 6

PeerFinder* PeerFinder::create(string def_file)
{
	_instance = new PeerFinder();
	FILE *fp = fopen(def_file.c_str(), "r");
	if(fp){
		char defline[128];
		while(fgets(defline, sizeof(defline), fp)){
			chomp(defline);
			if (defline[0] == '#' || strlen(defline) < 2){
				continue;
			}else{
				std::vector<std::string> elems;
				if (strncmp(PKEY, defline, KEYLEN) == 0){
					_instance->initPeers(split(defline+KEYLEN, ',', elems));
				}
				if (strncmp(KKEY, defline, KEYLEN) == 0){
					_instance->initKnobs(split(defline+KEYLEN, ',', elems));
				}
			}
		}
	}
	return _instance;
}
Knob* Knob::create(const string _name, mode_t mode)
{
	const char* name = _name.c_str();
	Knob* knob;
	int verbosek = 0;
	if (strncmp(name, "streamtonowhered", 10) == 0){
		VPRINTF("we have it\n");
		verbosek = 1;
	}
	if (HASX(mode)){
		knob = new KnobX(name);
	}else if (HASW(mode)){
		static int limit_check = -2;

		Knob* knob = new KnobRW(name);

		if (limit_check == -2){
			limit_check = access("/usr/share/doc/numerics", F_OK);
		}
		if (limit_check == 0){
			char cmd[128];
			char reply[128];
			sprintf(cmd, "grep %s /usr/share/doc/numerics", name);
			Pipe pn(cmd, "r");
			if (fgets(reply, 128, pn.fp)){
				Validator* v = NumericValidator::create(reply);
				if (v){
					knob->validator = v;
				}
			}
		}
		return knob;
	}else{
		knob = new KnobRO(name);
	}
	if (verbosek){
		knob->print();
		VPRINTF("::create done\n");
	}
	return knob;

}


class GroupKnob : public Knob {
	GroupKnob(string name) : Knob(name.c_str()) {}

	virtual int _set(char* buf, int maxbuf, const char* args)
	/* the peers do ALL the work */
	{
		return -1;
	}
public:

	virtual int get(char* buf, int maxbuf) {
		return peers[0]->get(buf, maxbuf);
	}
	static Knob* create(string name, string def);
};

#define GROUP_MODE	(S_IWUSR|S_IWGRP|S_IWOTH)

Knob* GroupKnob::create(string name, string def)
{
	Knob* knob = new GroupKnob(name);
	/* first collect all the peers in the same site .. they already exist */
	/* the double iteration isn't efficient, but 666MIPS .. who cares .. */
	vector<string> peer_names = glob(def);
	vector<string>::iterator its;
	for (its = peer_names.begin(); its < peer_names.end(); ++its){
		for (VKI itk = KNOBS.begin(); itk != KNOBS.end(); ++itk){
			if (Knob::match((*itk)->getName(), (*its).c_str()) == 1){
				knob->peers.push_back(*itk);
			}
		}
	}

	/* then collect peers for other sites. we have to make them */
	if (PeerFinder::instance()->hasPeers(name)){
		vector<string>* peer_names = PeerFinder::instance()->getPeernames(name);
		vector<string>::iterator it;
		for (it = peer_names->begin(); it != peer_names->end(); ++it){
			VPRINTF("create Knob %s\n", (*it).c_str());
			knob->peers.push_back(Knob::create(*it, GROUP_MODE));
		}
		VPRINTF("knob %s has %d peers\n", name.c_str(), knob->peers.size());
	}
	return knob;
}


class Grouper {
	static void create(vector<string> &elems);
public:
	static void create(string group);
};

/*
group file
group_name group_def
group_def is classically a glob pattern
in the future, it could be a comma sep list
*/

void Grouper::create(vector<string> &elems) {
	KNOBS.push_back(GroupKnob::create(elems[0], elems[1]));
}

void Grouper::create(string def_file)
{
	FILE *fp = fopen(def_file.c_str(), "r");
	if(fp){
		char defline[128];
		while(fgets(defline, sizeof(defline), fp)){
			chomp(defline);
			if (defline[0] == '#' || strlen(defline) < 2){
				continue;
			}else{
				std::vector<std::string> elems;
				split(defline, '=', elems);
				if (elems.size() < 2){
					fprintf(stderr, "ERROR: group def must be group=glob\n");
				}
				create(elems);
			}
		}
	}
	fclose(fp);
}

class Helper: public Knob
{
public:
	Helper(const char* _name) : Knob(_name) {}
};
class Prompt: public Helper
/* singleton */
{

	static bool enabled;
	Prompt(): Helper("prompt") {}

	/* return >0 on success, <0 on fail */
	virtual int _set(char* buf, int maxbuf, const char* args) {
		if (strcmp(args, "on") == 0){
			enabled = 1;
		}else if (strcmp(args, "off") == 0){
			enabled = 0;
		}
		return 1;
	}
public:

	char* getName() { return name; }


	virtual int get(char* buf, int maxbuf) {
		return snprintf(buf, maxbuf, "%s", enabled? "on": "off");
	}
	virtual void print(void) { cprint("Knob"); }

	void prompt(FILE* fout) {
		if (enabled){
			fprintf(fout, "acq400.%d %d >", site, err);
		}else if (err){
			;
		}
		fflush(fout);
	}
	static Prompt* instance() {
		static Prompt* _instance;
		if (!_instance){
			return _instance = new Prompt;
		}else{
			return _instance;
		}
	}
};

bool Prompt::enabled;


/*
for file in $(ls -1)
do
        echo $file:
        HTEXT="$(grep -m1 ^$file $HROOT/acq400_help* | cut -f2-)"
        if [ $? -eq 0 ]; then
                echo "  $HTEXT";
        else
                echo $file;
        fi
done
*/

#define HROOT "/usr/share/doc"

class Help: public Helper {

protected:
	virtual int query(Knob* knob, char* buf, int buflen){
		snprintf(buf, buflen, "%s\n", knob->getName());
		return 1;
	}
	virtual int _set(char* buf, int maxbuf, const char* args) {
		char* cursor = buf;
		for (VKI it = KNOBS.begin(); it != KNOBS.end(); ++it){
			if (Knob::match((*it)->getName(), args)){
				query(*it, cursor, MAXOUTBUF - (cursor-buf));
				cursor += strlen(cursor);
			}
		}
		return 1;
	}
public:
	Help() : Helper("help") {}
	Help(const char* _key) : Helper(_key) {}
	virtual int get(char* buf, int maxbuf) {
		char* cursor = buf;
		for (VKI it = KNOBS.begin(); it != KNOBS.end(); ++it){
			query(*it, cursor, MAXOUTBUF - (cursor-buf));
			cursor += strlen(cursor);
		}
		return 1;
	}
};

class Help2: public Help {
protected:
	virtual int query(Knob* knob, char* buf, int buflen){
		char cmd[128];
		char reply[128];
		sprintf(cmd, "grep -m1 ^%s %s/acq400_help* | cut -f2 -",
					knob->getName(), HROOT);
		Pipe grep(cmd, "r");
		if (fgets(reply, 128, grep.fp)){
			snprintf(buf, buflen, "%-20s : %s\n\t%s",
				knob->getName(), knob->getAttr(), reply);
		}
		return 1;
	}
public:
	Help2() : Help("help2") {}
};

class HelpA: public Help {
protected:
	virtual int query(Knob* knob, char* buf, int buflen){
		snprintf(buf, buflen, "%-20s : %s\n",
				knob->getName(), knob->getAttr());
		return 1;
	}
public:
	HelpA() : Help("helpA") {}
};

int filter(const struct dirent *dir)
{
        return fnmatch(pattern, dir->d_name, 0);
}

void announce() {
	char fname[80];
	sprintf(fname, "/var/run/acq400_knobs.%d.pid", ::site);
	FILE* fp = fopen(fname, "w");
	fprintf(fp, "%d", getpid());
	fclose(fp);
}
int do_scan()
{
	struct dirent **namelist;
	int nn = scandir(".", &namelist, filter, versionsort);

	if (nn < 0){
		perror("scandir");
		exit(1);
	}
	VPRINTF("scandir returned %d\n", nn);

	for (int ii = 0; ii < nn; ++ii){
		if (strcmp(namelist[ii]->d_name, "peers") == 0){
			PeerFinder::create("peers");
			break;
		}
	}
	for (int ii = 0; ii < nn; ++ii){
		char* alias = namelist[ii]->d_name;
		struct stat sb;
		if (stat(alias, &sb) == -1){
			VPRINTF("ERROR: rejecting %s\n", alias);
			perror("stat");
		}else{
			if (!S_ISREG(sb.st_mode)){
				VPRINTF("not a regular file:%s", alias);
			}else{
				Knob* knob = Knob::create(alias, sb.st_mode);
				if (PeerFinder::instance()->hasPeers(alias)){
					vector<string>* peer_names = PeerFinder::instance()->getPeernames(alias);
					vector<string>::iterator it;
					for (it = peer_names->begin(); it != peer_names->end(); ++it){
						knob->peers.push_back(Knob::create(*it, sb.st_mode));
					}
				}
				KNOBS.push_back(knob);
			}
		}
	}
	KNOBS.push_back(Prompt::instance());
	KNOBS.push_back(new Help);
	KNOBS.push_back(new Help2);
	KNOBS.push_back(new HelpA);

	for (int ii = 0; ii < nn; ++ii){
		if (strcmp(namelist[ii]->d_name, "groups") == 0){
			Grouper::create("groups");
			break;
		}
	}
	free(namelist);

	char newpath[1024];
	snprintf(newpath, 1023, "%s:%s", get_current_dir_name(), getenv("PATH"));

	setenv("PATH", newpath, 1);
	announce();
	return 0;
}



bool is_tcp_server;

struct poptOption opt_table[] = {
	{ "server", 's', POPT_ARG_NONE, 0, 's', "tcp server" },
	{ "host",   'h', POPT_ARG_STRING, &host, 0, "restrict to host (any)" },
	{ "port",   'p', POPT_ARG_STRING, &port, 0, "use this port (4220+site)"},
	POPT_AUTOHELP
	POPT_TABLEEND
};

void cli(int argc, const char** argv)
{
	const char *dir;
	char dbuf[128];
	char sname[32];
	const char* arg1;
	const char* arg2;

	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
		case 's':
			is_tcp_server = true;
			break;
		}
	}
	if (getenv("VERBOSE")){
		verbose = atoi(getenv("VERBOSE"));
	}
	VPRINTF("%s verbose set %d\n", VERID, verbose);

	arg1 = poptGetArg(opt_context);
	if (!arg1){
		return;
	}
	arg2 = poptGetArg(opt_context);

	if (arg2){
		dir = arg2;
	}else{
		site = atoi(arg1);
		sprintf(dbuf, "/etc/acq400/%d", site);
		dir = dbuf;
		sprintf(sname, "%d", site);
	}
	setenv("SITE", sname, 0);
	chdir(dir);

	if (is_tcp_server){
		if (port == 0){
			port = new char[16];
			sprintf(port, "%d", 4220+site);
		}
	}
}


int interpret_phrase(char* phrase, char* buf_out, FILE* fout)
{
	char *args = 0;
	char *key = 0;
	bool is_query = false;
	bool found = false;

	chomp(phrase);
	if (strlen(phrase) == 0){
		return 0;
	}

	unsigned isep = strcspn(phrase, "= ");
	if (isep != strlen(phrase)){
		args = phrase + isep + strspn(phrase+isep, "= ");
		phrase[isep] = '\0';
	}else{
		is_query = true;
	}
	key = phrase;


	for (VKI it = KNOBS.begin(); it != KNOBS.end(); ++it){
		Knob* knob = *it;
		err = false;
		int rc;
		bool is_glob = true;
		buf_out[0] = '\0';
		switch(Knob::match(knob->getName(), key)){
		case 0:
			continue;
		case 1:
			is_glob = false;
			it = KNOBS.end() - 1; // fall thru, drop out
		case -1:
			if (is_query){
				rc = knob->get(buf_out, 4096);
				if (is_glob){
					if (dynamic_cast<Helper*>(knob) != 0){
						/* don't query Helper */
						continue;
					}
					if (!strstr(buf_out, knob->getName())){
						fprintf(fout, "%s ", knob->getName());
					}
				}
			}else{
				rc = knob->set(buf_out, 4096, args);
			}
			if (rc){
				fprintf(fout, "%s\n", chomp(buf_out));
				fflush(fout);
			}

			err = rc < 0;
			found = true;
		}
	}

	if (!found){
		fprintf(fout, "ERROR:\%s\" not found\n", key);
	}
	return 0;
}
int interpreter(FILE* fin, FILE* fout)
{
	char* buf_in = new char[4096];
	char* buf_out = new char[MAXOUTBUF];

	for (; fgets(buf_in, 4096, fin); Prompt::instance()->prompt(fout)){
		char* phrase = buf_in;
		char* phrase99;

		while ((phrase99 = strchr(phrase, ';'))){
			char* phrase2 = phrase99 + strspn(phrase99, "; \t");
			*phrase99 = '\0';
			interpret_phrase(phrase, buf_out, fout);
			phrase = phrase2;
		}

		interpret_phrase(phrase, buf_out, fout);
	}
	return 0;
}



int main(int argc, const char* argv[])
{
	cli(argc, argv);
	do_scan();
	if (is_tcp_server){
		printf("call tcp_server\n");
		return tcp_server(host, port, interpreter);
	}else{
		return interpreter(stdin, stdout);
	}
}
