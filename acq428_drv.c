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
#define MAX_SITES 6

struct acq428_site_data {
    struct i2c_adapter *adap;
    struct i2c_client *client1;
    struct i2c_client *client2;
    struct pca953x_platform_data pdata1;
    struct pca953x_platform_data pdata2;
};

static struct acq428_site_data site_data[MAX_SITES];
static int ndev;

static void __init acq428_init_site(int site)
{
    struct acq428_site_data *sd;
    struct i2c_board_info info1 = {};
    struct i2c_board_info info2 = {};
    int ch = site + 1;
    int gpio_base = acq428_gpio_base + (ndev * N_PGA_GPIO * 2);
    if (site < 0 || site > MAX_SITES) {
        pr_err("acq428: Site %d is out of bounds\n", site);
        return;
    }

    sd = &site_data[site];
    sd->adap = i2c_get_adapter(ch);
    if (!sd->adap) {
        pr_err("acq428: i2c adapter %d not found for site %d\n", ch, site);
        return;
    }

    // Setup persistent platform data
    sd->pdata1.gpio_base = gpio_base;
    sd->pdata1.irq_base = -1;

    sd->pdata2.gpio_base = gpio_base + N_PGA_GPIO;
    sd->pdata2.irq_base = -1;

    // Initialise PGA 1
    strlcpy(info1.type, PGA_TYPE, I2C_NAME_SIZE);
    info1.addr = PGA_ADDR_1;
    info1.platform_data = &sd->pdata1;
    sd->client1 = i2c_new_device(sd->adap, &info1);
    if (!sd->client1)
        pr_err("acq428_init_site(%d) PGA 1 not found\n", site);

    // Initialise PGA 2
    strlcpy(info2.type, PGA_TYPE, I2C_NAME_SIZE);
    info2.addr = PGA_ADDR_2;
    info2.platform_data = &sd->pdata2;
    sd->client2 = i2c_new_device(sd->adap, &info2);
    if (!sd->client2)
        pr_err("acq428_init_site(%d) PGA 2 not found\n", site);
}


static void __init acq428_remove_site(int site)
{
    struct acq428_site_data *sd;
    if (site < 0 || site > MAX_SITES)
        return;

    sd = &site_data[site];
    pr_info("acq428_remove_site %d channel %d\n", site, site + 1);
    
    if (sd->client1)
        i2c_unregister_device(sd->client1);
    if (sd->client2)
        i2c_unregister_device(sd->client2);
    if (sd->adap)
        i2c_put_adapter(sd->adap);
}


static void __exit acq428_exit(void)
{
	for (; ndev--;){
		acq428_remove_site(acq428sites[ndev]);
	}
}


static int __init acq428_init(void)
{

	pr_info("D-TACQ ACQ428ELF Driver %s\n", REVID);

	for (ndev = 0; ndev < acq428sites_count; ++ndev){
		acq428_init_site(acq428sites[ndev]);
	}
        return 0;
}

module_init(acq428_init);
module_exit(acq428_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ400_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
