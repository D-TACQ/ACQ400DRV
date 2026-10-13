/* testharness for dmadescfs */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "dmadescfs_ioctl.h"

#define MAPLEN 32768

void *get_mapping(int fd, int mmap_mode)
{
	void *region = mmap(NULL, MAPLEN, mmap_mode, MAP_SHARED, fd, 0);
	if (region == (caddr_t)-1) {
		perror("mmap failed");
		exit(-1);
	}
	return region;
}

void report_pa(int fd)
{
	unsigned pa = 0;
	int rc = ioctl(fd, DD_GETPA, &pa);

	if (rc == 0) {
		fprintf(stderr, "report_pa: 0x%08x\n", pa);
	} else {
		perror("ioctl DD_GETPA failed");
		exit(-1);
	}
}

int write_test(char *fname)
{
	int fd;
	unsigned *buf;
	int ii;

	fprintf(stderr, "write_test %s\n", fname);
	fd = open(fname, O_RDWR);
	if (fd < 0) {
		perror("write_test bad device");
		exit(-1);
	}
	buf = get_mapping(fd, PROT_WRITE | PROT_READ);
	report_pa(fd);

	buf[0] = getpid();
	for (ii = 1; ii < 512 / sizeof(unsigned); ++ii) {
		buf[ii] = ii;
	}
	if (ioctl(fd, DD_TODEV, 0) != 0) {
		perror("ioctl DD_TOCPU FAIL");
		exit(1);
	}
	return 0;
}

int read_test(char *fname)
{
	int fd;
	char *buf;

	fprintf(stderr, "read_test %s\n", fname);
	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("read_test bad device");
		exit(-1);
	}
	buf = get_mapping(fd, PROT_READ);
	report_pa(fd);
	if (ioctl(fd, DD_TOCPU, 0) != 0) {
		perror("ioctl DD_TOCPU FAIL");
		exit(1);
	}

	write(1, buf, 512);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc >= 2) {
		int write_mode = 0;
		if (argc >= 3) {
			write_mode = strcmp(argv[2], "write") == 0;
		}
		if (write_mode) {
			return write_test(argv[1]);
		} else {
			return read_test(argv[1]);
		}
	}
	return 1;
}
