/* converts egu to integer */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

int egu2int(char *results, int cursor, const char *kvp)
{
	char key[80];
	float val;
	char modifier;
	int multiplier = 1;
	const char *sep = index(kvp, '=');
	const char *vp;

	if (sep) {
		strncpy(key, kvp, sep - kvp);
		key[sep - kvp] = '\0';
		vp = sep + 1;
	} else {
		vp = kvp;
	}
	int nscan = sscanf(vp, "%f%c", &val, &modifier);

	if (nscan >= 1) {
		if (cursor) {
			results[cursor++] = ' ';
		}
		if (nscan == 2) {
			switch (modifier) {
			case 'k':
			case 'K':
				multiplier = 1000;
				break;
			case 'M':
				multiplier = 1000000;
				break;
			default:
				fprintf(stderr, "ERROR %s bad modifier %c\n",
					kvp, modifier);
			}
		}
		if (sep) {
			cursor += sprintf(results + cursor, "%s=%d", key,
					  (int)(val * multiplier));
		} else {
			cursor += sprintf(results + cursor, "%d",
					  (int)(val * multiplier));
		}
		return cursor;
	} else {
		fprintf(stderr, "ERROR scan %s failed %d\n", kvp, nscan);
		exit(1);
	}
}

int main(int argc, const char *argv[])
{
	int ii;
	char *results = calloc(4096, 1);
	int cursor = 0;

	for (ii = 1; ii < argc; ++ii) {
		cursor += egu2int(results, cursor, argv[ii]);
	}
	printf("%s\n", results);
	return 0;
}
