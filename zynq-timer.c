/* ------------------------------------------------------------------------- */
/* zynq-timer.c  		                     	 */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 pgm, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *   Created on: Jul 18, 2013
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * zynq-timer.c : temporary hack to measure the passage of time.
 *
 *  Created on: Jul 18, 2013
 *      Author: pgm
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/sizes.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>
#include <asm/io.h>

#define SLCR_XSCU 0xf8f00000
#define SLCR_XSCU_RANGE 0x00002000

#define XSCU_GTC0_COUNT0 0x0200
#define XSCU_GTC0_COUNT1 0x0204
#define XSCU_GTC0_CR 0x0208

static void *xscu;

unsigned long long _otick(void)
/*
To get the value from the Global Timer Counter
register proceed as follows:
1. Read the upper 32-bit timer counter register
2. Read the lower 32-bit timer counter register
3. Read the upper 32-bit timer counter register
again. If the value is different to the
32-bit upper value read previously, go back to
step 2. Otherwise the 64-bit time
*/
{
	u32 ctr12 = readl(xscu + XSCU_GTC0_COUNT1);
	u32 ctr0;
	u32 ctr1;
	unsigned long long llc;

	do {
		ctr1 = ctr12;
		ctr0 = readl(xscu + XSCU_GTC0_COUNT0);
		ctr12 = readl(xscu + XSCU_GTC0_COUNT1);
	} while (ctr1 != ctr12);

	llc = ctr1;
	llc <<= 32;
	llc |= ctr0;
	return llc;
}

void init_xscu(void)
{
	xscu = ioremap(SLCR_XSCU, SLCR_XSCU_RANGE);
}
unsigned long long otick(void)
{
	if (!xscu) {
		init_xscu();
	}
	return _otick();
}

/* zynq 333MHz clock -> 3nsec / tick */
unsigned delta_nsec(unsigned long long t0, unsigned long long t1)
{
	return ((unsigned)(t1 - t0)) * 3;
}
