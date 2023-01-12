/* print hudp status from site 10
 */
#include <stdio.h>
#include <stdlib.h>
#include "knobs.h"

int main(int argc, char* argv[])
{
	unsigned status;
	getKnob(10, "hudp_status", &status, "%x");

	printf("%30s 0x%08x\n", "STATUS", 	status);
	printf("%30s %s\n", "DUPLEX_MODE", 	(status&(1<<28))? "FULL": "HALF");
	printf("%30s %s\n", "SPEED",       	(status&(3<<26)) == (2<<26)? "1000": "100");
	printf("%30s %d\n", "PHY_LINK_STATUS",  (status&(1<<23)) != 0);
	printf("%30s %d\n", "RUDI INVALID",	(status&(1<<20)) != 0);
	printf("%30s %d\n", "RUDI IDLES",	(status&(1<<19)) != 0);
	printf("%30s %d\n", "RUDI CONF",	(status&(1<<18)) != 0);
	printf("%30s %d\n", "LINK_SYNC",	(status&(1<<17)) != 0);
	printf("%30s %d\n", "LINK_STATUS",	(status&(1<<16)) != 0);
	printf("%30s %d\n", "READY_FOR_HDR",	(status&( 1<<6)) != 0);
	printf("%30s %d\n", "READY_FOR_DATA",	(status&( 1<<5)) != 0);
	printf("%30s %d\n", "TX FIFO EMPTY",	(status&( 1<<4)) != 0);

	const char* tx_st[4] = { "IDLE", "WRITE_HDR", "WRITE_PKT", "PAUSE/LOOP" };
	printf("%30s %s\n", "TX_STATUS",	tx_st[status&(3<<0)]);
	return 0;
}
