/* ------------------------------------------------------------------------- *
 * acq428_drv.c  		                     	                    
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2026 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 6 Feb 2026  
 *    Author: cph                                                         
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

#define REVID "0.1.0"

/* define ARCH_NR_GPIOS		256
 * we're going to run out with multiple devS, so we really have to increase
 * that number ...
 */
int acq428_gpio_base = 0;
module_param(acq428_gpio_base, int, 0644);

int acq428sites[6] = { 0,  };
int acq428sites_count = 0;
module_param_array(acq428sites, int, &acq428sites_count, 0644);

struct i2c_adapter *i2c_adap[7];   /* index by site 1..6 from zero */

static int ndev;

#define PGA_TYPE	"tca6424"
#define PGA_ADDR_1	0x22
#define PGA_ADDR_2      0x23
#define N_PGA_GPIO 24

extern void acq480_hook_spi(void);

static struct i2c_client* new_device(
		struct i2c_adapter *adap,
		const char* name, unsigned short addr, int gpio_base)
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
static void __init acq428_init_site(int site)
{
	int ch = site+1;
	int gpio_base = acq428_gpio_base + ndev * N_PGA_GPIO;
        acq480_hook_spi();

	i2c_adap[site] = i2c_get_adapter(ch);

	if (new_device(i2c_adap[site], PGA_TYPE, PGA_ADDR_1, gpio_base) == 0){
		printk("acq428_init_site(%d) PGA NOT found\n", site);
	}
	if (new_device(i2c_adap[site], PGA_TYPE, PGA_ADDR_2, gpio_base + N_PGA_GPIO) == 0){
		printk("acq428_init_site(%d) PGA NOT found\n", site);
	}

}

static void __init acq428_remove_site(int site)
{
	int ch = site+1;
	printk("acq428_init_site %d channel %d\n", site, ch);
	i2c_put_adapter(i2c_adap[site]);
}

static void __exit acq428_exit(void)
{
	for (; ndev--;){
		acq428_remove_site(acq428sites[ndev]);
	}
}


static int __init acq428_init(void)
{
        int status = 0;

	printk("D-TACQ ACQ428ELF Driver %s\n", REVID);

	for (ndev = 0; ndev < acq428sites_count; ++ndev){
		acq428_init_site(acq428sites[ndev]);
	}
        return status;
}

module_init(acq428_init);
module_exit(acq428_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
