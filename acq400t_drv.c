/* ------------------------------------------------------------------------- *
 * acq400t_drv.c
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

#define REVID "0.007"

int acq400t_sites[6] = {
	0,
};
int acq400t_sites_count = 0;
module_param_array(acq400t_sites, int, &acq400t_sites_count, 0644);

int site2channel = 1;
module_param(site2channel, int, 0444);
MODULE_PARM_DESC(site2channel,
		 "offset from site to i2c bus# for MTCA test set 3");

struct i2c_adapter *i2c_adap[7]; /* index from one */

static const unsigned short gpio_addr[] = { 0x20, I2C_CLIENT_END };

static int ntest;

#define GPIO_TYPE "pca9534"
#define GPIO_ADDR 0x20
#define N_GPIO_GPIO 8

static struct i2c_client *new_device(struct i2c_adapter *adap, const char *name,
				     unsigned short addr, int gpio_base)
{
	struct pca953x_platform_data pca_data = {};
	struct i2c_board_info info = {};

	strlcpy(info.type, name, I2C_NAME_SIZE);
	info.addr = addr;
	pca_data.gpio_base = gpio_base;
	pca_data.irq_base = -1;
	info.platform_data = &pca_data;
	return i2c_new_device(adap, &info);
}
static void __init acq400t_init_site(int site)
{
	int ch = site + site2channel;

	i2c_adap[site] = i2c_get_adapter(ch);

	if (new_device(i2c_adap[site], GPIO_TYPE, GPIO_ADDR, -1) == 0) {
		printk("acq400t_init_site(%d) GPIO NOT found\n", site);
	}
}

static void __init acq400t_remove_site(int site)
{
	int ch = site + site2channel;
	printk("acq400t_init_site %d channel %d\n", site, ch);
	i2c_put_adapter(i2c_adap[site]);
}

static void __exit acq400t_exit(void)
{
	for (; ntest--;) {
		acq400t_remove_site(acq400t_sites[ntest]);
	}
}

static int __init acq400t_init(void)
{
	int status = 0;

	printk("D-TACQ ACQ400TEST Driver %s\n", REVID);

	for (ntest = 0; ntest < acq400t_sites_count; ++ntest) {
		acq400t_init_site(acq400t_sites[ntest]);
	}
	return status;
}

module_init(acq400t_init);
module_exit(acq400t_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400TEST_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
