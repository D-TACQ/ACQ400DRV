/* ------------------------------------------------------------------------- *
 * acq425_drv.c
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

#define REVID "0.006"

int acq425_sites[6] = {
	0,
};
int acq425_sites_count = 0;
module_param_array(acq425_sites, int, &acq425_sites_count, 0644);

static int n_acq425;
module_param(n_acq425, int, 0444);

static int single_pga_script = 0;
module_param(single_pga_script, int, 0644);

static int i2c_chan_offset = 1;
module_param(i2c_chan_offset, int, 0644);

/* adapters indexed by site .. use I2C_ADAPTER() to access */
struct i2c_adapter *__i2c_adap[6];

#define I2C_CHAN(site) ((site) + i2c_chan_offset)
#define I2C_ADAPTER(site) (__i2c_adap[(site) - 1])

#define PGA_TYPE "tca6424cr"
#define NGPIO_CHIP 24

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
static void __init acq425_init_site(int site)
{
	I2C_ADAPTER(site) = i2c_get_adapter(I2C_CHAN(site));

	if (new_device(I2C_ADAPTER(site), PGA_TYPE, 0x22, -1) == 0) {
		printk("acq425_init_site(%d) PGA1 NOT found\n", site);
	}
	if (single_pga_script) {
		return;
	}
	if (new_device(I2C_ADAPTER(site), PGA_TYPE, 0x23, -1) == 0) {
		printk("acq425_init_site(%d) PGA2 NOT found\n", site);
	}
}

static void __init acq425_remove_site(int site)
{
	printk("acq425_remove_site %d channel %d\n", site, I2C_CHAN(site));
	i2c_put_adapter(I2C_ADAPTER(site));
}

static void __exit acq425_exit(void)
{
	for (; n_acq425--;) {
		acq425_remove_site(acq425_sites[n_acq425]);
	}
}

static int __init acq425_init(void)
{
	int status = 0;

	printk("D-TACQ ACQ425 i2c Driver %s\n", REVID);

	for (n_acq425 = 0; n_acq425 < acq425_sites_count; ++n_acq425) {
		acq425_init_site(acq425_sites[n_acq425]);
	}
	return status;
}

module_init(acq425_init);
module_exit(acq425_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ425ELF i2c Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
