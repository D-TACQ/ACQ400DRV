/* 
 * reset state to 0
 * sed -ie 's/^[0-9]/0/' /dev/shm/state   breaks inotifywait
 * so we do it the unobtrusive way.
 * NB: STATE MUST be a single char at [0]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF	"/dev/shm/state"

#define MAXLEN 128
#define DEFSTR "STX 0 0 0 0 0"

int main(int argc, char* argv[])
{
	FILE *fp = fopen(SF, "r+");
	int args[5];
	char data[MAXLEN];
	if (!fp){
		perror(SF);
		return 1;
	}
	fgets(data, MAXLEN, fp);
	if (sscanf(data, "STX %d %d %d %d %d", args+0, args+1, args+2, args+3, args+4) == 5){
		// ensure first field is 0 aka STATE=IDLE
		sprintf(data, "STX %d %d %d %d %d", 0, args[1], args[2], args[3], args[4]);
	}else{
		strcpy(data, DEFSTR);
	}
	rewind(fp);
	fputs(data, fp);
	fclose(fp);
	return 0;
}
