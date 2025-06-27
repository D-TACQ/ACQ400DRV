/* ------------------------------------------------------------------------- *
 * acq426_drv.c cloned from acq465_drv.c
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

/*
 * vestigial: needs to hook the "chip/board" CS call back. Real work is done by ADI driver.
 */
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


static int __init acq426_init(void)
{
	int status = 0;


	printk("D-TACQ ACQ426 Driver %s\n", REVID);

	acq480_hook_spi_cb();

	return status;
}

static void __exit acq426_exit(void){

}

module_init(acq426_init);
module_exit(acq426_exit);



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ465ELF SPI Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
