/*
 * dump_regs.c
 *
 *  Created on: 4 Sep 2019
 *      Author: pgm
 */

// replace dump_regs script: taking 3s on acq400.0


		/* enable versionsort */


#define _FILE_OFFSET_BITS 64

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>


static const char* pattern = "*0x*";

int filter(const struct dirent *dir)
{
        return fnmatch(pattern, dir->d_name, 0) == 0;
}

int regsort(const struct dirent **a, const struct dirent **b)
{
	return (*a)->d_ino  > (*b)->d_ino;
}
void scancat(const char* dir)
{
	struct dirent **namelist;
	int nn = scandir(dir, &namelist, filter, regsort);
	for (int ii = 0; ii < nn; ++ii){
		const char* kname = namelist[ii]->d_name;
		char path[1024];
		snprintf(path, 1024, "%s/%s", dir, kname);
		FILE* fp = fopen(path, "r");
		if (fp){
			char value[80];
			printf( "%34s %s", kname, fgets(value, 80, fp));
			fclose(fp);
		}
	}
}

int main(int argc, char* argv[])
{
	if (getenv("DUMP_REGS_PATTERN")){
		pattern = getenv("DUMP_REGS_PATTERN");
	}

	for (int ii = 1; ii < argc; ++ii){
		scancat(argv[ii]);
	}
}

