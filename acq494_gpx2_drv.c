/*
 * acq494_gpx2_drv.c
 *
 *  Created on: 31 May 2022
 *      Author: pgm
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
#include <linux/platform_device.h>
#include <linux/platform_data/pca953x.h>

#define REVID 		"0.3.0"
#define MODULE_NAME	"acq494"

extern void acq480_hook_spi(void);


int acq494_sites[6] = { 0,  };
int acq494_sites_count = 0;
module_param_array(acq494_sites, int, &acq494_sites_count, 0644);

static int n_acq494;
module_param(n_acq494, int, 0444);

static int claim_0x20 = 1;
module_param(claim_0x20, int, 0444);

static int spi_bus_num = 1;
module_param(spi_bus_num, int, 0444);

static int SPI_MODE = SPI_MODE_1;
module_param(SPI_MODE, int, 0644);

static int do_signon;
module_param(do_signon, int, 0644);

int site2cs(int site)
{
	return site - 1;
}
int cs2site(int cs)
{
	return cs + 1;
}

#define TDC_WRITE_CONFIG 0x80
#define TDC_READ_RESULT	 0x60
#define TDC_READ_CONFIG	 0x40
#define TDC_POWER	 0x30
#define TDC_INIT	 0x18



int tdc_gpx2_spi_write_then_read(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	int rc;
	spi->mode &= ~SPI_MODE_3;
	spi->mode |= SPI_MODE;

	rc = spi_write_then_read(spi, txbuf, n_tx, rxbuf, n_rx);
	if (rc == 0){
		if (n_rx){
			dev_dbg(&spi->dev, "tdc_gpx2_spi_write_then_read() m:%d 0x%02x R %2d = 0x%02x", spi->mode&SPI_MODE_3, *(char*)txbuf, *(char*)txbuf&0x1f, *(char*)rxbuf);
		}else{
			dev_dbg(&spi->dev, "tdc_gpx2_spi_write_then_read() m:%d 0x%02x W %2d", spi->mode&SPI_MODE_3, *(char*)txbuf, *(char*)txbuf&0x1f);
		}
		return 0;
	}else{
		dev_err(&spi->dev, "tdc_gpx2_spi_write_then_read() fail");
		return rc;
	}
}

ssize_t store_byte(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const unsigned REG)
{
	char tmps[16];
	char data[2];
	unsigned long tmp;
	int rc;

	dev_dbg(dev, "store_byte REG:%x DATA:%x", REG, data[1]);

	strncpy(tmps, buf, 16-1);
	rc = kstrtoul(tmps, 16, &tmp);
	if (rc != 0){
		dev_err(dev, "store_byte unable to convert \"%s\"", tmps);
		return rc;
	}

	data[0] = TDC_WRITE_CONFIG|REG;
	data[1] = tmp;

	if ((rc = tdc_gpx2_spi_write_then_read(to_spi_device(dev), data, 2, 0, 0)) == 0){
		return count;
	}else{
		dev_err(dev, "store_byte fail");
		return rc;
	}
}

static ssize_t show_byte(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const unsigned REG)
{
	char cmd = TDC_READ_CONFIG|REG;
	char data;

	dev_dbg(dev, "show_byte REG:%d", REG);
	if (tdc_gpx2_spi_write_then_read(to_spi_device(dev), &cmd, 1, &data, 1) == 0){
		return sprintf(buf, "0x%02x\n", data);
	}else{
		return -1;
	}
}


#define TDC_GPX2_REG(name, REG) 					\
static ssize_t show_##name(						\
	struct device *dev,						\
	struct device_attribute *attr,					\
	char* buf)							\
{									\
	return show_byte(dev, attr, buf, REG);	\
}									\
static ssize_t store_##name(						\
	struct device *dev,						\
	struct device_attribute *attr,					\
	const char* buf,						\
	size_t count)							\
{									\
	return store_byte(dev, attr, buf, count, REG);\
}									\
static DEVICE_ATTR(tdc_gpx2_##name, S_IRUGO|S_IWUSR, show_##name, store_##name);


TDC_GPX2_REG(cfg00,  0);
TDC_GPX2_REG(cfg01,  1);
TDC_GPX2_REG(cfg02,  2);
TDC_GPX2_REG(cfg03,  3);
TDC_GPX2_REG(cfg04,  4);
TDC_GPX2_REG(cfg05,  5);
TDC_GPX2_REG(cfg06,  6);
TDC_GPX2_REG(cfg07,  7);
TDC_GPX2_REG(cfg08,  8);
TDC_GPX2_REG(cfg09,  9);
TDC_GPX2_REG(cfg10, 10);
TDC_GPX2_REG(cfg11, 11);
TDC_GPX2_REG(cfg12, 12);
TDC_GPX2_REG(cfg13, 13);
TDC_GPX2_REG(cfg14, 14);
TDC_GPX2_REG(cfg15, 15);
TDC_GPX2_REG(cfg16, 16);
TDC_GPX2_REG(cfg17, 17);

static ssize_t show_result_triplet(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const unsigned REG)
{
	char cmd = TDC_READ_RESULT|REG;
	char data[3];
	int ii;

	for (ii = 0; ii < 3; ++ii){
		dev_dbg(dev, "show_byte REG:%d", REG);
		cmd = TDC_READ_RESULT|(REG+ii);
		if (tdc_gpx2_spi_write_then_read(to_spi_device(dev), &cmd, 1, &data+ii, 1) != 0){
			return -1;
		}
	}

	return sprintf(buf, "0x%02x%02x%02x\n", data[0], data[1], data[2]);
}

#define TDC_GPX2_RESULT(name, REG) 					\
static ssize_t show_result##name(					\
	struct device *dev,						\
	struct device_attribute *attr,					\
	char* buf)							\
{									\
	return show_result_triplet(dev, attr, buf, REG);		\
}									\
									\
static DEVICE_ATTR(tdc_gpx2_##name, S_IRUGO, show_result##name, 0);

TDC_GPX2_RESULT(ch01_ref,   8);
TDC_GPX2_RESULT(ch01_stop, 11);
TDC_GPX2_RESULT(ch02_ref,  14);
TDC_GPX2_RESULT(ch02_stop, 17);
TDC_GPX2_RESULT(ch03_ref,  20);
TDC_GPX2_RESULT(ch03_stop, 23);
TDC_GPX2_RESULT(ch04_ref,  26);
TDC_GPX2_RESULT(ch04_stop, 29);

static ssize_t store_power(
	struct device *dev,
	struct device_attribute *attr,
	const char* buf,
	size_t count)
{
	char data = TDC_POWER;
	dev_dbg(dev, "store_power DATA:%x", data);

	if (tdc_gpx2_spi_write_then_read(to_spi_device(dev), &data, 1, 0, 0) == 0){
		return count;
	}else{
		dev_err(dev, "store_power fail");
		return -1;
	}
}
static DEVICE_ATTR(tdc_gpx2_power, S_IWUSR, 0, store_power);


static ssize_t store_init(
	struct device *dev,
	struct device_attribute *attr,
	const char* buf,
	size_t count)
{
	char data = TDC_INIT;
	dev_dbg(dev, "store_init DATA:%x", data);

	if (tdc_gpx2_spi_write_then_read(to_spi_device(dev), &data, 1, 0, 0) == 0){
		return count;
	}else{
		dev_err(dev, "store_init fail");
		return -1;
	}
}
static DEVICE_ATTR(tdc_gpx2_init, S_IWUSR, 0, store_init);



const struct attribute *tdc_gpx2_attrs[] = {
	&dev_attr_tdc_gpx2_cfg00.attr,
	&dev_attr_tdc_gpx2_cfg01.attr,
	&dev_attr_tdc_gpx2_cfg02.attr,
	&dev_attr_tdc_gpx2_cfg03.attr,
	&dev_attr_tdc_gpx2_cfg04.attr,
	&dev_attr_tdc_gpx2_cfg05.attr,
	&dev_attr_tdc_gpx2_cfg06.attr,
	&dev_attr_tdc_gpx2_cfg07.attr,
	&dev_attr_tdc_gpx2_cfg08.attr,
	&dev_attr_tdc_gpx2_cfg09.attr,
	&dev_attr_tdc_gpx2_cfg10.attr,
	&dev_attr_tdc_gpx2_cfg11.attr,
	&dev_attr_tdc_gpx2_cfg12.attr,
	&dev_attr_tdc_gpx2_cfg13.attr,
	&dev_attr_tdc_gpx2_cfg14.attr,
	&dev_attr_tdc_gpx2_cfg15.attr,
	&dev_attr_tdc_gpx2_cfg16.attr,
	&dev_attr_tdc_gpx2_cfg17.attr,
	&dev_attr_tdc_gpx2_power.attr,
	&dev_attr_tdc_gpx2_init.attr,
	&dev_attr_tdc_gpx2_ch01_ref.attr,
	&dev_attr_tdc_gpx2_ch01_stop.attr,
	&dev_attr_tdc_gpx2_ch02_ref.attr,
	&dev_attr_tdc_gpx2_ch02_stop.attr,
	&dev_attr_tdc_gpx2_ch03_ref.attr,
	&dev_attr_tdc_gpx2_ch03_stop.attr,
	&dev_attr_tdc_gpx2_ch04_ref.attr,
	&dev_attr_tdc_gpx2_ch04_stop.attr,
	0
};

void tdc_sign_on(struct spi_device *spi)
{
	int reg;
	char tx = TDC_POWER;

	if (tdc_gpx2_spi_write_then_read(spi, &tx, 1, 0, 0) != 0){
		dev_err(&spi->dev, "tdc_sign_on RESET fail");
		return;
	}
	for (reg = 0; reg <= 16; ++reg){
		char rx;

		tx = TDC_READ_CONFIG|reg;

		if (tdc_gpx2_spi_write_then_read(spi, &tx, 1, &rx, 1) == 0){
			;
		}else{
			dev_err(&spi->dev, "tdc_sign_on fail");
			return;
		}
	}
}
static int tdc_gpx2spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;

	dev_info(&spi->dev, "ads5294spi_probe() bus:%d cs:%d",
			spi->master->bus_num, spi->chip_select);

	if (do_signon){
		tdc_sign_on(spi);
	}

	if (sysfs_create_files(&dev->kobj, tdc_gpx2_attrs)){
		dev_err(dev, "tdc_gpx2spi_probe() failed to create knobs");
		return -1;
	}
	return 0;
}
static int tdc_gpx2spi_remove(struct spi_device *spi)
{
	return 0;
}


#define I2C_CHAN(site) 	((site)+1)
#define NGPIO_CHIP	8

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


static void __init acq494_i2c_create(int site)
{
	struct i2c_adapter *i2c_adapter = i2c_get_adapter(I2C_CHAN(site));
	if (claim_0x20){
		if (new_device(i2c_adapter, "pca9534", 0x20, -1) == 0){
			printk("acq494_i2c_create(%d) 50R switch NOT found\n", site);
		}
	}
}

static void __init acq494_init_site(int site)
{
	static struct spi_board_info tdc_gpx2spi_spi_slave_info = {
			.modalias	= "tdc_gpx2spi",
			.platform_data	= 0,
			.irq		= -1,
			.max_speed_hz	= 20000000,
			.bus_num	= 0,
			.chip_select	= 0,
	};
	tdc_gpx2spi_spi_slave_info.bus_num = spi_bus_num;
	tdc_gpx2spi_spi_slave_info.chip_select = site2cs(site);
	spi_register_board_info(&tdc_gpx2spi_spi_slave_info, 1);

	acq494_i2c_create(site);
}

static void __init acq494_remove_site(int site)
{
	printk("acq494_remove_site %d ERROR removal not supported\n", site);
}


static struct spi_driver tdc_gpx2spi_driver = {
	.driver = {
		.name	= "tdc_gpx2spi",
		.owner	= THIS_MODULE,
	},
	//.id_table = m25p_ids,
	.probe	= tdc_gpx2spi_probe,
	.remove	= tdc_gpx2spi_remove,
};


static void __exit acq494_exit(void)
{
	for (; n_acq494--;){
		acq494_remove_site(acq494_sites[n_acq494]);
	}
	spi_unregister_driver(&tdc_gpx2spi_driver);
}

static int __init acq494_init(void)
{
        int status = 0;


	printk("D-TACQ ACQ494 Driver %s\n", REVID);

	spi_register_driver(&tdc_gpx2spi_driver);

	acq480_hook_spi();

	for (n_acq494 = 0; n_acq494 < acq494_sites_count; ++n_acq494){
		acq494_init_site(acq494_sites[n_acq494]);
	}
        return status;
}

module_init(acq494_init);
module_exit(acq494_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ494FMC spi Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
