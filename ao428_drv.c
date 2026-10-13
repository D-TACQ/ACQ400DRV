/* ------------------------------------------------------------------------- *
 * ao428_drv.c
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 7 Apr 2014  
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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/platform_data/pca953x.h>

#define REVID "0.001"

/* ao428 : dac celf + DAC20 : adt7410 at 0x48
 */

int I2C_CELF = 4;
module_param(I2C_CELF, int, 0444);

#define MAX_DEVICES 1

struct i2c_adapter *i2c_adap[1];
int nsites;

static struct i2c_client *new_device(struct i2c_adapter *adap, const char *name,
				     unsigned short addr)
{
	struct i2c_board_info info = {};

	strlcpy(info.type, name, I2C_NAME_SIZE);
	info.addr = addr;

	return i2c_new_device(adap, &info);
}
static int __init ao428_init_site(int site)
{
	i2c_adap[site] = i2c_get_adapter(I2C_CELF);

	printk("Hello i2c_adap[%d] = %p  I2C_CELF=%d\n", site, i2c_adap[site],
	       I2C_CELF);

	if (new_device(i2c_adap[site], "adt7410", 0x48) == 0) {
		printk("ao428_init_site(%d) ADT7410 NOT FOUND\n", site);
		return -1;
	}
	printk("Hello all good \n");
	return 0;
}

static void __init ao428_remove_site(int site)
{
	if (i2c_adap[0]) {
		i2c_put_adapter(i2c_adap[site]);
	}
}

static void __exit ao428_exit(void)
{
	ao428_remove_site(0);
}

static int __init ao428_init(void)
{
	return ao428_init_site(0);
}

module_init(ao428_init);
module_exit(ao428_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ AO428 20-bit DAC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
