/* ------------------------------------------------------------------------- */
/* ACQ420_FMC_drv.c  ACQ420 FMC D-TACQ DRIVER		                     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2012 Craig Noble, D-TACQ Solutions Ltd                    *
 *                      <craig dot noble at D hyphen TACQ dot com>           *
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/sizes.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>
#include <asm/io.h>
#include <mach/pl330.h>
#include <linux/of.h>

/* Define debugging for use during our driver bringup */
#undef PDEBUG
#define PDEBUG(fmt, args...) printk(KERN_INFO fmt, ## args)

/* Offsets for control registers in the AXI MM2S FIFO */
#define AXI_FIFO              0x0
#define AXI_FIFO_LEN          0x1000

#define ALG_CTRL		0x1000
#define ALG_HITIDE		0x1004
#define ALG_SAMPLES		0x1008
#define ALG_STATUS		0x100C
#define ALG_INT_CTRL		0x1010
#define ALG_INT_FORCE		0x1014
#define ALG_INT_STAT		0x1018


#define ALG_CTRL_ADC_ENABLE	(1 << 4)
#define ALG_CTRL_ADC_RESET	(1 << 3)
#define ALG_CTRL_FIFO_ENABLE	(1 << 2)
#define ALG_CTRL_FIFO_RESET	(1 << 1)
#define ALG_CTRL_ALG_ENABLE	(1 << 0)

#define ADC_HT_INT		91
#define HITIDE			0x40

#define MODULE_NAME             "acq420"
#define XFIFO_DMA_MINOR         0

int acq420_major = 60;
module_param(acq420_major, int, 0);

dma_addr_t write_buffer;

DECLARE_WAIT_QUEUE_HEAD(acq420_wait);

struct acq420_dev {
	dev_t devno;
	struct mutex mutex;
	struct cdev cdev;
	struct platform_device *pdev;

	struct pl330_client_data *client_data;

	u32 dma_channel;
	u32 fifo_depth;
	u32 burst_length;
	u32 irq;		/* device IRQ number	*/
	u32 DMA_READY;

	/* Current DMA buffer information */
	dma_addr_t buffer_d_addr;
	void *buffer_v_addr;
	size_t count;
	size_t this_count;
	int busy;

	/* Hardware device constants */
	u32 dev_physaddr;
	void *dev_virtaddr;
	u32 dev_addrsize;

	/* Driver reference counts */
	u32 writers;

	/* Driver statistics */
	u32 bytes_written;
	u32 writes;
	u32 reads;
	u32 opens;
	u32 closes;
	u32 errors;
};

struct acq420_dev *acq420_dev;

static void acq420_reset_fifo(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_CTRL);
	iowrite32((status | ALG_CTRL_ADC_RESET | ALG_CTRL_FIFO_RESET)
			, acq420_dev->dev_virtaddr + ALG_CTRL);

	/* Release reset */
	iowrite32(status, acq420_dev->dev_virtaddr + ALG_CTRL);

	// printk("Using Control Address : 0x%08x\n", acq420_dev->dev_virtaddr + ALG_CTRL);
	// printk("Got 0x%08x from control address\n");

}

static void acq420_enable_fifo(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_CTRL);
	iowrite32((status | ALG_CTRL_ADC_ENABLE | ALG_CTRL_FIFO_ENABLE | ALG_CTRL_ALG_ENABLE)
			, acq420_dev->dev_virtaddr + ALG_CTRL);

}

static void acq420_disable_fifo(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_CTRL);
	status &= ~(ALG_CTRL_ADC_ENABLE | ALG_CTRL_FIFO_ENABLE | ALG_CTRL_ALG_ENABLE);
	iowrite32(status, acq420_dev->dev_virtaddr + ALG_CTRL);

}

static u32 acq420_get_fifo_status(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_SAMPLES);
	return status;
}

static void acq420_enable_interrupt(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_INT_CTRL);
	iowrite32(HITIDE
			, acq420_dev->dev_virtaddr + ALG_HITIDE);
	iowrite32((status | 0x1)
			, acq420_dev->dev_virtaddr + ALG_INT_CTRL);

}

static void acq420_disable_interrupt(void)
{
	u32 status, control;
	control = ioread32(acq420_dev->dev_virtaddr + ALG_INT_CTRL);
	status = ioread32(acq420_dev->dev_virtaddr + ALG_INT_STAT);

	// printk("Interrupt status is 0x%08x\n", status);
	control &= ~0x1;
	// printk("New interrupt enable is 0x%08x\n", control);
	iowrite32((0x0)
			, acq420_dev->dev_virtaddr + ALG_INT_CTRL);

}

static void acq420_clear_interrupt(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_INT_STAT);
	iowrite32(status, acq420_dev->dev_virtaddr + ALG_INT_STAT);

}

// static void acq420_force_interrupt(int interrupt)
// {
	// u32 status;
	// status = ioread32(acq420_dev->dev_virtaddr + ALG_INT_FORCE);
	// iowrite32((interrupt)
			// , acq420_dev->dev_virtaddr + ALG_INT_FORCE);

// }

static u32 acq420_get_interrupt(void)
{
	u32 status;
	status = ioread32(acq420_dev->dev_virtaddr + ALG_INT_STAT);
	return status;
}

// static int __init acq420_findirq(void)
// {
	// int timeout = 20;
	// unsigned long cookie;
	// int foundIRQ;

	// // DBG(SMC_DEBUG_FUNC, "--> %s\n", __FUNCTION__);

	// cookie = probe_irq_on();

	// printk("Pre-probe interrupt mask is : 0x%08x\n", (u32)cookie);
	// /*
	 // * Force a SW interrupt
	 // */
	// acq420_enable_interrupt();
	// acq420_force_interrupt(0x1);

	// /*
	 // * Wait until positive that the interrupt has been generated
	 // */
	// do {
		// int int_status;
		// udelay(10);
		// int_status = acq420_get_interrupt();
		// printk("IRQ status is 0x%08x\n", int_status);
		// if (int_status & 0x1)
			 // break;		/* got the interrupt */
	// } while (--timeout);

	// /*
	 // * there is really nothing that I can do here if timeout fails,
	 // * as autoirq_report will return a 0 anyway, which is what I
	 // * want in this case.	 Plus, the clean up is needed in both
	 // * cases.
	 // */

	 // foundIRQ =  probe_irq_off(cookie);
	 // printk("Auto-detected IRQ at %d\n", foundIRQ);

	// /* and disable all interrupts again */
	// acq420_force_interrupt(0x0);
	// acq420_disable_interrupt();

	// /* and return what I found */
	// return foundIRQ;
// }

/* File operations */
int acq420_open(struct inode *inode, struct file *filp)
{
        struct acq420_dev *dev;
        int retval;

        retval = 0;
        dev = container_of(inode->i_cdev, struct acq420_dev, cdev);
        filp->private_data = dev;       /* For use elsewhere */

        if (mutex_lock_interruptible(&dev->mutex)) {
                return -ERESTARTSYS;
        }

        /* We're only going to allow one write at a time, so manage that via
         * reference counts
         */
        switch (filp->f_flags & O_ACCMODE) {
        case O_RDONLY:
                break;
        case O_WRONLY:
                if (dev->writers || dev->busy) {
                        retval = -EBUSY;
                        goto out;
                }
                else {
                        dev->writers++;
                }
                break;
        case O_RDWR:
        default:
                if (dev->writers || dev->busy) {
                        retval = -EBUSY;
                        goto out;
                }
                else {
                        dev->writers++;
                }
        }

        dev->opens++;

out:
        mutex_unlock(&dev->mutex);
        return retval;
}

int acq420_release(struct inode *inode, struct file *filp)
{
        struct acq420_dev *dev = filp->private_data;

        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }

        /* Manage writes via reference counts */
        switch (filp->f_flags & O_ACCMODE) {
        case O_RDONLY:
                break;
        case O_WRONLY:
                dev->writers--;
                break;
        case O_RDWR:
        default:
                dev->writers--;
        }

        dev->closes++;

        mutex_unlock(&dev->mutex);

        return 0;
}

static void acq420_fault_callback(unsigned int channel,
        unsigned int fault_type,
        unsigned int fault_address,
        void *data)
{
        struct acq420_dev *dev = data;

        dev_err(&dev->pdev->dev,
                "DMA fault type 0x%08x at address 0x%0x on channel %d\n",
                fault_type, fault_address, channel);

        dev->errors++;
        acq420_reset_fifo();
        dev->busy = 0;
        wake_up_interruptible(&acq420_wait);
}

static void acq420_done_callback(unsigned int channel, void *data)
{
        struct acq420_dev *dev = data;

	   // printk("DMA Done!");

        dev->bytes_written += dev->count;
        dev->busy = 0;

        wake_up_interruptible(&acq420_wait);
}

ssize_t acq420_read(struct file *filp, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct acq420_dev *dev = filp->private_data;

	// u64 checkval= 0;
	int retval = 0;

	if (mutex_lock_interruptible(&dev->mutex)) {
		return -EINTR;
	}

	dev->reads++;
	dev->count = count;
	dev->this_count = 0;

	/** @@todo pgm: in a streaming system, preallocate streaming dma buffers (not coherent) */
	/* Allocate a DMA buffer for the transfer */
	dev->buffer_v_addr = 0;
	dev->buffer_v_addr = dma_zalloc_coherent(&dev->pdev->dev, count,
		&dev->buffer_d_addr, GFP_KERNEL);
	if (!dev->buffer_v_addr) {
		dev_err(&dev->pdev->dev,
			"coherent DMA buffer allocation failed\n");
		retval = -ENOMEM;
		goto fail_buffer;
	}

	if (request_dma(dev->dma_channel, MODULE_NAME)) {
		dev_err(&dev->pdev->dev,
			"unable to alloc DMA channel %d\n",
			dev->dma_channel);
		retval = -EBUSY;
		goto fail_client_data;
	}

	acq420_enable_fifo();
	acq420_reset_fifo();

	dev->DMA_READY = 1;

	/*Wait for FIFO to fill*/
	acq420_enable_interrupt();

	/* Kick off the DMA */
	mutex_unlock(&dev->mutex);

	do{
		wait_event_interruptible(acq420_wait, dev->busy == 0);
	}while(dev->this_count < count);

	/* Cleanup after transfer */
	// printk("Clearing the capture FIFO\n");
	acq420_reset_fifo();

	/* Retrieve our DMA buffer with the user data */
	copy_to_user(buf, dev->buffer_v_addr, count);

	/* Deallocate the DMA buffer and free the channel */
	free_dma(dev->dma_channel);

	// printk("Freeing memory!\n");
	dma_free_coherent(&dev->pdev->dev, count, dev->buffer_v_addr,
		dev->buffer_d_addr);

	return count;

fail_client_data:
	dma_free_coherent(&dev->pdev->dev, count, dev->buffer_v_addr,
		dev->buffer_d_addr);
fail_buffer:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t acq420_write(struct file *filp, const char __user *buf, size_t count,
        loff_t *f_pos)
{
        struct acq420_dev *dev = filp->private_data;
        size_t transfer_size;

        int retval = 0;

        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }

        dev->writes++;

        transfer_size = count;
        if (count > dev->fifo_depth) {
                transfer_size = dev->fifo_depth;
        }

        /* Allocate a DMA buffer for the transfer */
        dev->buffer_v_addr = dma_alloc_coherent(&dev->pdev->dev, transfer_size,
                &dev->buffer_d_addr, GFP_KERNEL);
        if (!dev->buffer_v_addr) {
                dev_err(&dev->pdev->dev,
                        "coherent DMA buffer allocation failed\n");
                retval = -ENOMEM;
                goto fail_buffer;
        }

        PDEBUG("dma buffer alloc - d @0x%0x v @0x%0x\n",
                (u32)dev->buffer_d_addr, (u32)dev->buffer_v_addr);

        if (request_dma(dev->dma_channel, MODULE_NAME)) {
                dev_err(&dev->pdev->dev,
                        "unable to alloc DMA channel %d\n",
                        dev->dma_channel);
                retval = -EBUSY;
                goto fail_client_data;
        }

        dev->busy = 1;
        dev->count = transfer_size;

        set_dma_mode(dev->dma_channel, DMA_MODE_WRITE);
        set_dma_addr(dev->dma_channel, dev->buffer_d_addr);
        set_dma_count(dev->dma_channel, transfer_size);
        set_pl330_client_data(dev->dma_channel, dev->client_data);
        set_pl330_done_callback(dev->dma_channel,
                acq420_done_callback, dev);
        set_pl330_fault_callback(dev->dma_channel,
                acq420_fault_callback, dev);
        set_pl330_incr_dev_addr(dev->dma_channel, 0);

        /* Load our DMA buffer with the user data */
        copy_from_user(dev->buffer_v_addr, buf, transfer_size);

        acq420_reset_fifo();
        /* Kick off the DMA */
        enable_dma(dev->dma_channel);

        mutex_unlock(&dev->mutex);

        wait_event_interruptible(acq420_wait, dev->busy == 0);

        /* Deallocate the DMA buffer and free the channel */
        free_dma(dev->dma_channel);

        dma_free_coherent(&dev->pdev->dev, dev->count, dev->buffer_v_addr,
                dev->buffer_d_addr);

        PDEBUG("dma write %d bytes\n", transfer_size);

        return transfer_size;

fail_client_data:
        dma_free_coherent(&dev->pdev->dev, transfer_size, dev->buffer_v_addr,
                dev->buffer_d_addr);
fail_buffer:
        mutex_unlock(&dev->mutex);
        return retval;
}

static irqreturn_t fire_dma(int irq, void *dev_id)
{
	struct acq420_dev *dev = (struct acq420_dev *)dev_id;
	u32 status;

	do{
		status = acq420_get_fifo_status();
		status *= 4;
		// printk("FIFO contains %d samples\n", status);
		// printk("Transfer wants %d bytes\n", dev->count);
		if ((dev->this_count + status) >= dev->count)
		{
			acq420_disable_fifo();
			status = dev->count - dev->this_count;
			if (status == 0x0)
			{
				// printk("Breaking!\n");
				break;
			}
		}

		if (status >= 0x1000)
		{
			status=0x1000;
		}
		/* Lock the DMA device */
		dev->busy = 1;

		// printk("Loading DMA for %d bytes\n", (status));
		// printk("Trigger point was set at %d\n", HITIDE);

		/* Setup DMA */
		set_dma_mode(dev->dma_channel, DMA_MODE_READ);
		set_dma_addr(dev->dma_channel, (dev->buffer_d_addr + dev->this_count));
		set_dma_count(dev->dma_channel, (size_t)(status));
		set_pl330_client_data(dev->dma_channel, dev->client_data);
		set_pl330_done_callback(dev->dma_channel,
			acq420_done_callback, dev);
		set_pl330_fault_callback(dev->dma_channel,
			acq420_fault_callback, dev);
		set_pl330_incr_dev_addr(dev->dma_channel, 0);

		/* Update the Count status */
		dev->this_count += status;

		/* Kick off the DMA */
		enable_dma(dev->dma_channel);

		/* Wait for Completion*/
		wait_event_interruptible(acq420_wait, dev->busy == 0);

		status = acq420_get_fifo_status();
		// printk("FIFO has been drained to %d samples\n", status);

	}while(status >= HITIDE-16);


	acq420_enable_interrupt();

	return IRQ_HANDLED;
}

static irqreturn_t acq420_int_handler(int irq, void *dev_id)
{
	irqreturn_t irq_status;
	struct acq420_dev *dev = (struct acq420_dev *)dev_id;
	u32 status = acq420_get_interrupt();
	// u32 samples = acq420_get_fifo_status();


	iowrite32((0xCAFEBABE) , acq420_dev->dev_virtaddr);

	// printk("FIFO contains %d samples\n", samples);

	if (status == 0x1 && dev->DMA_READY == 1) {
		dev->DMA_READY = 0;
	}
	acq420_disable_interrupt();
	acq420_clear_interrupt();
	// printk("Got an IRQ!\n");

	irq_status = IRQ_WAKE_THREAD;
	return irq_status;
}

struct file_operations acq420_fops = {
        .owner = THIS_MODULE,
        .read = acq420_read,
        .write = acq420_write,
        .open = acq420_open,
        .release = acq420_release
};

/* Driver /proc filesystem operations so that we can show some statistics */
static void *acq420_proc_seq_start(struct seq_file *s, loff_t *pos)
{
        if (*pos == 0) {
                return acq420_dev;
        }

        return NULL;
}

static void *acq420_proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
        (*pos)++;
        return NULL;
}

static void acq420_proc_seq_stop(struct seq_file *s, void *v)
{
}

static int acq420_proc_seq_show(struct seq_file *s, void *v)
{
        struct acq420_dev *dev;

        dev = v;
        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }

        seq_printf(s, "\nFIFO DMA Test:\n\n");
        seq_printf(s, "Device Physical Address: 0x%0x\n", dev->dev_physaddr);
        seq_printf(s, "Device Virtual Address:  0x%0x\n",
                (u32)dev->dev_virtaddr);
        seq_printf(s, "Device Address Space:    %d bytes\n", dev->dev_addrsize);
        seq_printf(s, "DMA Channel:             %d\n", dev->dma_channel);
        seq_printf(s, "FIFO Depth:              %d bytes\n", dev->fifo_depth);
        seq_printf(s, "Burst Length:            %d words\n", dev->burst_length);
        seq_printf(s, "\n");
        seq_printf(s, "Opens:                   %d\n", dev->opens);
        seq_printf(s, "Writes:                  %d\n", dev->writes);
        seq_printf(s, "Bytes Written:           %d\n", dev->bytes_written);
        seq_printf(s, "Closes:                  %d\n", dev->closes);
        seq_printf(s, "Errors:                  %d\n", dev->errors);
        seq_printf(s, "Busy:                    %d\n", dev->busy);
        seq_printf(s, "\n");

        mutex_unlock(&dev->mutex);
        return 0;
}

/* SEQ operations for /proc */
static struct seq_operations acq420_proc_seq_ops = {
        .start = acq420_proc_seq_start,
        .next = acq420_proc_seq_next,
        .stop = acq420_proc_seq_stop,
        .show = acq420_proc_seq_show
};

static int acq420_proc_open(struct inode *inode, struct file *file)
{
        return seq_open(file, &acq420_proc_seq_ops);
}

static struct file_operations acq420_proc_ops = {
        .owner = THIS_MODULE,
        .open = acq420_proc_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};

static int acq420_remove(struct platform_device *pdev)
{
        cdev_del(&acq420_dev->cdev);

        remove_proc_entry("driver/acq420", NULL);

        unregister_chrdev_region(acq420_dev->devno, 1);

        /* Unmap the I/O memory */
        if (acq420_dev->dev_virtaddr) {
                iounmap(acq420_dev->dev_virtaddr);
                release_mem_region(acq420_dev->dev_physaddr,
                        acq420_dev->dev_addrsize);
        }

        /* Free the PL330 buffer client data descriptors */
        if (acq420_dev->client_data) {
                kfree(acq420_dev->client_data);
        }

        if (acq420_dev) {
                kfree(acq420_dev);
        }

        return 0;
}

#ifdef CONFIG_OF
static struct of_device_id xfifodma_of_match[] __devinitdata = {
        { .compatible = "D-TACQ,ACQ420_FMC", },
        { /* end of table */}
};
MODULE_DEVICE_TABLE(of, xfifodma_of_match);
#else
#define xfifodma_of_match NULL
#endif /* CONFIG_OF */


static int acq420_probe(struct platform_device *pdev)
{
        int status, rc;
        struct proc_dir_entry *proc_entry;
        struct resource *acq420_resource;


        /* Get our platform device resources */
        PDEBUG("We have %d resources\n", pdev->num_resources);
        acq420_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (acq420_resource == NULL) {
                dev_err(&pdev->dev, "No resources found\n");
                return -ENODEV;
        }

        /* Allocate a private structure to manage this device */
        acq420_dev = kzalloc(sizeof(struct acq420_dev), GFP_KERNEL);
        if (acq420_dev == NULL) {
                dev_err(&pdev->dev,
                        "unable to allocate device structure\n");
                return -ENOMEM;
        }

        /* Get our device properties from the device tree, if they exist */
        if (pdev->dev.of_node) {
                if (of_property_read_u32(pdev->dev.of_node, "dma-channel",
                        &acq420_dev->dma_channel) < 0) {
                        dev_warn(&pdev->dev,
                                "DMA channel unspecified - assuming 0\n");
                        acq420_dev->dma_channel = 0;
                }
                dev_info(&pdev->dev,
                        "read DMA channel is %d\n", acq420_dev->dma_channel);
                if (of_property_read_u32(pdev->dev.of_node, "fifo-depth",
                        &acq420_dev->fifo_depth) < 0) {
                        dev_warn(&pdev->dev,
                                "depth unspecified, assuming 0xffffffff\n");
                        acq420_dev->fifo_depth = 0xffffffff;
                }
                dev_info(&pdev->dev,
                        "DMA fifo depth is %d\n", acq420_dev->fifo_depth);
                if (of_property_read_u32(pdev->dev.of_node, "burst-length",
                        &acq420_dev->burst_length) < 0) {
                        dev_warn(&pdev->dev,
                                "burst length unspecified - assuming 1\n");
                        acq420_dev->burst_length = 1;
                }
                dev_info(&pdev->dev,
                        "DMA burst length is %d\n",
                        acq420_dev->burst_length);
        }

        acq420_dev->pdev = pdev;

        acq420_dev->devno = MKDEV(acq420_major, XFIFO_DMA_MINOR);
        PDEBUG("devno is 0x%0x, pdev id is %d\n", acq420_dev->devno, XFIFO_DMA_MINOR);

        status = register_chrdev_region(acq420_dev->devno, 1, MODULE_NAME);
        if (status < 0) {
                dev_err(&pdev->dev, "unable to register chrdev %d\n",
                        acq420_major);
                goto fail;
        }

        /* Register with the kernel as a character device */
        cdev_init(&acq420_dev->cdev, &acq420_fops);
        acq420_dev->cdev.owner = THIS_MODULE;
        acq420_dev->cdev.ops = &acq420_fops;

        /* Initialize our device mutex */
        mutex_init(&acq420_dev->mutex);

        acq420_dev->dev_physaddr = acq420_resource->start;
        acq420_dev->dev_addrsize = acq420_resource->end -
                acq420_resource->start + 1;
        if (!request_mem_region(acq420_dev->dev_physaddr,
                acq420_dev->dev_addrsize, MODULE_NAME)) {
                dev_err(&pdev->dev, "can't reserve i/o memory at 0x%08X\n",
                        acq420_dev->dev_physaddr);
                status = -ENODEV;
                goto fail;
        }
        acq420_dev->dev_virtaddr = ioremap(acq420_dev->dev_physaddr,
                acq420_dev->dev_addrsize);
        PDEBUG("acq420: mapped 0x%0x to 0x%0x\n", acq420_dev->dev_physaddr,
                (unsigned int)acq420_dev->dev_virtaddr);

        acq420_dev->client_data = kmalloc(sizeof(struct pl330_client_data),
                GFP_KERNEL);
        if (!acq420_dev->client_data) {
                dev_err(&pdev->dev, "can't allocate PL330 client data\n");
                goto fail;
        }
        memset(acq420_dev->client_data, 0, sizeof(struct pl330_client_data));

        acq420_dev->client_data->dev_addr =
                acq420_dev->dev_physaddr + AXI_FIFO;
        acq420_dev->client_data->dev_bus_des.burst_size = 4;
        acq420_dev->client_data->dev_bus_des.burst_len =
                acq420_dev->burst_length;
        acq420_dev->client_data->mem_bus_des.burst_size = 4;
        acq420_dev->client_data->mem_bus_des.burst_len =
                acq420_dev->burst_length;

        status = cdev_add(&acq420_dev->cdev, acq420_dev->devno, 1);

        /* Create statistics entry under /proc */
        proc_entry = create_proc_entry("driver/acq420", 0, NULL);
        if (proc_entry) {
                proc_entry->proc_fops = &acq420_proc_ops;
        }


	acq420_dev->irq = ADC_HT_INT; //acq420_findirq();
	rc = request_threaded_irq(
		acq420_dev->irq, 
		acq420_int_handler, 
		fire_dma, 
		IRQF_SHARED, "ACQ420_FMC FIFO HighTide", acq420_dev);

	if (rc)	{
		printk("%s unable to secure IRQ%d\n", "ACQ420", 
			acq420_dev->irq);
		goto fail;
	}
        //acq420_reset_fifo();
        dev_info(&pdev->dev, "added ACQ420 FMC successfully\n");

        return 0;

        fail:
	   printk("Bailout!\n");
        acq420_remove(pdev);
        return status;
}

static struct platform_driver acq420_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = xfifodma_of_match,
        },
        .probe = acq420_probe,
        .remove = acq420_remove,
};

static void __exit acq420_exit(void)
{
	// free_irq(acq420_dev->irq, &acq420_dev->pdev->dev);
	platform_driver_unregister(&acq420_driver);
}

static int __init acq420_init(void)
{
        int status;

	printk("Loading D-TACQ ACQ420 FMC Driver for Slot %d\n", 0);
        status = platform_driver_register(&acq420_driver);

        return status;
}

module_init(acq420_init);
module_exit(acq420_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ ACQ420_FMC Driver");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION("0.1");




