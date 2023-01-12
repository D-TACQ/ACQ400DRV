/* ------------------------------------------------------------------------- *
 * acq480_drv.c
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
 * instantiates ACQ480 i2c device (50R termination)
 * also creates a device driver hook for the spi buffer
 * acq480_knobs app mmaps this buffer and updates it.
 * a task on a poll loop in this driver monitors changes and sends them to SPI
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

#include "acq480_ioctl.h"

#define REVID 		"1.1.0"
#define MODULE_NAME	"acq480"

int acq480_sites[6] = { 0,  };
int acq480_sites_count = 0;
module_param_array(acq480_sites, int, &acq480_sites_count, 0644);

static int n_acq480;
module_param(n_acq480, int, 0444);

static int claim_0x20 = 1;
module_param(claim_0x20, int, 0444);

static int claim_0x22 = 1;
module_param(claim_0x22, int, 0444);

static int spi_bus_num = 1;
module_param(spi_bus_num, int, 0444);

#define I2C_CHAN(site) 	((site)+1)
#define NGPIO_CHIP	8

#define SPI_BUFFER_LEN	4096	/* 1 page */

#define REGS_LEN	0x100

int site2cs(int site)
{
	return site - 1;
}
int cs2site(int cs)
{
	return cs + 1;
}

struct acq480_dev {
	dev_t devno;
	struct cdev cdev;
	struct platform_device *pdev;
	struct i2c_adapter *i2c_adapter;
	char devname[16];

	/* client stores to cli_buffer. dev_buffer caches device values
	 * update flushes client changes to to device
	 */
	struct HBM* cli_buf;
	struct HBM* dev_buf;

	struct spi_device *spi;
};

#define DEVP(adev) (&adev->pdev->dev)

static struct proc_dir_entry *acq480_proc_root;
static struct acq480_dev* acq480_devs[7];	/* 6 sites index from 1 */

/* we're ASSUMING that ads5294 doesn't mind access to non-existant regs ..
 * otherwise we have to have a sparse reg table
 */
#define ADS5294_MAXREG	0xff				/* address in shorts */
#define ADS5294_REGSLEN	((ADS5294_MAXREG+1)*sizeof(short))

unsigned short *getShortBuffer(struct HBM* buf)
{
	return (short*)buf->va;
}

int get_site(struct acq480_dev *adev)
{
	return adev->pdev->id;
}

static void ads5294_setReadout(struct acq480_dev* adev, int readout)
{
	char cmd[3] = { 0x1, 0, 0x1 };
	if (!readout) cmd[2] = 0;

	dev_dbg(DEVP(adev), "ads5294_setReadout () spi_write %02x %02x %02x",
						cmd[0], cmd[1], cmd[2]);
	spi_write(adev->spi, cmd, 3);
}
static void ads5294_cache_invalidate(struct acq480_dev* adev)
/* read back cache from device */
{
	short *cache = getShortBuffer(adev->dev_buf);
	int reg;
	ads5294_setReadout(adev, 1);
	for (reg = 2; reg <= ADS5294_MAXREG; ++reg){
		unsigned short tmp = spi_w8r16(adev->spi, reg);
		cache[reg] = tmp>>8 | ((tmp&0x00ff)<<8);
		dev_dbg(DEVP(adev),
			"ads5294_cache_invalidate() spi_w8r16 %02x %02x %02x",
			reg, cache[reg]>>8, cache[reg]&0x0ff);
	}
	ads5294_setReadout(adev, 0);

	memcpy(adev->cli_buf->va, adev->dev_buf->va, ADS5294_REGSLEN);
}
static void ads5294_cache_flush(struct acq480_dev* adev)
/* flush changes in cache to device */
{
	short *cache = getShortBuffer(adev->dev_buf);
	short *clibuf = getShortBuffer(adev->cli_buf);

	int reg;
	for (reg = 2; reg <= ADS5294_MAXREG; ++reg){
		if (cache[reg] != clibuf[reg]){
			char cmd[3];
			cache[reg] = clibuf[reg];
			cmd[0] = reg;
			cmd[1] = cache[reg] >> 8;
			cmd[2] = cache[reg] &0x0ff;
			dev_dbg(DEVP(adev),
				"ads5294_cache_flush() spi_write %02x %02x %02x",
				cmd[0], cmd[1], cmd[2]);
			spi_write(adev->spi, &cmd, 3);
		}
	}
}

static void ads5294_reset(struct acq480_dev* adev)
{
	char cmd[3] = { 0x0, 0, 0x1 };

	dev_dbg(DEVP(adev), "ads5294_reset () spi_write %02x %02x %02x",
						cmd[0], cmd[1], cmd[2]);
	spi_write(adev->spi, &cmd, 3);
}


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


#define acq480_of_match 0

struct acq480_dev* acq480_allocate_dev(struct platform_device *pdev)
{
	int site = pdev->id;
	struct acq480_dev* adev =
		kzalloc(sizeof(struct acq480_dev), GFP_KERNEL);

	if (adev == NULL){
		return NULL;
	}
	acq480_devs[site] = adev;

	adev->pdev = pdev;
	adev->i2c_adapter = i2c_get_adapter(I2C_CHAN(site));
	if (claim_0x20){
		if (new_device(adev->i2c_adapter, "pca9534", 0x20, -1) == 0){
			printk("acq480_init_site(%d) 50R switch NOT found\n", site);
		}
	}
	if (claim_0x22){
		if (new_device(adev->i2c_adapter, "tca6424", 0x22, -1) == 0){
			printk("acq480_init_site(%d) J-clean NOT found\n", site);
		}
	}

	snprintf(adev->devname, 16, "%s.%d", pdev->name, pdev->id);

	return adev;
}

int acq480_open(struct inode *inode, struct file *file)
{
        file->private_data = container_of(inode->i_cdev, struct acq480_dev, cdev);
        return 0;
}

int acq480_cli_buffer_mmap(struct file* file, struct vm_area_struct* vma)
/**
 * mmap the host buffer.
 */
{
	struct acq480_dev* adev = (struct acq480_dev*)file->private_data;

	struct HBM *hb = adev->cli_buf;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = hb->len;
	unsigned pfn = hb->pa >> PAGE_SHIFT;

	dev_dbg(&adev->pdev->dev, "%c vsize %lu psize %lu %s",
		'D', vsize, psize, vsize>psize? "EINVAL": "OK");

	if (vsize > psize){
		return -EINVAL;                   /* request too big */
	}
	if (remap_pfn_range(
		vma, vma->vm_start, pfn, vsize, vma->vm_page_prot)){
		return -EAGAIN;
	}else{
		return 0;
	}
}

static long
acq480_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct acq480_dev* adev = (struct acq480_dev*)file->private_data;
	switch(cmd){
	case ACQ480_CACHE_INVALIDATE:
		ads5294_cache_invalidate(adev);
		return 0;
	case ACQ480_CACHE_FLUSH:
		ads5294_cache_flush(adev);
		return 0;
	case ACQ480_RESET:
		ads5294_reset(adev);
		return 0;
	default:
		return -ENODEV;
	}
}
struct file_operations acq480_fops = {
        .owner = THIS_MODULE,
        .open = acq480_open,
        .mmap = acq480_cli_buffer_mmap,
        .unlocked_ioctl = acq480_unlocked_ioctl
};

/* read 0x100 bytes, 0x10 bytes at a time */
static void *acq480_proc_seq_start_buffers(struct seq_file *s, loff_t *pos)
{
        if (*pos == 0) {
        	struct acq480_dev *adev = s->private;
        	return adev->cli_buf->va;
        }

        return NULL;
}
#define BUFREAD	0x10	/* words to read 	*/
#define BUFMAX  0x100	/* total words to read 	*/
static int acq480_proc_seq_show_spibuf_row(struct seq_file *s, void *v)
{
	struct acq480_dev *adev = s->private;
	unsigned short* regs = (unsigned short*)v;
	unsigned short* base = getShortBuffer(adev->cli_buf);
	unsigned offregs = regs - base;

	int ir;

	seq_printf(s, "%02x:", offregs);
	for (ir = 0; ir < BUFREAD-1; ++ir){
		seq_printf(s, "%04x ", regs[ir]);
	}
	seq_printf(s, "%04x\n", regs[ir]);
	return 0;
}

static void* acq480_proc_seq_next_buffers(
		struct seq_file *s, void* v, loff_t *pos)
{
	*pos += BUFREAD;
	if (*pos  < BUFMAX){
		return v + BUFREAD * sizeof(short);
	}else{
		return NULL;
	}
}
static void acq480_proc_seq_stop(struct seq_file *s, void* v)
{

}

static int acq480_proc_open_spibuf(struct inode *inode, struct file *file)
{
	static struct seq_operations proc_seq_ops_spi_buf = {
	        .start = acq480_proc_seq_start_buffers,
	        .next = acq480_proc_seq_next_buffers,
	        .stop = acq480_proc_seq_stop,
	        .show = acq480_proc_seq_show_spibuf_row
	};
	int rc = seq_open(file, &proc_seq_ops_spi_buf);
	if (rc == 0){
		struct seq_file *m = file->private_data;
		m->private = PDE_DATA(inode);
	}
	return rc;
}

struct file_operations acq480_proc_fops = {
	        .owner = THIS_MODULE,
	        .open = acq480_proc_open_spibuf,
	        .read = seq_read,
	        .llseek = seq_lseek,
	        .release = seq_release
};


static int acq480_probe(struct platform_device *pdev)
{
	struct acq480_dev* adev = acq480_allocate_dev(pdev);
	struct device* dev = &pdev->dev;
	int rc;

	dev_info(dev, "acq480_probe: %d %s", pdev->id, adev->devname);
	rc = alloc_chrdev_region(&adev->devno, 0, 0, adev->devname);
	if (rc < 0) {
		dev_err(&pdev->dev, "unable to register chrdev\n");
	        goto fail;
	}

        cdev_init(&adev->cdev, &acq480_fops);
        adev->cdev.owner = THIS_MODULE;
        rc = cdev_add(&adev->cdev, adev->devno, 1);
        if (rc < 0){
        	goto fail;
        }

        /* there's no actual DMA, we're after the pa, but DMA_NONE BUGs */
        adev->cli_buf = hbm_allocate1(dev,  SPI_BUFFER_LEN, 0, DMA_BIDIRECTIONAL);
        memset(adev->cli_buf->va, 0, SPI_BUFFER_LEN);

        adev->dev_buf = hbm_allocate1(dev,  SPI_BUFFER_LEN, 0, DMA_BIDIRECTIONAL);

        if (acq480_proc_root == 0){
        	acq480_proc_root = proc_mkdir("driver/acq480", 0);
        }
        proc_create_data("spibuf", 0,
        		proc_mkdir(adev->devname, acq480_proc_root),
        		&acq480_proc_fops, adev);
	return 0;

fail:
	kfree(adev);
	return -1;
}

static int acq480_remove(struct platform_device *pdev)
{
	int site = pdev->id;

	i2c_put_adapter(acq480_devs[site]->i2c_adapter);
	kfree(acq480_devs[site]);
	acq480_devs[site] = 0;
	return -1;
}

static int ads5294spi_probe(struct spi_device *spi)
{
	struct acq480_dev* adev = acq480_devs[cs2site(spi->chip_select)];
	adev->spi = spi;
	dev_info(&spi->dev, "ads5294spi_probe() bus:%d cs:%d",
			spi->master->bus_num, spi->chip_select);

	ads5294_cache_invalidate(adev);
	return 0;
}
static int ads5294spi_remove(struct spi_device *spi)
{
	return 0;
}

static void __init acq480_init_site(int site)
{
	static struct spi_board_info ads5294spi_spi_slave_info = {
			.modalias	= "ads5294spi",
			.platform_data	= 0,
			.irq		= -1,
			.max_speed_hz	= 20000000,
			.bus_num	= 0,
			.chip_select	= 0,
	};
	struct platform_device* pdev =
			kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	pdev->name = MODULE_NAME;
	pdev->id = site;

	ads5294spi_spi_slave_info.bus_num = spi_bus_num;
	platform_device_register(pdev);

	ads5294spi_spi_slave_info.chip_select = site - 1;
	spi_register_board_info(&ads5294spi_spi_slave_info, 1);
}

static void __init acq480_remove_site(int site)
{
	printk("acq480_remove_site %d ERROR removal not supported\n", site);
}
static struct platform_driver acq480_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = acq480_of_match,
        },
        .probe = acq480_probe,
        .remove = acq480_remove,
};

static struct spi_driver ads5294spi_driver = {
	.driver = {
		.name	= "ads5294spi",
		.owner	= THIS_MODULE,
	},
	//.id_table = m25p_ids,
	.probe	= ads5294spi_probe,
	.remove	= ads5294spi_remove,
};

static void __exit acq480_exit(void)
{
	for (; n_acq480--;){
		acq480_remove_site(acq480_sites[n_acq480]);
	}
	platform_driver_unregister(&acq480_driver);
	spi_unregister_driver(&ads5294spi_driver);
}

extern void acq480_hook_spi(void);

static int __init acq480_init(void)
{
        int status = 0;


	printk("D-TACQ ACQ480 Driver %s\n", REVID);

	platform_driver_register(&acq480_driver);
	acq480_proc_root = proc_mkdir("driver/acq480", 0);

	spi_register_driver(&ads5294spi_driver);

	acq480_hook_spi();

	for (n_acq480 = 0; n_acq480 < acq480_sites_count; ++n_acq480){
		acq480_init_site(acq480_sites[n_acq480]);
	}
        return status;
}

module_init(acq480_init);
module_exit(acq480_exit);



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ480ELF spi/i2c Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
