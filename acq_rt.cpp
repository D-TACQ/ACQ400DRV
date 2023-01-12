/*
 * acq_rt.cpp
 *
 *  Created on: 8 Jan 2021
 *      Author: pgm
 */

#include <stdio.h>
#include <stdlib.h>

#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "acq-util.h"

void goRealTime(int sched_fifo_priority)
{
        struct sched_param p = {};
        p.sched_priority = sched_fifo_priority;

        int rc = sched_setscheduler(0, SCHED_FIFO, &p);

        if (rc){
                perror("failed to set RT priority");
        }
}



int getenv_default(const char* key, int def){
	const char* vs = getenv(key);
	if (vs){
		return atoi(vs);
	}else{
		return def;
	}
}


int getBufferId(int fc) {
	char buf[80];
	int rc = read(fc, buf, 80);

	if (rc > 0){
		buf[rc] = '\0';
		unsigned ib = strtoul(buf, 0, 10);
		assert(ib >= 0);
//		assert(ib <= Buffer::nbuffers);
		return ib;
	}else{
		return rc<0 ? rc: -1;
	}
}

char *getRoot(int devnum)
{
	char *_root = new char [128];
	struct stat sb;

	sprintf(_root, "/dev/acq420.%d", devnum);
	if (stat(_root, &sb) == 0){
		return _root;
	}

	sprintf(_root, "/dev/acq400.%d", devnum);
	if (stat(_root, &sb) == 0){
		return _root;
	}

	fprintf(stderr, "ERROR: /dev/acq4x0.%d NOT FOUND\n", devnum);
	exit(1);
}
