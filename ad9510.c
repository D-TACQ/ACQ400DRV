/* ------------------------------------------------------------------------- */
/* ad9510.c ACQ420_FMC						     */
/* ------------------------------------------------------------------------- */
/* ad9510.c  clock distributor driver
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

#define REVID	"3"

#define SCPC_OFF	0x00

#define PLLCNTA_OFF 	0x04
#define PLLCNTB_OFF 	0x05
/* PLLCNTB_LSB 		0x06 	*/
#define PLL1_OFF	0x07
#define PLL2_OFF	0x08
#define PLL3_OFF	0x09
#define PLL4_OFF	0x0a
#define PLLRDIV_OFF	0x0b
/* PLLRDIV_LSB		0x0c 	*/
#define PLL5_OFF	0x0d

#define DLYBP5_OFF	0x34
#define DLYFS5_OFF	0x35
#define DLYFA5_OFF	0x36
/* Not used 0x37 */
#define DLYBP6_OFF	0x38
#define DLYFS6_OFF	0x39
#define DLYFA6_OFF	0x3a
/* Not used 0x3b */
#define LVPECL_OUT0_OFF	0x3c
#define LVPECL_OUT1_OFF	0x3d
#define LVPECL_OUT2_OFF	0x3e
#define LVPECL_OUT3_OFF	0x3f

#define LVDS_CMOS_OUT4_OFF	0x40
#define LVDS_CMOS_OUT5_OFF	0x41
#define LVDS_CMOS_OUT6_OFF	0x42
#define LVDS_CMOS_OUT7_OFF	0x43

#define CSPD_OFF	0x45

#define DIV0_OFF	0x48
#define DIV1_OFF 	0x4a
#define DIV2_OFF	0x4c
#define DIV3_OFF	0x4e
#define DIV4_OFF	0x50
#define DIV5_OFF	0x52
#define DIV6_OFF	0x54
#define DIV7_OFF	0x56

#define FUNPS_OFF	0x58
#define UPDATE_OFF	0x5a

#define SCPC_LEN	1
#define PLLCNTA_LEN 	1
#define PLLCNTB_LEN 	2
#define PLL1_LEN	1
#define PLL2_LEN	1
#define PLL3_LEN	1
#define PLL4_LEN	1
#define PLLRDIV_LEN	2
#define PLL5_LEN	1

#define DLYBP5_LEN	1
#define DLYFS5_LEN	1
#define DLYFA5_LEN	1
/* Not used 0x37 */
#define DLYBP6_LEN	1
#define DLYFS6_LEN	1
#define DLYFA6_LEN	1
/* Not used 0x3b */
#define LVPECL_OUT0_LEN	1
#define LVPECL_OUT1_LEN	1
#define LVPECL_OUT2_LEN	1
#define LVPECL_OUT3_LEN	1

#define LVDS_CMOS_OUT4_LEN	1
#define LVDS_CMOS_OUT5_LEN	1
#define LVDS_CMOS_OUT6_LEN	1
#define LVDS_CMOS_OUT7_LEN	1

#define CSPD_LEN	1

#define DIV0_LEN	2
#define DIV1_LEN 	2
#define DIV2_LEN	2
#define DIV3_LEN	2
#define DIV4_LEN	2
#define DIV5_LEN	2
#define DIV6_LEN	2
#define DIV7_LEN	2

#define FUNPS_LEN	1
#define UPDATE_LEN	1

#define MAX_OFF		0x5a
#define MAX_DATA	2

#define CMD		0
#define ADDR		1
/* easiest to work in bytes ..
 * tx[0] = cmd byte
 * tx[1] = addr byte
 * We only support 1 or 2 byte payloads ..
 */
#define AD9510RnW	0x80
#define AD9510_WX	0x60
#define AD9510_WX1	0x00
#define AD9510_WX2	0x20


int dummy_device;
module_param(dummy_device, int, 0644);

static int dummy_spi_write_then_read(
	struct device * dev,
	const void *txbuf, unsigned n_tx, void *rxbuf, unsigned n_rx)
{
	static char mirror[MAX_OFF][MAX_DATA];	/** @todo one mirror for all devs? */
	const char* txb = (const char*)txbuf;
	char *rxb = (char*)rxbuf;
	int addr = txb[1];
	int is_read = (txb[0]&AD9510RnW) != 0;
	//int nb = ((txb[0]&AD9510_WX) == AD9510_WX2)? 2: 1;

	if (addr > MAX_OFF){
		dev_err(dev, "ERROR bad addr > MAX_OFF");
		return -1;
	}

	if (is_read){
		dev_dbg(dev, "read [%02x%02x] %d: %02x %02x .. ",
				txb[0], txb[1], n_rx, mirror[addr][0], mirror[addr][1]);
		memcpy(rxb, mirror[addr], n_rx);
	}else{
		dev_dbg(dev, "write [%02x%02x] %d: %02x %02x .. ",
				txb[0], txb[1], n_tx-2, txb[2], txb[3]);
		memcpy(mirror[addr], txb+2, n_tx-1);
	}
	return 0;
}

static int ad9510_spi_write_then_read(
	struct device * dev, const void *txbuf, unsigned n_tx,
	void *rxbuf, unsigned n_rx)
{
	if (dummy_device){
		return dummy_spi_write_then_read(dev, txbuf, n_tx, rxbuf, n_rx);
	}else{
		return spi_write_then_read(to_spi_device(dev), txbuf, n_tx, rxbuf, n_rx);
	}
}

int get_hex_bytes(const char* buf, char* data, int maxdata)
{
	char tmp[4] = {};
	const char* cursor = buf;
	int it;
	int id = 0;

	for (it = 0; id < maxdata && *cursor; it = !it, ++cursor){
		tmp[it] = *cursor;
		if (it == 1){
			if (kstrtou8(tmp, 16, &data[id]) == 0){
				++id;
			}else{
				dev_err(0, "kstrtou8 returns error on \"%s\"", tmp);
				return id;
			}
		}
	}
	return id;
}
static ssize_t store_multibytes(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int REG, const int LEN)
{
	char data[MAX_DATA+2];

	dev_dbg(dev, "store_multibytes REG:%d LEN:%d \"%s\"", REG, LEN, buf);

	if (get_hex_bytes(buf, data+2, MAX_DATA) == LEN){
		data[CMD] = LEN==2? AD9510_WX2: AD9510_WX1;
		data[ADDR] = REG;


		dev_info(dev, "data: %02x %02x%02x%02x",
				data[0],data[1],data[2],data[3]);
		if (ad9510_spi_write_then_read(dev, data, LEN+2, 0, 0) == 0){
			return count;
		}else{
			dev_err(dev, "ad9854_spi_write_then_read failed");
			return -1;
		}
	}else{
		dev_err(dev, "store_multibytes reg:%02x %d bytes needed", REG, LEN);
		return -1;
	}

	return 0;
}
static ssize_t show_multibytes(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int REG, const int LEN)
{
	char cmd[2];
	char data[MAX_DATA];

	cmd[CMD] = AD9510RnW | (LEN==2? AD9510_WX2: AD9510_WX1);
	cmd[ADDR] = REG;

	dev_dbg(dev, "show_multibytes REG:%d LEN:%d", REG, LEN);
	if (ad9510_spi_write_then_read(dev, cmd, 2, data, LEN) == 0){
		int ib;
		char* cursor = buf;
		for (ib = 0; ib < LEN; ++ib){
			cursor += sprintf(cursor, "%02x", data[ib]);
		}
		strcat(cursor, "\n");
		return strlen(buf);
	}else{
		return -1;
	}
}

static ssize_t show_help(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	const int REG, const int LEN)
{
	return sprintf(buf, "%d %d\n", REG, LEN);
}


#define AD9510_REG(name) 						\
static ssize_t show_##name(						\
	struct device *dev,						\
	struct device_attribute *attr,					\
	char* buf)							\
{									\
	return show_multibytes(dev, attr, buf, name##_OFF, name##_LEN);	\
}									\
static ssize_t show_##name##_help(					\
	struct device *dev,						\
	struct device_attribute *attr,					\
	char* buf)							\
{									\
	return show_help(dev, attr, buf, name##_OFF, name##_LEN);	\
}									\
static ssize_t store_##name(						\
	struct device *dev,						\
	struct device_attribute *attr,					\
	const char* buf,						\
	size_t count)							\
{									\
	return store_multibytes(dev, attr, buf, count, name##_OFF, name##_LEN);\
}									\
static DEVICE_ATTR(name, S_IRUGO|S_IWUSR, show_##name, store_##name);	\
static DEVICE_ATTR(_##name, S_IRUGO, show_##name##_help, 0);


AD9510_REG(SCPC);

AD9510_REG(PLLCNTA);
AD9510_REG(PLLCNTB);
AD9510_REG(PLL1);
AD9510_REG(PLL2);
AD9510_REG(PLL3);
AD9510_REG(PLL4);
AD9510_REG(PLLRDIV);
AD9510_REG(PLL5);

AD9510_REG(DLYBP5);
AD9510_REG(DLYFS5);
AD9510_REG(DLYFA5);

AD9510_REG(DLYBP6);
AD9510_REG(DLYFS6);
AD9510_REG(DLYFA6);

AD9510_REG(LVPECL_OUT0);
AD9510_REG(LVPECL_OUT1);
AD9510_REG(LVPECL_OUT2);
AD9510_REG(LVPECL_OUT3);

AD9510_REG(LVDS_CMOS_OUT4);
AD9510_REG(LVDS_CMOS_OUT5);
AD9510_REG(LVDS_CMOS_OUT6);
AD9510_REG(LVDS_CMOS_OUT7);

AD9510_REG(CSPD);

AD9510_REG(DIV0);
AD9510_REG(DIV1);
AD9510_REG(DIV2);
AD9510_REG(DIV3);
AD9510_REG(DIV4);
AD9510_REG(DIV5);
AD9510_REG(DIV6);
AD9510_REG(DIV7);

AD9510_REG(FUNPS);
AD9510_REG(UPDATE);

const struct attribute *ad9510_attrs[] = {
	&dev_attr_SCPC.attr,	&dev_attr__SCPC.attr,
	&dev_attr_PLLCNTA.attr,	&dev_attr__PLLCNTA.attr,
	&dev_attr_PLLCNTB.attr,	&dev_attr__PLLCNTB.attr,
	&dev_attr_PLL1.attr,	&dev_attr__PLL1.attr,
	&dev_attr_PLL2.attr,	&dev_attr__PLL2.attr,
	&dev_attr_PLL3.attr,	&dev_attr__PLL3.attr,
	&dev_attr_PLL4.attr,	&dev_attr__PLL4.attr,
	&dev_attr_PLLRDIV.attr,	&dev_attr__PLLRDIV.attr,
	&dev_attr_PLL5.attr,	&dev_attr__PLL5.attr,

	&dev_attr_DLYBP5.attr,	&dev_attr__DLYBP5.attr,
	&dev_attr_DLYFS5.attr,	&dev_attr__DLYFS5.attr,
	&dev_attr_DLYFA5.attr,	&dev_attr__DLYFA5.attr,

	&dev_attr_DLYBP6.attr,	&dev_attr__DLYBP6.attr,
	&dev_attr_DLYFS6.attr,	&dev_attr__DLYFS6.attr,
	&dev_attr_DLYFA6.attr,	&dev_attr__DLYFA6.attr,

	&dev_attr_LVPECL_OUT0.attr, &dev_attr__LVPECL_OUT0.attr,
	&dev_attr_LVPECL_OUT1.attr, &dev_attr__LVPECL_OUT1.attr,
	&dev_attr_LVPECL_OUT2.attr, &dev_attr__LVPECL_OUT2.attr,
	&dev_attr_LVPECL_OUT3.attr, &dev_attr__LVPECL_OUT3.attr,

	&dev_attr_LVDS_CMOS_OUT4.attr, &dev_attr__LVDS_CMOS_OUT4.attr,
	&dev_attr_LVDS_CMOS_OUT5.attr, &dev_attr__LVDS_CMOS_OUT5.attr,
	&dev_attr_LVDS_CMOS_OUT6.attr, &dev_attr__LVDS_CMOS_OUT6.attr,
	&dev_attr_LVDS_CMOS_OUT7.attr, &dev_attr__LVDS_CMOS_OUT7.attr,

	&dev_attr_CSPD.attr,	&dev_attr__CSPD.attr,

	&dev_attr_DIV0.attr,	&dev_attr__DIV0.attr,
	&dev_attr_DIV1.attr,	&dev_attr__DIV1.attr,
	&dev_attr_DIV2.attr,	&dev_attr__DIV2.attr,
	&dev_attr_DIV3.attr,	&dev_attr__DIV3.attr,
	&dev_attr_DIV4.attr,	&dev_attr__DIV4.attr,
	&dev_attr_DIV5.attr,	&dev_attr__DIV5.attr,
	&dev_attr_DIV6.attr,	&dev_attr__DIV6.attr,
	&dev_attr_DIV7.attr,	&dev_attr__DIV7.attr,

	&dev_attr_FUNPS.attr,	&dev_attr__FUNPS.attr,
	&dev_attr_UPDATE.attr,	&dev_attr__UPDATE.attr,
	0
};
static int ad9510_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	if (sysfs_create_files(&dev->kobj, ad9510_attrs)){
		dev_err(dev, "ad9510_probe() failed to create knobs");
		return -1;
	}
	return 0;
}
static int ad9510_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver ad9510_spi_driver = {
	.driver = {
		.name	= "ad9510",
		.owner	= THIS_MODULE,
	},
	.probe	= ad9510_probe,
	.remove	= ad9510_remove,
};


static void __exit ad9510_exit(void)
{
	spi_unregister_driver(&ad9510_spi_driver);
}


static int __init ad9510_init(void)
{
        int status = 0;

	printk("D-TACQ AD9510 spi driver %s\n", REVID);

	spi_register_driver(&ad9510_spi_driver);

        return status;
}

module_init(ad9510_init);
module_exit(ad9510_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ AD9510 spi Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
