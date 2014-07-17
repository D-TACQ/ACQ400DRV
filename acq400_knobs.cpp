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


#include <dirent.h>
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

const char* pattern = "";

using namespace std;

#define VERID "acq400_knobs B1000"

bool err;
int site;

int verbose;

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
		fclose(fp);
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
			perror("pclose");
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
		if (ndef = strstr(def, "numeric=")){
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
			return snprintf(buf, maxbuf, "ERROR: failed to open \"%s\"\n", name);
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
		}else{
			return fputs(args, knob.fp) > 0? 0: -snprintf(buf, maxbuf, "ERROR:");
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
	KnobX(const char* _name) : Knob(_name) {}

	virtual int get(char* buf, int maxbuf) {
		char cmd[128];
		snprintf(cmd, 128, "%s ", name);		// @@todo no trailing space, no work
		return runcmd(cmd, buf, maxbuf);
	}
	virtual void print(void) { cprint("KnobX"); }
	virtual const char* getAttr() {
			return "rwx";
	}
};

int KnobX::runcmd(const char* cmd, char* buf, int maxbuf){
	Pipe knob(cmd, "r");
	if (knob.fp == NULL) {
		return -snprintf(buf, maxbuf,
				"ERROR: failed to open \"%s\"\n", name);
	}
	char* cursor;
	for (char* cursor = buf;
		(cursor = fgets(cursor, maxbuf-(cursor-buf), knob.fp)) != NULL;
		cursor += strlen(cursor)){
		;
	}
	if (knob.close() == -1){
		return cursor-buf > 0? 1:
			-snprintf(buf, maxbuf, "ERROR on close");
	}
	return 1;

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
				peer_names->push_back(*new string(name));
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
}
Knob* Knob::create(const string _name, mode_t mode)
{
	const char* name = _name.c_str();
	if (HASX(mode)){
		return new KnobX(name);
	}else if (HASW(mode)){
		Knob* knob = new KnobRW(name);
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
		return knob;
	}else{
		return new KnobRO(name);
	}
}


class GroupKnob : public Knob {
	GroupKnob(string name) : Knob(name.c_str()) {}

	virtual int _set(char* buf, int maxbuf, const char* args)
	/* the peers do ALL the work */
	{}
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

class Prompt: public Knob
/* singleton */
{
	static bool enabled;

	Prompt(): Knob("prompt") {

	}
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

	static void prompt();

	static class Knob* create() {
		static Knob* _instance;
		if (!_instance){
			return _instance = new Prompt;
		}else{
			_instance;
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

class Help: public Knob {

protected:
	virtual int query(Knob* knob){
		printf("%s\n", knob->getName());
		return 1;
	}
	virtual int _set(char* buf, int maxbuf, const char* args) {
		for (VKI it = KNOBS.begin(); it != KNOBS.end(); ++it){
			if (Knob::match((*it)->getName(), args)){
				query(*it);
			}
		}
		return 1;
	}
public:
	Help() : Knob("help") {}
	Help(const char* _key) : Knob(_key) {}
	virtual int get(char* buf, int maxbuf) {
		for (VKI it = KNOBS.begin(); it != KNOBS.end(); ++it){
			query(*it);
		}
		return 1;
	}
};

class Help2: public Help {
protected:
	virtual int query(Knob* knob){
		char cmd[128];
		char reply[128];
		sprintf(cmd, "grep -m1 ^%s %s/acq400_help* | cut -f2 -",
					knob->getName(), HROOT);
		Pipe grep(cmd, "r");
		if (fgets(reply, 128, grep.fp)){
			printf("%-20s : %s\n\t%s",
				knob->getName(), knob->getAttr(), reply);
		}
		return 1;
	}
public:
	Help2() : Help("help2") {}
};


int filter(const struct dirent *dir)
{
        return fnmatch(pattern, dir->d_name, 0);
}
int do_scan()
{
	struct dirent **namelist;
	int nn = scandir(".", &namelist, filter, versionsort);

	if (nn < 0){
		perror("scandir");
		exit(1);
	}

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
			perror("stat");
		}else{
			if (!S_ISREG(sb.st_mode)){
				;
				//fprintf(stderr, "not a regular file:%s", alias);
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
	KNOBS.push_back(Prompt::create());
	KNOBS.push_back(new Help);
	KNOBS.push_back(new Help2);

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
}




void Prompt::prompt() {
	if (enabled){
		printf("acq400.%d %d >", site, err);
	}else if (err){
		;
	}
	fflush(stdout);
}


void cli(int argc, char* argv[])
{
	char *dir;
	char dbuf[128];
	char sname[32];

	if (getenv("VERBOSE")){
		verbose = atoi(getenv("VERBOSE"));
	}
	VPRINTF("%s verbose set %d\n", VERID, verbose);

	if (argc > 2){
		dir = argv[2];
	}else if (argc > 1){
		site = atoi(argv[1]);
		sprintf(dbuf, "/etc/acq400/%d", site);
		dir = dbuf;
		sprintf(sname, "%d", site);
	}else{
		return;
	}
	setenv("SITE", sname, 0);
	chdir(dir);
}
int main(int argc, char* argv[])
{
	cli(argc, argv);
	do_scan();
	char* ibuf = new char[128];
	char* obuf = new char[4096];


	for (; fgets(ibuf, 128, stdin); Prompt::prompt()){
		char *args = 0;
		int cursor;
		char *key = 0;
		bool is_query = false;

		chomp(ibuf);

		int len = strlen(ibuf);
		if (len == 0){
			continue;
		}else{
			int isep = strcspn(ibuf, "= ");
			if (isep != strlen(ibuf)){
				args = ibuf + isep+ strspn(ibuf+isep, "= ");
				ibuf[isep] = '\0';
			}else{
				is_query = true;
			}
			key = ibuf;
		}

		bool found = false;
		for (VKI it = KNOBS.begin(); it != KNOBS.end(); ++it){
			Knob* knob = *it;
			err = false;
			int rc;
			bool is_glob = true;
			obuf[0] = '\0';
			switch(Knob::match(knob->getName(), key)){
			case 0:
				continue;
			case 1:
				is_glob = false;
				it = KNOBS.end() - 1; // fall thru, drop out
			case -1:
				if (is_query){
					rc = knob->get(obuf, 4096);
					if (is_glob){
						if (!strstr(obuf, knob->getName())){
							printf("%s ", knob->getName());
						}
					}
				}else{
					rc = knob->set(obuf, 4096, args);
				}
				if (rc){
					puts(chomp(obuf));
				}

				err = rc < 0;
				found = true;
			}
		}

		if (!found){
			printf("ERROR:\%s\" not found\n", key);
		}
	}
}
