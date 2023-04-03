/* ------------------------------------------------------------------------- */
/* acq400_includes.h  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 9 Aug 2018  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2018 Peter Milne, D-TACQ Solutions Ltd         *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO 
 * TODO
 * ------------------------------------------------------------------------- */

#ifndef ACQ400_INCLUDES_H_
#define ACQ400_INCLUDES_H_

#include <asm/uaccess.h>
#include <asm/sizes.h>


#include "include/linux/dmaengine.h"
#include <linux/kernel.h>
#include <linux/kthread.h>
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
#include <linux/spinlock.h>

#include <linux/circ_buf.h>
#include <linux/debugfs.h>
#include <linux/poll.h>
//#include <mach/pl330.h>
#include "include/linux/amba/pl330.h"
#include <linux/of.h>
#include <linux/types.h>
#include <linux/cdev.h>

#include <asm/barrier.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>
#include <asm/io.h>

/*
 *  Minor encoding
 *  0 : the original device
 *  100..164 : buffers
 *  200..231 : channels when available
 */
#define ACQ420_MINOR_0	        0
#define ACQ420_MINOR_CONTINUOUS	1
#define ACQ420_MINOR_HISTO	2
#define ACQ400_MINOR_HB0	3	// block on HB0 fill
#define ACQ420_MINOR_SIDEPORTED 4	// enable, but data flow elsewhere
#define ACQ400_MINOR_GPGMEM	5	// mmap 4K of this
#define ACQ400_MINOR_EVENT	6	// blocks on event
#define ACQ400_MINOR_STREAMDAC	7
#define ACQ400_MINOR_BOLO_AWG	8
#define AO420_MINOR_HB0_AWG_ONCE	9
#define AO420_MINOR_HB0_AWG_LOOP	10
#define ACQ400_MINOR_RESERVE_BLOCKS	11
#define ACQ400_MINOR_SEW1_FIFO	12		/* Software Embedded Word */
#define ACQ400_MINOR_SEW2_FIFO	13
#define AO420_MINOR_HB0_AWG_ONCE_RETRIG	14
#define ACQ400_MINOR_BQ_NOWAIT	15
#define ACQ400_MINOR_ATD	16
#define ACQ400_MINOR_BQ_FULL	17
#define ACQ400_MINOR_RSV_DIST	18
#define ACQ400_MINOR_AXI_DMA_ONCE 19
#define ACQ400_MINOR_WR_TS	20	// read : u32 WR_TAI_STAMP
#define ACQ400_MINOR_WR_PPS	21      // read : u32 WR_TAI_CUR_L
#define ACQ400_MINOR_WR_CUR	22	// read : u32 WR_
#define ACQ400_MINOR_WRTT	23	// read : u32 WR_TT CUR time
#define ACQ400_MINOR_WR_CUR_TAI 24	// read : u32 WR_TAI_CUR_L no block
#define ACQ400_MINOR_WR_CUR_TRG0 25	// read : u32 WR_TAI_TRG0, no block.  write: u32 WR_TAI_TRG0
#define ACQ400_MINOR_WR_CUR_TRG1 26	// read : u32 WR_TAI_TRG1, no block.  write: u32 WR_TAI_TRG1
#define ACQ400_MINOR_WRTT1	27
#define ACQ400_MINOR_GPGMEM32	28
#define ACQ400_MINOR_STREAMDAC_DUMMY	37
#define ACQ400_MINOR_AOFIFO	38
#define ACQ400_MINOR_ADC_SAMPLE_COUNT 39
#define ACQ400_MINOR_ADC_NACC_SUBRATE 40
#define ACQ400_MINOR_AWG_ABCDE  41

#define ACQ400_MINOR_MAP_PAGE	64	// 32 : page 0, 33: page 1 .. 47: page 15

#define ACQ400_MINOR_MAP_PAGE_OFFSET(minor) \
	((minor-ACQ400_MINOR_MAP_PAGE)*PAGE_SIZE)
#define ACQ400_MINOR_BUF	1000
#define ACQ400_MINOR_BUF2	2000
#define ACQ420_MINOR_MAX	ACQ400_MINOR_BUF2
#define ACQ420_MINOR_CHAN	200
#define ACQ420_MINOR_CHAN2	232	// in reality 203 of course, but looking ahead ..

#define ACQ420_MINOR_TIGA_TS_1	120
#define ACQ420_MINOR_TIGA_TT_1	130
#define ACQ420_MINOR_TIGA_TTB_1 140
#define ACQ420_MINOR_TIGA_99	150

#define MINOR_IS_TIGA_TS(minor) \
	()


#define BQ_MIN_BACKLOG		2
#define BQ_MAX_BACKLOG		512

#define IS_BUFFER(minor) \
	((minor) >= ACQ400_MINOR_BUF && (minor) <= ACQ400_MINOR_BUF2)
#define BUFFER(minor) 		((minor) - ACQ400_MINOR_BUF)

#endif /* ACQ400_INCLUDES_H_ */
