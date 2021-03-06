/* ------------------------------------------------------------------------- */
/* acq400_wrdrv.c  D-TACQ ACQ400 White Rabbit  DRIVER	                     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2019 Peter Milne, D-TACQ Solutions Ltd                    *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * acq400_wrdrv.c
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */


#include "acq400.h"
#include "bolo.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"
#include "acq400_ui.h"



int wr_ts_inten = 1;
module_param(wr_ts_inten, int, 0444);
MODULE_PARM_DESC(wr_ts_inten, "1: enable Time Stamp interrupts");

int wr_pps_inten = 1;
module_param(wr_pps_inten, int, 0444);
MODULE_PARM_DESC(wr_pps_inten, "1: enable PPS interrupts");

int wr_tt_inten = 1;
module_param(wr_tt_inten, int, 0444);
MODULE_PARM_DESC(wr_tt_inten, "1: enable WRTT interrupts");

static inline u32 wr_ctrl_set(struct acq400_dev *adev, unsigned bits){
	u32 ctrl = acq400rd32(adev, WR_CTRL);
	ctrl |= bits;
	acq400wr32(adev, WR_CTRL, ctrl);
	return ctrl;
}
static inline u32 wr_ctrl_clr(struct acq400_dev *adev, unsigned bits){
	u32 ctrl = acq400rd32(adev, WR_CTRL);
	ctrl &= ~bits;
	acq400wr32(adev, WR_CTRL, ctrl);
	return ctrl;
}

static void wrtt_client_isr_action(struct acq400_sc_dev* sc_dev, struct WrClient* cli, u32 ts)
{
	cli->wc_ts = ts;
	cli->wc_count++;
	wake_up_interruptible(&cli->wc_waitq);
}

static irqreturn_t wr_ts_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	u32 int_sta = acq400rd32(adev, WR_CTRL);

	if (int_sta&WR_CTRL_TT0_STA){
		wrtt_client_isr_action(sc_dev, &sc_dev->wrtt_client0, acq400rd32(adev, WR_CUR_VERNR));
	}
	if (int_sta&WR_CTRL_TT1_STA){
		wrtt_client_isr_action(sc_dev, &sc_dev->wrtt_client1, acq400rd32(adev, WR_CUR_VERNR));
	}
	if (int_sta&WR_CTRL_TS_STA){
		wrtt_client_isr_action(sc_dev, &sc_dev->ts_client, acq400rd32(adev, WR_TAI_STAMP));
	}
	acq400wr32(adev, WR_CTRL, int_sta);
	return IRQ_HANDLED;	/* canned */
}
#if 0
static irqreturn_t wr_ts_kthread(int irq, void *dev_id)
/* keep the AO420 FIFO full. Recycle buffer only */
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t wr_pps_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	u32 int_sta = acq400rd32(adev, WR_CTRL);

	sc_dev->pps_client.wc_ts =acq400rd32(adev, WR_TAI_CUR_L);
	sc_dev->pps_client.wc_count++;

	acq400wr32(adev, WR_CTRL, int_sta);
	wake_up_interruptible(&sc_dev->pps_client.wc_waitq);
	return IRQ_HANDLED;	/* canned */
}
#if 0
static irqreturn_t wr_pps_kthread(int irq, void *dev_id)
/* keep the AO420 FIFO full. Recycle buffer only */
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	return IRQ_HANDLED;
}
#endif


void init_scdev(struct acq400_dev* adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	init_waitqueue_head(&sc_dev->pps_client.wc_waitq);
	init_waitqueue_head(&sc_dev->ts_client.wc_waitq);
	init_waitqueue_head(&sc_dev->wrtt_client0.wc_waitq);
	init_waitqueue_head(&sc_dev->wrtt_client1.wc_waitq);

	if (wr_ts_inten){
		wr_ctrl_set(adev, WR_CTRL_TS_INTEN);
	}
	if (wr_pps_inten){
		wr_ctrl_set(adev, WR_CTRL_PPS_INTEN);
	}
	if (wr_tt_inten){
		wr_ctrl_set(adev, WR_CTRL_TT1_INTEN);
		wr_ctrl_set(adev, WR_CTRL_TT0_INTEN);
	}
}

int acq400_wr_init_irq(struct acq400_dev* adev)
{
	int rc;
	int irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET);
	if (irq <= 0){
		return 0;
	}
	dev_info(DEVP(adev), "acq400_wr_init_irq %d", irq);

/*
	rc = devm_request_threaded_irq(
			DEVP(adev), irq, wr_ts_isr, wr_ts_kthread, IRQF_NO_THREAD,
			"wr_ts",	adev);
*/
	rc = devm_request_irq(DEVP(adev), irq, wr_ts_isr, IRQF_NO_THREAD, "wr_ts", adev);

	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d K414 KLUDGE IGNORE\n", irq);
		return 0;
	}

	irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET+1);
	if (irq <= 0){
		return 0;
	}

	dev_info(DEVP(adev), "acq400_wr_init_irq %d", irq);
/*
	rc = devm_request_threaded_irq(
			DEVP(adev), irq, wr_pps_isr, wr_pps_kthread, IRQF_NO_THREAD,
			"wr_pps",	adev);
*/
	rc = devm_request_irq(DEVP(adev), irq, wr_pps_isr, IRQF_NO_THREAD, "wr_pps", adev);
	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d K414 KLUDGE IGNORE\n", irq);
		return 0;
	}

	init_scdev(adev);
	return rc;
}

struct WrClient *getWCfromMinor(struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	unsigned minor = pdesc->minor;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	switch(minor){
	case ACQ400_MINOR_WR_TS:
		return &sc_dev->ts_client;
	case ACQ400_MINOR_WR_PPS:
		return &sc_dev->pps_client;
	case ACQ400_MINOR_WRTT:
		return &sc_dev->wrtt_client0;
	case ACQ400_MINOR_WRTT1:
		return &sc_dev->wrtt_client1;
	default:
		dev_err(DEVP(adev), "getWCfromMinor: BAD MINOR %u", minor);
		return 0;
	}
}

#define ACCMODE(file) (file->f_flags & O_ACCMODE)

#define READ_REQUESTED(file) (ACCMODE(file) == O_RDONLY || ACCMODE(file) == O_RDWR)

int _acq400_wr_open(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	unsigned minor = pdesc->minor;
	struct WrClient *wc = getWCfromMinor(file);

	dev_dbg(DEVP(adev), "_acq400_wr_open() %d minor %d flags %x", __LINE__, PD(file)->minor, file->f_flags);

	if (wc == 0){
		return -ENODEV;
	}else if (minor!=ACQ400_MINOR_WR_TS && (file->f_flags & O_WRONLY)) {		// only ts is writeable
		return -EACCES;
	}else if (READ_REQUESTED(file) && wc->wc_pid != 0 && wc->wc_pid != current->pid){
		return -EBUSY;
	}else{
		if ((file->f_flags & O_ACCMODE) != O_WRONLY){
			wc->wc_pid = current->pid;
			if ((file->f_flags & O_NONBLOCK) == 0){
				dev_dbg(DEVP(adev), "_acq400_wr_open clear %d", wc->wc_ts);
				wc->wc_ts = 0;
			}
		}
		return 0;
	}
}

int acq400_wr_release(struct inode *inode, struct file *file)
{
	if (PD(file)){
		kfree(PD(file));
		SETPD(file, 0);
	}
	return 0;
}

int acq400_wr_release_exclusive(struct inode *inode, struct file *file)
{
	struct WrClient *wc = getWCfromMinor(file);

	if (wc->wc_pid != 0 && wc->wc_pid == current->pid){
		wc->wc_pid = 0;
	}
	return acq400_wr_release(inode, file);
}

ssize_t acq400_wr_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct WrClient *wc = getWCfromMinor(file);
	u32 tmp;
	int rc;

	if (count < sizeof(u32)){
		return -EINVAL;
	}
	dev_dbg(DEVP(adev), "acq400_wr_read %d", wc->wc_ts);
	if (wait_event_interruptible(wc->wc_waitq, wc->wc_ts)){
		return -EINTR;
	}
	tmp = wc->wc_ts;
	wc->wc_ts = 0;

	rc = copy_to_user(buf, &tmp, sizeof(u32));

	if (rc){
		return -rc;
	}else{
		f_pos += sizeof(u32);
		return sizeof(u32);
	}
}


ssize_t acq400_wr_read_cur(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	int reg;
	int rc;

	switch(pdesc->minor){
	case ACQ400_MINOR_WR_CUR:
		reg = WR_CUR_VERNR; 	break;
	case ACQ400_MINOR_WR_CUR_TRG0:
		reg = WR_TAI_TRG0; 	break;
	case ACQ400_MINOR_WR_CUR_TRG1:
		reg = WR_TAI_TRG1; 	break;
	default:
		reg = WR_TAI_CUR_L;
	}
	if (count < sizeof(u32)){
		return -EINVAL;
	}else{
		u32 tmp = acq400rd32(adev, reg);
		rc = copy_to_user(buf, &tmp, sizeof(u32));

		if (rc){
			return -rc;
		}else{
			f_pos += sizeof(u32);
			return sizeof(u32);
		}
	}
}
ssize_t acq400_wr_write(
	struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_path_descriptor* pdesc = PD(file);
	u32 tmp;
	int rc;
	u32 reg;

	switch(pdesc->minor){
	case ACQ400_MINOR_WR_CUR_TRG0:
		reg = WR_TAI_TRG0;	break;
	case ACQ400_MINOR_WR_CUR_TRG1:
		reg = WR_TAI_TRG1; 	break;
	default:
		return -ENODEV;
	}
	if (count < sizeof(u32)){
		return -EINVAL;
	}
	dev_dbg(DEVP(adev), "acq400_wr_write() ");


	rc = copy_from_user(&tmp, buf, sizeof(u32));

	if (rc){
		return -1;
	}else{
		acq400wr32(adev, reg, tmp);
	}

	*f_pos += sizeof(u32);
	dev_dbg(DEVP(adev), "acq400_wr_write() return count:%u", sizeof(u32));

	return sizeof(u32);
}


int open_ok(struct inode *inode, struct file *file)
{
	return 0;
}
/*
[   85.627158] acq420 40000000.acq2006sc: acq400_open FAIL minor:23 rc:-13
ACQ400_MINOR_WRTT 23
EACCES		13

[   85.633927] acq420 40000000.acq2006sc: acq400_open FAIL minor:27 rc:-19
ACQ400_MINOR_WRTT1 27
ENODEV		19
 *
 */

int acq400_wr_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_wr = {
			.open = _acq400_wr_open,
			.read = acq400_wr_read,
			.release = acq400_wr_release_exclusive
	};
	static struct file_operations acq400_fops_wr_cur = {
			.open = open_ok,
			.read = acq400_wr_read_cur,
			.release = acq400_wr_release
	};
	static struct file_operations acq400_fops_wr_trg = {
			.open = open_ok,
			.read = acq400_wr_read_cur,
			.write = acq400_wr_write,
			.release = acq400_wr_release
	};


	switch(PD(file)->minor){
	case ACQ400_MINOR_WR_CUR:
	case ACQ400_MINOR_WR_CUR_TAI:
		dev_dbg(DEVP(ACQ400_DEV(file)), "acq400_wr_open() %d minor %d ", __LINE__, PD(file)->minor);
		file->f_op = &acq400_fops_wr_cur;
		break;
	case ACQ400_MINOR_WR_CUR_TRG0:
	case ACQ400_MINOR_WR_CUR_TRG1:
		dev_dbg(DEVP(ACQ400_DEV(file)), "acq400_wr_open() %d minor %d ", __LINE__, PD(file)->minor);
		file->f_op = &acq400_fops_wr_trg;
		break;
	default:
		dev_dbg(DEVP(ACQ400_DEV(file)), "acq400_wr_open() %d minor %d ", __LINE__, PD(file)->minor);
		file->f_op = &acq400_fops_wr;
		break;
	}
	return file->f_op->open(inode, file);
}
