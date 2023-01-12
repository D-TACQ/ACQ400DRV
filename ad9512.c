/* ------------------------------------------------------------------------- */
/* ad9512.c ACQ420_FMC						     */
/* ------------------------------------------------------------------------- */
/* ad9512.c  clock distributor driver
 * Project: ACQ420_FMC
 * Created: 4 Aug 2016  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2016 Peter Milne, D-TACQ Solutions Ltd         *
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

#define REVID	"4"

#define SCPC_OFF	0x00
#define DBP4_OFF	0x34
#define DFS4_OFF	0x35
#define DFA4_OFF	0x36
#define LVPECL0_OFF	0x3d
#define LVPECL1_OFF	0x3e
#define LVPECL2_OFF 0x3f
#define LVDS3_OFF	0x40
#define LVDS4_OFF	0x41
#define CSPD_OFF	0x45

#define DIV0_OFF	0x4a
#define DIV1_OFF 	0x4c
#define DIV2_OFF	0x4e
#define DIV3_OFF	0x50
#define DIV4_OFF	0x52

#define FUNPS_OFF	0x58
#define UPDATE_OFF	0x5a

#define SCPC_LEN	1
#define DBP4_LEN	1
#define DFS4_LEN	1
#define DFA4_LEN	1
#define LVPECL0_LEN	1
#define LVPECL1_LEN	1
#define LVPECL2_LEN 	1
#define LVDS3_LEN	1
#define LVDS4_LEN	1
#define CSPD_LEN	1

#define DIV0_LEN	2
#define DIV1_LEN 	2
#define DIV2_LEN	2
#define DIV3_LEN	2
#define DIV4_LEN	2

#define FUNPS_LEN	1
#define UPDATE_LEN	1

#define MAX_OFF		0x5a
#define MAX_DATA	2

#define CMD		0
#define ADDR		1
#define IHL		2		// Instruction Header Len
/* easiest to work in bytes ..
 * tx[0] = cmd byte
 * tx[1] = addr byte
 * We only support 1 or 2 byte payloads ..
 */
#define AD9512RnW	0x80
#define AD9512_WX	0x60
#define AD9512_WX1	0x00
#define AD9512_WX2	0x20


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
	int is_read = (txb[0]&AD9512RnW) != 0;
	//int nb = ((txb[0]&AD9512_WX) == AD9512_WX2)? 2: 1;

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

static int ad9512_spi_write_then_read(
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

#define SWAP(aa,bb,tt)  ( tt = aa, aa = bb, bb = tt )

/* handle bizzare MSB addressing
 * set addr to the high-address byte, send it first
 */
ssize_t store_multibytes(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int REG, const int LEN)
{
	char data[IHL+MAX_DATA];

	dev_dbg(dev, "store_multibytes REG:%d LEN:%d \"%s\"", REG, LEN, buf);

	if (get_hex_bytes(buf, data+2, MAX_DATA) == LEN){
		data[CMD] = LEN==2? AD9512_WX2: AD9512_WX1;
		data[ADDR] = LEN==2? REG+1: REG;

		if (LEN==2){
			char t;
			SWAP(data[2], data[3], t);

			dev_dbg(dev, "data: %02x%02x %02x%02x  hi-addr-first",
					data[0],data[1],data[2],data[3]);
		}else{
			dev_dbg(dev, "data: %02x%02x %02x",
					data[0],data[1],data[2]);
		}


		if (ad9512_spi_write_then_read(dev, data, IHL+LEN, 0, 0) == 0){
			return count;
		}else{
			dev_err(dev, "ad9854_spi_write_then_read failed");
			return -1;
		}
	}else{
		dev_err(dev, "store_multibytes %d bytes needed", LEN);
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

	cmd[CMD] = AD9512RnW | (LEN==2? AD9512_WX2: AD9512_WX1);
	cmd[ADDR] = LEN==2? REG+1: REG;

	dev_dbg(dev, "show_multibytes REG:%d LEN:%d", REG, LEN);
	if (ad9512_spi_write_then_read(dev, cmd, IHL, data, LEN) == 0){
		int ib;
		char* cursor = buf;

		if (LEN==2){
			char t;
			SWAP(data[0], data[1], t);
		}
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


#define AD9512_REG(name) 						\
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


AD9512_REG(SCPC);
AD9512_REG(DBP4);
AD9512_REG(DFS4);
AD9512_REG(DFA4);
AD9512_REG(LVPECL0);
AD9512_REG(LVPECL1);
AD9512_REG(LVPECL2);
AD9512_REG(LVDS3);
AD9512_REG(LVDS4);
AD9512_REG(CSPD);

AD9512_REG(DIV0);
AD9512_REG(DIV1);
AD9512_REG(DIV2);
AD9512_REG(DIV3);
AD9512_REG(DIV4);

AD9512_REG(FUNPS);
AD9512_REG(UPDATE);

const struct attribute *ad9512_attrs[] = {
	&dev_attr_SCPC.attr,	&dev_attr__SCPC.attr,
	&dev_attr_DBP4.attr,	&dev_attr__DBP4.attr,
	&dev_attr_DFS4.attr,	&dev_attr__DFS4.attr,
	&dev_attr_DFA4.attr,	&dev_attr__DFA4.attr,
	&dev_attr_LVPECL0.attr,	&dev_attr__LVPECL0.attr,
	&dev_attr_LVPECL1.attr,	&dev_attr__LVPECL1.attr,
	&dev_attr_LVPECL2.attr,	&dev_attr__LVPECL2.attr,
	&dev_attr_LVDS3.attr,	&dev_attr__LVDS3.attr,
	&dev_attr_LVDS4.attr,	&dev_attr__LVDS4.attr,
	&dev_attr_CSPD.attr,	&dev_attr__CSPD.attr,

	&dev_attr_DIV0.attr,	&dev_attr__DIV0.attr,
	&dev_attr_DIV1.attr,	&dev_attr__DIV1.attr,
	&dev_attr_DIV2.attr,	&dev_attr__DIV2.attr,
	&dev_attr_DIV3.attr,	&dev_attr__DIV3.attr,
	&dev_attr_DIV4.attr,	&dev_attr__DIV4.attr,

	&dev_attr_FUNPS.attr,	&dev_attr__FUNPS.attr,
	&dev_attr_UPDATE.attr,	&dev_attr__UPDATE.attr,
	0
};
static int ad9512_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	if (sysfs_create_files(&dev->kobj, ad9512_attrs)){
		dev_err(dev, "ad9512_probe() failed to create knobs");
		return -1;
	}
	return 0;
}
static int ad9512_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver ad9512_spi_driver = {
	.driver = {
		.name	= "ad9512",
		.owner	= THIS_MODULE,
	},
	//.id_table = m25p_ids,
	.probe	= ad9512_probe,
	.remove	= ad9512_remove,
};


static void __exit ad9512_exit(void)
{
	spi_unregister_driver(&ad9512_spi_driver);
}


static int __init ad9512_init(void)
{
        int status = 0;

	printk("D-TACQ AD9512 spi driver %s\n", REVID);

	spi_register_driver(&ad9512_spi_driver);

        return status;
}

module_init(ad9512_init);
module_exit(ad9512_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ AD9512 spi Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
