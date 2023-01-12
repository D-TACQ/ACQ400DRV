/* ------------------------------------------------------------------------- *
 * acq465_drv.c
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2021 Peter Milne, D-TACQ Solutions Ltd
 *                      <peter dot milne at D hyphen TACQ dot com>          
 *                         www.d-tacq.com
 *   Created on: 28 September 2021
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
 * instantiates ACQ465 spi buffer
 * also creates a device driver hook for the spi buffer
 * AD7134 has 0x48 addresses. Map the buffer as follows:
 * acq465.1/chip0 : 0x0000 .. 0x007f
 * acq465.1/chip1 : 0x0080 .. 0x0100
 * ...
 * acq465.1/chip7 : 0x0780 .. 0x07ff
 *
 * acq465.2/chip0 : 0x0800 .. 0x087f
 *
 * acq465.5/chip0 : 0x1000 .. 0x107f         # site 5 takes second 4K page
 *
 * acq465_knobs app mmaps this buffer and updates it.
 * a task on a poll loop in this driver monitors changes and sends them to SPI
 * We have one linux device per site, but we have to be aware of the 8 physical devices..
 */
#include <linux/kernel.h>

#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#ifdef READY_FOR_CRC8
/* kernel CONFIG_CRC8 set */
#include <linux/crc8.h>
#endif




#include "hbm.h"

#include "acq465_ioctl.h"

#define COPY_FROM_USER(to, from, len) \
        if (copy_from_user(to, from, len)) { return -EFAULT; }

#define COPY_TO_USER(to, from, len) \
        if (copy_to_user(to, from, len)) { return -EFAULT; }


/*
 * acq1001_301> cat /mnt/local/apply_dig_if_reset
map /usr/local/acq1002.map

mm $s1+2c 8
sleep 0.1
acq465_knobs -A reg 0x01 0x82
sleep 0.1
#acq465_knobs -A reg 0x01 0x80
sleep 0.1
mm $s1+2c 0
 *
 */

#define ACQ465_LCS_BROADCAST	0x00000008

extern void acq465_lcs(int site, unsigned value);

#define REVID 		"0.2.5"
#define MODULE_NAME	"acq465"

int acq465_sites[6] = { 0,  };
int acq465_sites_count = 0;
module_param_array(acq465_sites, int, &acq465_sites_count, 0644);

static int n_acq465;
module_param(n_acq465, int, 0444);

static int spi_bus_num = 1;
module_param(spi_bus_num, int, 0444);

static int set_ident = 1;
module_param(set_ident, int, 0444);

static int clear_lcs = 1;
module_param(clear_lcs, int, 0644);

static int HW = 1;
module_param(HW, int, 0644);

static int USE_CRC = 0;
module_param(USE_CRC, int, 0444);

static int dummy_mclk;
module_param(dummy_mclk, int, 0644);
MODULE_PARM_DESC(dummy_mclk, "no spi transaction, just msleep: timing test");

int mclk_counts[2] = { 0,  };
int mclk_counts_entries = 2;
module_param_array(mclk_counts, int, &mclk_counts_entries, 0644);
MODULE_PARM_DESC(mclk_counts, "[0] rollover_counts [1] regular_counts expect [1] = [0]*3 or better");

int spi_clr = 0;
module_param(spi_clr, int, 0644);
MODULE_PARM_DESC(spi_clr, "send a clear command to spi before reset");

#define CMDLEN (USE_CRC? 3: 2)

// round up to integer pages
// 6 sites -> 2 pages, but if there is only 2 sites eg ACQ1002, we can save a PAGE..



static struct HBM* driver_spi_buffer;     // single buffer for all devices.
static struct HBM* client_spi_buffer;     // single buffer for all devices.

#define DRV_PA(devnum) 	(driver_spi_buffer->pa+MODULE_SPI_BUFFER_LEN*((devnum)-1))
#define DRV_VA(devnum) 	((unsigned char*)driver_spi_buffer->va+MODULE_SPI_BUFFER_LEN*((devnum)-1))

#define CLI_PA(devnum) 	(client_spi_buffer->pa+MODULE_SPI_BUFFER_LEN*((devnum)-1))
#define CLI_VA(devnum) 	((unsigned char*)client_spi_buffer->va+MODULE_SPI_BUFFER_LEN*((devnum)-1))

int site2cs(int site)
{
	return site - 1;
}
int cs2site(int cs)
{
	return cs + 1;
}

struct acq465_dev {
	dev_t devno;
	struct cdev cdev;
	struct platform_device *pdev;
	char devname[16];

	/* client stores to cli_buffer. dev_buffer caches device values
	 * update flushes client changes to to device
	 */
	struct BUF {
		unsigned char* va;
		unsigned pa;
	} cli_buf, dev_buf;

	struct spi_device *spi;
	struct mutex sem;
	void* owner;
};



#define DEVP(adev) (&adev->pdev->dev)

static struct proc_dir_entry *acq465_proc_root;
static struct acq465_dev* acq465_devs[7];	/* 6 sites index from 1 */

/* we're ASSUMING that ad7134 doesn't mind access to non-existant regs ..
 * otherwise we have to have a sparse reg table
 */
#define AD7134_MAXREG	0x48				/* address in bytes */
#define AD7134_REGSLEN	((AD7134_MAXREG+1))            /* register set length in bytes */

#define AD7134_RD	0x80

#define AD7413_MCLK_COUNTER	0x3f

static void _ident(unsigned char* drv_buf, int site, int id)
{
	unsigned char* pb = drv_buf + (site-1)*MODULE_SPI_BUFFER_LEN;
	int chip;
	int ii;
	for (chip = 0; chip < NCHIPS; ++chip, pb += REGS_LEN){
		for (ii = 0; ii < REGS_LEN; ++ii){
			pb[ii] = ii < AD7134_REGSLEN? ii+id: chip|(site<<4);
		}
	}
}
static void ident(unsigned char* drv_buf, int id)
{
	int nn;

	for (nn = 0; nn < acq465_sites_count; ++nn){
		_ident(drv_buf, acq465_sites[nn], id);
	}
}
int get_site(struct acq465_dev *adev)
{
	return adev->pdev->id;
}

char* make_cmd_string(char buf[], unsigned char cmd[], int ncmd)
{
	char* pb = buf;
	int ic;
	for (ic = 0; ic < ncmd; ++ic){
		pb += sprintf(pb, "%02x,", cmd[ic]);
	}
	return buf;
}

#define CRC_POLY	0x83     /* x8 + x2 + x1 */
#define CRC_SEED	0xa5

#ifdef READY_FOR_CRC8	
static u8 crc8_table[256];

void init_crc(void)
{
	crc8_populate_lsb(crc8_table, CRC_POLY);
}
#else
void init_crc(void) {}
#endif	
unsigned char CRC3(unsigned char cmd[])
/* set 3rd byte to CRC of first 2 bytes */
{
#ifdef READY_FOR_CRC8	
	return cmd[2] = crc8(crc8_table, cmd, 2, CRC_SEED);
#else
	return cmd[2] = '\0';
#endif	
}

int acq465_spi_write(struct acq465_dev* adev, unsigned chip, unsigned char cmd[], int ncmd)
{
	char buf[16];
	char rxbuf[8];
	int status;
	struct spi_transfer xfers[1] = {};

	xfers[0].tx_buf = cmd;
	xfers[0].rx_buf = rxbuf;
	xfers[0].len = ncmd;

	dev_dbg(DEVP(adev), "%s %s lcs:%02x txb: %s", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, cmd, ncmd));

	if (HW){
		acq465_lcs(get_site(adev), chip);
		status = spi_sync_transfer(adev->spi, xfers, 1);
		if (clear_lcs){
			acq465_lcs(get_site(adev), 0);
		}
	}

	dev_dbg(DEVP(adev), "%s %s lcs:%02x rxb: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, rxbuf, ncmd), status);

	return status;
}

int acq465_spi_read(struct acq465_dev* adev, unsigned chip, unsigned char cmd[], int ncmd, unsigned char rxbuf[])
{
	char buf[16];
	int status = 0;
	struct spi_transfer xfers[1] = {};

	xfers[0].tx_buf = cmd;
	xfers[0].rx_buf = rxbuf;
	xfers[0].len = ncmd;

	dev_dbg(DEVP(adev), "%s %s lcs:%02x cmd: %s", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, cmd, ncmd));

	rxbuf[0] = 0x0a; rxbuf[1] = 0x0b; rxbuf[2] = 0x0c;

	if (HW){
		acq465_lcs(get_site(adev), chip);
		status = spi_sync_transfer(adev->spi, xfers, 1);
		if (clear_lcs){
			acq465_lcs(get_site(adev), 0);
		}
	}

	dev_dbg(DEVP(adev), "%s %s lcs:%02x txb: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, cmd, ncmd), status);
	dev_dbg(DEVP(adev), "%s %s lcs:%02x rxb: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, rxbuf, ncmd), status);

	return status;
}

static int ad7134_cache_invalidate(struct acq465_dev* adev, int chip)
/* fill cache by readback from device */
{
	unsigned char *cache = adev->dev_buf.va + chip*REGS_LEN;
	int status = 0;

	dev_dbg(DEVP(adev), "%s chip %x", __FUNCTION__, chip);

	if (HW){
		unsigned addr;
		for (addr = 0; addr <= AD7134_MAXREG; ++addr){
			unsigned char txd[3] = { AD7134_RD|addr, 0, 0 };
			unsigned char rxd[3];
			CRC3(txd);
			status = acq465_spi_read(adev, chip, txd, CMDLEN, rxd);
			if (status != 0){
				char buf[16];
				dev_err(DEVP(adev), "%s %s fail %d", __FUNCTION__, make_cmd_string(buf, txd, CMDLEN), status);
				return status;
			}
			cache[addr] = rxd[1];
		}
	}
	memcpy(adev->cli_buf.va+chip*REGS_LEN, cache, REGS_LEN);
	return status;
}

static int ad7134_cache_flush(struct acq465_dev* adev, int chip)
/* flush changes in cache to device */
{
	unsigned char *cache = adev->dev_buf.va + chip*REGS_LEN;
	unsigned char *clibuf = adev->cli_buf.va + chip*REGS_LEN;
	char buf[16];
	int status = 0;
	int reg;

	dev_dbg(DEVP(adev), "%s chip %x", __FUNCTION__, chip);

	for (reg = 0; reg <= AD7134_MAXREG; ++reg){
		if (cache[reg] != clibuf[reg]){
			unsigned char cmd[3];

			cache[reg] = clibuf[reg];
			cmd[0] = reg;
			cmd[1] = cache[reg];
			cmd[2] = CRC3(cmd);
			if (HW){
				status = acq465_spi_write(adev, chip, cmd, CMDLEN);
			}
			dev_dbg(DEVP(adev), "%s %s lcs:%02x cmd: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, cmd, CMDLEN), status);

		}
	}
	return status;
}

static int ad7134_reset(struct acq465_dev* adev, int chip)
{
/*
	char lock[3] 	= { 0xff, 0xff, 0xff };
	char unlock[3] 	= { 0xff, 0xff, 0xfe };

	acq465_spi_write(adev, chip, lock, 3);
	acq465_spi_write(adev, chip, unlock, 3);
*/
	char soft_reset[3] 		= { 0x00, 0x98, 0x0 };
	char soft_reset_release[3] 	= { 0x00, 0x18, 0x0 };

	acq465_spi_write(adev, chip, soft_reset, CMDLEN);
	acq465_spi_write(adev, chip, soft_reset_release, CMDLEN);
	return 0;
}


#define MCM_POLL_RATE_HZ	50

static long ad7134_monitor_mclk(struct acq465_dev* adev, struct MCM* mcm)
// MCLK=24MHz. CTR=MCLK/12000 = 2,000 Hz. 8 overflows/sec.
// poll at 25Hz, accumulate over period.
{
	unsigned chip = mcm->lcs;
	unsigned char *cache = adev->dev_buf.va + chip*REGS_LEN + AD7413_MCLK_COUNTER;
	unsigned char *clibuf = adev->cli_buf.va + chip*REGS_LEN + AD7413_MCLK_COUNTER;

	const unsigned max_step = mcm->sec * MCM_POLL_RATE_HZ;
	const unsigned sleepms = 1000/MCM_POLL_RATE_HZ;

	unsigned char tx[3] = {AD7134_RD|AD7413_MCLK_COUNTER, };
	unsigned char rx[3] = {};

	unsigned total_count = 0;
	unsigned char this_count, last_count;
	unsigned step = 0;

	int rc = 0;

	for(; step < max_step; ++step){
		if (!dummy_mclk){
			rc = acq465_spi_read(adev, mcm->lcs, tx, CMDLEN, rx);
		}
		if (rc != 0){
			dev_err(DEVP(adev), "%s acq465_spi_read fail", __FUNCTION__);
			return rc;
		}
		this_count = rx[1];

		if (step != 0){
			if (this_count < last_count){
				total_count += 0x100-last_count + this_count;
				mclk_counts[0]++;
			}else{
				total_count += this_count - last_count;
				mclk_counts[1]++;
			}
		}
		*clibuf = *cache = last_count = this_count;
		msleep(sleepms);
	}
	mcm->count = total_count;
	if (dummy_mclk){
		mcm->count = max_step+sleepms;
	}
	return rc;
}

static long acq465_dig_if_release(struct acq465_dev* adev)
{
#ifdef AD7134_LIKES_IT
	char dig_if_release[3] 	= { 0x01, 0x80, 0x0 };
	acq465_spi_write(adev, ACQ465_LCS_BROADCAST, dig_if_release, CMDLEN);
#endif
	acq465_lcs(get_site(adev), 0);
	dev_dbg(&adev->pdev->dev, "%s 99\n", __FUNCTION__);
	return 0;
}

static long acq465_dig_if_reset(struct acq465_dev* adev, unsigned do_it)
{
	dev_dbg(&adev->pdev->dev, "%s 01 do_it:%d\n", __FUNCTION__, do_it);

	if (do_it){
		char buf[16];
		unsigned char clr[3] 	= { 0x01, 0x00, 0x0 };
		unsigned char cmd[3] 	= { 0x01, 0x82, 0x0 };
		int chip = ACQ465_LCS_BROADCAST;
		int status;

		if (spi_clr&1){
			status = acq465_spi_write(adev, chip, clr, CMDLEN);
			dev_dbg(DEVP(adev), "%s %s lcs:%02x cmd: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, clr, CMDLEN), status);
		}
		status = acq465_spi_write(adev, chip, cmd, CMDLEN);
		if (spi_clr&2){
			status = acq465_spi_write(adev, chip, clr, CMDLEN);
			dev_dbg(DEVP(adev), "%s %s lcs:%02x cmd: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, clr, CMDLEN), status);
		}
		dev_dbg(DEVP(adev), "%s %s lcs:%02x cmd: %s status:%d", __FUNCTION__, HW? "HW": "sim", chip, make_cmd_string(buf, cmd, CMDLEN), status);
	}
	// clean up on release(), allow potentially multiple ACQ465 to be init at once.
	return 0;
}

#define acq465_of_match 0

struct acq465_dev* acq465_allocate_dev(struct platform_device *pdev)
{
	int site = pdev->id;
	struct acq465_dev* adev =
		kzalloc(sizeof(struct acq465_dev), GFP_KERNEL);

	if (adev == NULL){
		return NULL;
	}
	acq465_devs[site] = adev;

	adev->pdev = pdev;
	snprintf(adev->devname, 16, "%s.%d", pdev->name, pdev->id);
	mutex_init(&adev->sem);

	return adev;
}

int acq465_open(struct inode *inode, struct file *file)
{
        file->private_data = container_of(inode->i_cdev, struct acq465_dev, cdev);
        return 0;
}

int acq465_release(struct inode *inode, struct file *file)
{
	struct acq465_dev* adev = (struct acq465_dev*)file->private_data;
	if (mutex_is_locked(&adev->sem) && adev->owner == current){
		acq465_dig_if_release(adev);
		mutex_unlock(&adev->sem);
	}
	return 0;
}

int acq465_cli_buffer_mmap(struct file* file, struct vm_area_struct* vma)
/**
 * mmap the host buffer.
 */
{
	struct acq465_dev* adev = (struct acq465_dev*)file->private_data;

	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = 2*PAGE_SIZE;
	unsigned pfn = client_spi_buffer->pa >> PAGE_SHIFT;

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
acq465_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct acq465_dev* adev = (struct acq465_dev*)file->private_data;
	unsigned chip = arg;		// for ioctls taking a single unsigned arg LCS
	void* varg = (void*)arg;        // for ioctls with structure arg
	long rc;

	dev_dbg(DEVP(adev), "%s cmd:%u arg:%lu\n", __FUNCTION__, cmd, arg);

	rc = mutex_lock_interruptible(&adev->sem);

	if (rc){
		dev_dbg(DEVP(adev), "%s mutex is locked already, drop out\n", __FUNCTION__);
		return rc;
	}else{
		adev->owner = current;
	}
	/* inside mutex */
	switch(cmd){
	case ACQ465_CACHE_INVALIDATE:
		rc = ad7134_cache_invalidate(adev, chip);
		break;
	case ACQ465_CACHE_FLUSH:
		rc = ad7134_cache_flush(adev, chip);
		break;
	case ACQ465_RESET:
		rc = ad7134_reset(adev, chip);
		break;
	case ACQ465_MCLK_MONITOR: {
		struct MCM mcm;
		long rc;
		COPY_FROM_USER(&mcm, varg, sizeof(struct MCM));
		rc = ad7134_monitor_mclk(adev, &mcm);
		COPY_TO_USER(varg, &mcm, sizeof(struct MCM));
		break;
	}
	case ACQ465_DIG_IF_RESET: {
		unsigned do_it = arg;

		dev_dbg(DEVP(adev), "%s ACQ465_DIG_IF_RESET\n", __FUNCTION__);
		rc = acq465_dig_if_reset(adev, do_it);
		return rc;					/* leave release() to clean up the mutex */
	} default:
		rc = -ENODEV;
		break;
	}

	{
		unsigned long flags;
		spinlock_t lock;
		spin_lock_init(&lock);
		spin_lock_irqsave(&lock, flags);
		adev->owner = 0;		// race here.. so we made it atomic ..
		mutex_unlock(&adev->sem);
		spin_unlock_irqrestore(&lock, flags);
	}
	dev_dbg(DEVP(adev), "%s 99 cmd:%u arg:%lu rc:%ld\n", __FUNCTION__, cmd, arg, rc);
	return rc;
}

struct file_operations acq465_fops = {
        .owner = THIS_MODULE,
        .open = acq465_open,
        .mmap = acq465_cli_buffer_mmap,
        .unlocked_ioctl = acq465_unlocked_ioctl,
	.release = acq465_release
};

#define CHIPFMT	" - - - - - - - - - - - - chip:%c - - - - - - - - - - - -\n"

static void print_header(struct seq_file *s, char chip)
{
	seq_printf(s, CHIPFMT, chip+0);
}
/* read 0x100 bytes, 0x10 bytes at a time */
static void *acq465_proc_seq_start_buffers(struct seq_file *s, loff_t *pos)
{
        if (*pos == 0) {
        	struct acq465_dev *adev = s->private;
        	dev_dbg(DEVP(adev), "acq465_proc_seq_start_buffers() %s adev:%p clibuf:%p",
        			adev->devname, adev, adev->cli_buf.va);
        	print_header(s, 'A');
        	return adev->cli_buf.va;
        }

        return NULL;
}
#define BUFREAD	0x10	/* words to read 	*/
#define BUFMAX  0x100	/* total words to read 	*/
static int acq465_proc_seq_show_spibuf_row(struct seq_file *s, void *v)
{
	struct acq465_dev *adev = s->private;
	unsigned char* regs = (unsigned char*)v;
	unsigned char* base = adev->cli_buf.va;
	unsigned offregs;
	int ir;

	while ((offregs = (regs - base)) > AD7134_MAXREG){
		base += REGS_LEN;
	}

	seq_printf(s, "| %02x: ", offregs%AD7134_MAXREG);
	for (ir = 0; ir < BUFREAD-1; ++ir){
		seq_printf(s, "%02x ", regs[ir]);
	}
	seq_printf(s, "%02x |\n", regs[ir]);
	return 0;
}

static void* acq465_proc_seq_next_buffers(
		struct seq_file *s, void* v, loff_t *pos)
{
	*pos += BUFREAD;
	if (*pos < MODULE_SPI_BUFFER_LEN){
		loff_t offset = *pos%REGS_LEN;
		if (offset  < AD7134_REGSLEN){
			return v + BUFREAD;
		}else{
			struct acq465_dev *adev = s->private;

			int chip = *pos/REGS_LEN + 1;
			if (chip > 7){
				return NULL;
			}else{
				print_header(s, 'A'+chip);
				*pos = chip * REGS_LEN;
				return adev->cli_buf.va + *pos;
			}
		}
	}else{
		return NULL;
	}
}
static void acq465_proc_seq_stop(struct seq_file *s, void* v)
{

}

static int acq465_proc_open_spibuf(struct inode *inode, struct file *file)
{
	static struct seq_operations proc_seq_ops_spi_buf = {
	        .start = acq465_proc_seq_start_buffers,
	        .next = acq465_proc_seq_next_buffers,
	        .stop = acq465_proc_seq_stop,
	        .show = acq465_proc_seq_show_spibuf_row
	};
	int rc = seq_open(file, &proc_seq_ops_spi_buf);
	if (rc == 0){
		struct seq_file *m = file->private_data;
		struct acq465_dev* adev = PDE_DATA(inode);
		m->private = PDE_DATA(inode);

		dev_dbg(DEVP(adev), "acq465_proc_open_spibuf() %s adev:%p", adev->devname, adev);
	}
	return rc;
}

struct file_operations acq465_proc_fops = {
	        .owner = THIS_MODULE,
	        .open = acq465_proc_open_spibuf,
	        .read = seq_read,
	        .llseek = seq_lseek,
	        .release = seq_release
};


static int acq465_probe(struct platform_device *pdev)
{
	struct acq465_dev* adev = acq465_allocate_dev(pdev);
	struct device* dev = &pdev->dev;
	int rc;

	dev_info(dev, "acq465_probe: %d %s", pdev->id, adev->devname);
	rc = alloc_chrdev_region(&adev->devno, 0, 0, adev->devname);
	if (rc < 0) {
		dev_err(&pdev->dev, "unable to register chrdev\n");
	        goto fail;
	}

        cdev_init(&adev->cdev, &acq465_fops);
        adev->cdev.owner = THIS_MODULE;
        rc = cdev_add(&adev->cdev, adev->devno, 1);
        if (rc < 0){
        	goto fail;
        }

        if (driver_spi_buffer == 0){
        	driver_spi_buffer = hbm_allocate1(dev, TOTAL_SPI_BUFFER_LEN, 0, DMA_BIDIRECTIONAL);
        	client_spi_buffer = hbm_allocate1(dev, TOTAL_SPI_BUFFER_LEN, 0, DMA_BIDIRECTIONAL);

        	if (set_ident){
        		ident(DRV_VA(1), 0);
        		ident(CLI_VA(1), 1);
        	}
        }
        /* there's no actual DMA, we're after the pa, but DMA_NONE BUGs */
        adev->cli_buf.pa = CLI_PA(pdev->id);
        adev->cli_buf.va = CLI_VA(pdev->id);

        if (!set_ident){
        	memset(adev->cli_buf.va, 0, MODULE_SPI_BUFFER_LEN);
        }

        adev->dev_buf.pa = DRV_PA(pdev->id);
        adev->dev_buf.va = DRV_VA(pdev->id);

        if (acq465_proc_root == 0){
        	acq465_proc_root = proc_mkdir("driver/acq465", 0);
        }

        proc_create_data("spibuf", 0,
        		proc_mkdir(adev->devname, acq465_proc_root),
        		&acq465_proc_fops, adev);

	return 0;

fail:
	kfree(adev);
	return -1;
}

static int acq465_remove(struct platform_device *pdev)
{
	int site = pdev->id;

	kfree(acq465_devs[site]);
	acq465_devs[site] = 0;
	return -1;
}

static int ad7134spi_probe(struct spi_device *spi)
{
	struct acq465_dev* adev = acq465_devs[cs2site(spi->chip_select)];
	adev->spi = spi;
	dev_info(&spi->dev, "ad7134spi_probe() bus:%d cs:%d",
			spi->master->bus_num, spi->chip_select);

	//kthread_run(read_all_chips, adev, "%s.readall", adev->devname);
	return 0;
}
static int ad7134spi_remove(struct spi_device *spi)
{
	return 0;
}

static void __init acq465_init_site(int site)
{
	static struct spi_board_info ad7134spi_spi_slave_info = {
			.modalias	= "ad7134spi",
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

	ad7134spi_spi_slave_info.bus_num = spi_bus_num;
	platform_device_register(pdev);

	ad7134spi_spi_slave_info.chip_select = site - 1;
	spi_register_board_info(&ad7134spi_spi_slave_info, 1);
}

static void __init acq465_remove_site(int site)
{
	printk("acq465_remove_site %d ERROR removal not supported\n", site);
}
static struct platform_driver acq465_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = acq465_of_match,
        },
        .probe = acq465_probe,
        .remove = acq465_remove,
};

static struct spi_driver ad7134spi_driver = {
	.driver = {
		.name	= "ad7134spi",
		.owner	= THIS_MODULE,
	},
	//.id_table = m25p_ids,
	.probe	= ad7134spi_probe,
	.remove	= ad7134spi_remove,
};

static void __exit acq465_exit(void)
{
	for (; n_acq465--;){
		acq465_remove_site(acq465_sites[n_acq465]);
	}
	platform_driver_unregister(&acq465_driver);
	spi_unregister_driver(&ad7134spi_driver);
}

extern void acq480_hook_spi(void);

static int __init acq465_init(void)
{
        int status = 0;


	printk("D-TACQ ACQ465 Driver %s\n", REVID);

	init_crc();
	platform_driver_register(&acq465_driver);
	acq465_proc_root = proc_mkdir("driver/acq465", 0);

	spi_register_driver(&ad7134spi_driver);

	acq480_hook_spi();

	for (n_acq465 = 0; n_acq465 < acq465_sites_count; ++n_acq465){
		acq465_init_site(acq465_sites[n_acq465]);
	}
        return status;
}

module_init(acq465_init);
module_exit(acq465_exit);



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ465ELF SPI Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
