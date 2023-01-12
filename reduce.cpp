#include <stdio.h>
#include <stdlib.h>


int reduce(unsigned in_count, unsigned out_count)
{
	short* buf = new short[in_count];
	while(fread(buf, sizeof(short), in_count, stdin) == in_count){
		fwrite(buf, sizeof(short), out_count, stdout);
	}
	return 0;
}
int main(int argc, char* argv[])
{
	int in_count = argc>1? 	 strtoul(argv[1],0,0): 32;
	int out_count = argc >2? strtoul(argv[2],0,0): 4;

	return reduce(in_count, out_count);
}
