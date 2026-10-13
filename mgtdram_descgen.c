#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "popt.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DESCLEN 0x400000
#define SCALE 12
#define ID 8
#define ID_MASK 0x00ff

int fd_out = 1;

int descgen1(int dx)
{
	unsigned block = DESCLEN >> SCALE;
	unsigned addr = block * dx;
	unsigned descr = addr << ID | (dx & ID_MASK);
	if (write(fd_out, &descr, sizeof(descr)) != sizeof(descr)) {
		perror("write");
		exit(1);
	}
	return 0;
}
int descgen(int d0, int d1)
{
	int dx;
	for (dx = d0; dx <= d1; ++dx) {
		descgen1(dx);
	}
	return 0;
}

char *device;
int G_d0;
int G_d1;

struct poptOption opt_table[] = { { "file", 'f', POPT_ARG_STRING, &device,
				    'f' },
				  POPT_AUTOHELP POPT_TABLEEND };
int ui(int argc, const char *argv[])
{
	poptContext opt_context =
		poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	const char **args;
	int nargs;

	while ((rc = poptGetNextOpt(opt_context)) > 0) {
		switch (rc) {
		case 'f':
			fd_out = open(device, O_WRONLY);
			if (fd_out == -1) {
				perror(device);
				return (1);
			}
		default:;
		}
	}

	args = poptGetArgs(opt_context);
	for (nargs = 0; args[nargs] != NULL; ++nargs) {
		;
	}

	if (nargs == 2) {
		return descgen(atoi(args[0]), atoi(args[1]));
	} else {
		int ii;

		for (ii = 0; ii < nargs; ++ii) {
			int to, from;
			int x1, x2, x3;
			int nseq;
			if (sscanf(args[ii], "%d-%d", &from, &to) == 2 ||
			    sscanf(args[ii], "%d:%d", &from, &to) == 2) {
				descgen(from, to);
			} else if ((nseq = sscanf(args[ii], "%d,%d,%d", &x1,
						  &x2, &x3)) > 0) {
				if (nseq > 0)
					descgen1(x1);
				if (nseq > 1)
					descgen1(x2);
				if (nseq > 2)
					descgen1(x3);
			} else {
				descgen1(atoi(args[ii]));
			}
		}
	}
	return 0;
}
int main(int argc, const char *argv[])
{
	return ui(argc, argv);
}
