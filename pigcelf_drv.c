/* ------------------------------------------------------------------------- *
 * pigcelf_drv.c
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 16 October 2014
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
 * instantiates pigcelf i2c device (50R termination)
 * also creates a device driver hook for the spi buffer
 * pigcelf_knobs app mmaps this buffer and updates it.
 * a task on a poll loop in this driver monitors changes and sends them to SPI
 *
 * IMU ADIS 16448.
 * CONFIG_ADIS16400=m
 * How to hook it ?

 */
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/spi/spi.h>

//#include <linux/i2c/pca953x.h>
#include <linux/platform_device.h>
#include <linux/platform_data/pca953x.h>

#include <linux/dma-mapping.h>
#include "hbm.h"

#define REVID "5"
#define MODULE_NAME "pigcelf"

#define PIG_CELF_FPGA_SITE 2 /* site 2 in FPGA memory map */
#define I2C_CHAN(site) ((site) + 2) /* CELF BUS = 4 */

int site2cs(int site)
{
	return site - 1;
}
int cs2site(int cs)
{
	return cs + 1;
}

struct pigcelf_dev {
	struct platform_device *pdev;
	struct i2c_adapter *i2c_adapter;
	char devname[16];

	struct spi_device *spi;
};

#define DEVP(adev) (&adev->pdev->dev)

static struct proc_dir_entry *pigcelf_proc_root;
static struct pigcelf_dev *pigcelf_devs[7]; /* 6 sites index from 1 */

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

#define pigcelf_of_match 0

struct pigcelf_dev *pigcelf_allocate_dev(struct platform_device *pdev)
{
	int site = pdev->id;
	struct pigcelf_dev *adev =
		kzalloc(sizeof(struct pigcelf_dev), GFP_KERNEL);

	if (adev == NULL) {
		return NULL;
	}
	pigcelf_devs[site] = adev;

	adev->pdev = pdev;
	adev->i2c_adapter = i2c_get_adapter(I2C_CHAN(site));

	if (new_device(adev->i2c_adapter, "ad7417", 0x29, -1) == 0) {
		printk("pigcelf_init_site(%d) ad7417#2 NOT found\n", site);
	}

	snprintf(adev->devname, 16, "%s.%d", pdev->name, pdev->id);

	return adev;
}

static int pigcelf_probe(struct platform_device *pdev)
{
	struct pigcelf_dev *adev = pigcelf_allocate_dev(pdev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "pigcelf_probe: %d %s", pdev->id, adev->devname);

	return 0;
}

static int pigcelf_remove(struct platform_device *pdev)
{
	int site = pdev->id;

	i2c_put_adapter(pigcelf_devs[site]->i2c_adapter);
	kfree(pigcelf_devs[site]);
	pigcelf_devs[site] = 0;
	return -1;
}

extern int zynq_spi_goslow_usec_kludge;

static void pigcelf_init_site(int site)
{
	static struct spi_board_info imu = {
		.modalias = "adis16448",
		.max_speed_hz = 1000000,
		.bus_num = 1,
		.chip_select = 1,
		.platform_data = NULL, /* No spi_driver specific config */
		.irq = 0x42,
	};

	struct platform_device *pdev =
		kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	pdev->name = MODULE_NAME;
	pdev->id = site;
	platform_device_register(pdev);

	zynq_spi_goslow_usec_kludge = 0;
	dev_info(&pdev->dev,
		 "pigcelf_init_site() zynq_spi_goslow_msec_kludge %d",
		 zynq_spi_goslow_usec_kludge);
	dev_info(&pdev->dev, "pigcelf_init_site() irq %u", imu.irq);
	dev_info(&pdev->dev, "pigcelf_init_site() set max_speed_hz %u",
		 imu.max_speed_hz);

	imu.chip_select = site2cs(site);
	spi_register_board_info(&imu, 1);
}

static void __init pigcelf_remove_site(int site)
{
	printk("pigcelf_remove_site %d ERROR removal not supported\n", site);
}
static struct platform_driver pigcelf_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = pigcelf_of_match,
        },
        .probe = pigcelf_probe,
        .remove = pigcelf_remove,
};

static void __exit pigcelf_exit(void)
{
	pigcelf_remove_site(PIG_CELF_FPGA_SITE);
	platform_driver_unregister(&pigcelf_driver);
}

extern void acq480_hook_spi(void);

static int __init pigcelf_init(void)
{
	int status = 0;

	printk("D-TACQ pigcelf Driver %s\n", REVID);

	platform_driver_register(&pigcelf_driver);
	pigcelf_proc_root = proc_mkdir("driver/pigcelf", 0);

	acq480_hook_spi();
	pigcelf_init_site(PIG_CELF_FPGA_SITE);
	return status;
}

module_init(pigcelf_init);
module_exit(pigcelf_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ pigcelf i2c/spi Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
