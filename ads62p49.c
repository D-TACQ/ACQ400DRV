/* ------------------------------------------------------------------------- */
/* ads62p49.c ACQ420_FMC						     */
/* ------------------------------------------------------------------------- */
/* ads62p49.c  ADC SPI driver
 * Project: ACQ420_FMC
 * Created: 25 JAN 2017  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2017 Peter Milne, D-TACQ Solutions Ltd         *
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

/* MSB FIRST! */

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

#define REVID "1"

#define R00_OFF 0x00
#define R20_OFF 0x20
#define R3F_OFF 0x3f
#define R40_OFF 0x40
#define R41_OFF 0x41
#define R44_OFF 0x44
#define R50_OFF 0x50
#define R51_OFF 0x51
#define R52_OFF 0x52
#define R53_OFF 0x53
#define R55_OFF 0x55
#define R57_OFF 0x57
#define R62_OFF 0x62
#define R63_OFF 0x63
#define R66_OFF 0x66
#define R68_OFF 0x68
#define R6A_OFF 0x6a
#define R75_OFF 0x75
#define R76_OFF 0x76

#define CMD 0
#define ADDR 0
#define DATA 1
#define MAX_DATA 1
#define CMD_LEN 2

static int ads62p49_spi_write_then_read(struct device *dev, const void *txbuf,
					unsigned n_tx, void *rxbuf,
					unsigned n_rx)
{
	return spi_write_then_read(to_spi_device(dev), txbuf, n_tx, rxbuf,
				   n_rx);
}

int get_hex_bytes(const char *buf, char *data, int maxdata)
{
	char tmp[4] = {};
	const char *cursor = buf;
	int it;
	int id = 0;

	for (it = 0; id < maxdata && *cursor; it = !it, ++cursor) {
		tmp[it] = *cursor;
		if (it == 1) {
			if (kstrtou8(tmp, 16, &data[id]) == 0) {
				++id;
			} else {
				dev_err(0, "kstrtou8 returns error on \"%s\"",
					tmp);
				return id;
			}
		}
	}
	return id;
}
static ssize_t store_multibytes(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count, const int REG)
{
	char data[MAX_DATA + 2];

	dev_dbg(dev, "store_multibytes REG:%02x \"%s\"", REG, buf);

	if (get_hex_bytes(buf, data + DATA, MAX_DATA) == MAX_DATA) {
		data[ADDR] = REG;

		dev_dbg(dev, "data: %02x %02x", data[0], data[1]);
		if (ads62p49_spi_write_then_read(dev, data, CMD_LEN, 0, 0) ==
		    0) {
			return count;
		} else {
			dev_err(dev, "ad9854_spi_write_then_read failed");
			return -1;
		}
	} else {
		dev_err(dev, "store_multibytes %d bytes needed", MAX_DATA);
		return -1;
	}

	return 0;
}
static ssize_t show_multibytes(struct device *dev,
			       struct device_attribute *attr, char *buf,
			       const int REG)
{
	char cmd[2];
	char data[MAX_DATA];

	cmd[ADDR] = REG;
	cmd[DATA] = 0;

	dev_dbg(dev, "show_multibytes REG:%d", REG);
	if (ads62p49_spi_write_then_read(dev, cmd, 1, data, 1) == 0) {
		int ib;
		char *cursor = buf;
		for (ib = 0; ib < MAX_DATA; ++ib) {
			cursor += sprintf(cursor, "%02x", data[ib]);
		}
		strcat(cursor, "\n");
		return strlen(buf);
	} else {
		return -1;
	}
}

#define ADS62P49_REG(name)                                                   \
	static ssize_t show_##name(struct device *dev,                       \
				   struct device_attribute *attr, char *buf) \
	{                                                                    \
		return show_multibytes(dev, attr, buf, name##_OFF);          \
	}                                                                    \
	static ssize_t store_##name(struct device *dev,                      \
				    struct device_attribute *attr,           \
				    const char *buf, size_t count)           \
	{                                                                    \
		return store_multibytes(dev, attr, buf, count, name##_OFF);  \
	}                                                                    \
	static DEVICE_ATTR(name, S_IRUGO | S_IWUSR, show_##name, store_##name);

ADS62P49_REG(R00);
ADS62P49_REG(R20);
ADS62P49_REG(R3F);
ADS62P49_REG(R40);
ADS62P49_REG(R41);
ADS62P49_REG(R44);
ADS62P49_REG(R50);
ADS62P49_REG(R51);
ADS62P49_REG(R52);
ADS62P49_REG(R53);
ADS62P49_REG(R55);
ADS62P49_REG(R57);
ADS62P49_REG(R62);
ADS62P49_REG(R63);
ADS62P49_REG(R66);
ADS62P49_REG(R68);
ADS62P49_REG(R6A);
ADS62P49_REG(R75);
ADS62P49_REG(R76);

const struct attribute *ads62p49_attrs[] = {
	&dev_attr_R00.attr, &dev_attr_R20.attr,
	&dev_attr_R3F.attr, &dev_attr_R40.attr,
	&dev_attr_R41.attr, &dev_attr_R44.attr,
	&dev_attr_R50.attr, &dev_attr_R51.attr,
	&dev_attr_R52.attr, &dev_attr_R53.attr,
	&dev_attr_R55.attr, &dev_attr_R57.attr,
	&dev_attr_R62.attr, &dev_attr_R63.attr,
	&dev_attr_R66.attr, &dev_attr_R68.attr,
	&dev_attr_R6A.attr, &dev_attr_R75.attr,
	&dev_attr_R76.attr, 0
};
static int ads62p49_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	if (sysfs_create_files(&dev->kobj, ads62p49_attrs)) {
		dev_err(dev, "ads62p49_probe() failed to create knobs");
		return -1;
	}
	return 0;
}
static int ads62p49_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver ads62p49_spi_driver = {
	.driver = {
		.name	= "ads62p49",
		.owner	= THIS_MODULE,
	},
	//.id_table = m25p_ids,
	.probe	= ads62p49_probe,
	.remove	= ads62p49_remove,
};

static void __exit ads62p49_exit(void)
{
	spi_unregister_driver(&ads62p49_spi_driver);
}

static int __init ads62p49_init(void)
{
	int status = 0;

	printk("D-TACQ ADS62P49 spi driver %s\n", REVID);

	spi_register_driver(&ads62p49_spi_driver);

	return status;
}

module_init(ads62p49_init);
module_exit(ads62p49_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ADS62P49 spi Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
