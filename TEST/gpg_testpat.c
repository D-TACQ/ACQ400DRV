/* simple gpgtest pattern */

#include <stdio.h>

#define STEP 100
#define NELEMS 40

int main(int argc, char *argv[])
{
	int ii;
	int state = 0;
	unsigned opcode;

	for (ii = 0; ii < NELEMS; ++ii) {
		opcode = ((ii * STEP) << 8) | state;
		fwrite(&opcode, sizeof(unsigned), 1, stdout);
		state = ++state & 0xf;
	}
}
