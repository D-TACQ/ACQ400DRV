/* ------------------------------------------------------------------------- *
 * acq400_hook_spi_cs.c : hook the acq400 spi cs for use with SPIDEV
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2021 Peter Milne, D-TACQ Solutions Ltd
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 28 September 2021
 *    Author: pgm                                                         
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

#include <linux/kernel.h>

#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>


#define REVID 		"0.1.0"
#define MODULE_NAME	"acq426"

extern void acq480_hook_spi_cb(void);
extern void acq480_hook_spi(void);

int cb = 0;
module_param(cb, int, 0444);
MODULE_PARM_DESC(cb, "select  0: regular single step site cs, 1: two step chip-board cs");

static int __init acq400_hook_spi_cs_init(void)
{
	int status = 0;


	printk("D-TACQ ACQ400 SPI CS Hook %s cb=%d\n", REVID, cb);

	if (cb){
		acq480_hook_spi_cb();
	}else{
		acq480_hook_spi();
	}


	return status;
}

static void __exit acq400_hook_spi_cs_exit(void){

}

module_init(acq400_hook_spi_cs_init);
module_exit(acq400_hook_spi_cs_exit);



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400 SPI CS Hook");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
