/*
 * acq465_ioctl.h
 *
 *  Created on: 28 September 2021
 *      Author: pgm
 */

#ifndef ACQ465_IOCTL_H_
#define ACQ465_IOCTL_H_

#include <linux/ioctl.h>

#define MAGIC	0xa465

#define LCS	unsigned			/* local chip select 0..7 */


struct MCM {
	unsigned short lcs;		// W : chip select
	unsigned short sec;		// W : sec to hold for
	unsigned count;			// R : MCLK count for period
};

#define ACQ465_RESET		_IOW(MAGIC, 1, LCS)
#define ACQ465_CACHE_INVALIDATE	_IOW(MAGIC, 2, LCS)
#define ACQ465_CACHE_FLUSH	_IOW(MAGIC, 3, LCS)
#define ACQ465_MCLK_MONITOR	_IOWR(MAGIC, 4, struct MCM)   /* LCS out, count back */
#define ACQ465_DIG_IF_RESET	_IOWR(MAGIC, 5,LCS)

#define REGS_LEN	0x80
#define NCHIPS		8
#define MODULE_SPI_BUFFER_LEN 	(NCHIPS*REGS_LEN)					// 1K
//#define TOTAL_SPI_BUFFER_LEN	((acq465_sites_count&~0x3)+4)*MODULE_SPI_BUFFER_LEN

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define TOTAL_SPI_BUFFER_LEN	(2*PAGE_SIZE)

#endif /* ACQ465_IOCTL_H_ */
