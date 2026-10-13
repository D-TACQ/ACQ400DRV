/*
 * regmap.c : demonstrate memory mapping regs
 *
 *  Created on: 30 May 2019
 *      Author: pgm
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SITE0 0x40000000
#define DSP0 0x80000000

#define SPAD1 (0x84 / sizeof(unsigned))

#define MODE_CACHE 0
#define MODE_LOAD_STORE 1
#define MODE_STORE 2

int main(int argc, char **argv)
{
	unsigned offset = SITE0;
	unsigned length = 0x4000;
	int fd = open("/dev/mem", O_RDWR);
	volatile unsigned *regs;
	volatile int spad1;
	int MODE = MODE_CACHE;

	if (getenv("MODE") != 0) {
		MODE = atoi(getenv("MODE"));
	}

	assert(fd > 0);

	if (argc > 1) {
		offset = strtoul(argv[1], 0, 0);
	}
	if (argc > 2) {
		length = strtoul(argv[2], 0, 0);
	}

	regs = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	assert(regs != MAP_FAILED);

	switch (MODE) {
	case MODE_CACHE:
		spad1 = 0;

		do {
			spad1++;
		} while (spad1 != 0);
		break;
	case MODE_LOAD_STORE:

		do {
			spad1 = regs[SPAD1] += 1;
		} while (spad1 != 0);
		break;

	case MODE_STORE:
		spad1 = 0;
		do {
			++spad1;
			regs[SPAD1] = spad1;
		} while (spad1 != 0);
		break;

	default:
		fprintf(stderr, "ERROR: BAD MODE\n");
		return -1;
	}

	return 0;
}
