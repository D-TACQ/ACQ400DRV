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

static inline u32 tiga_ctrl_set(struct acq400_dev *adev, unsigned bits){
	u32 ctrl = acq400rd32(adev, WR_TIGA_CSR);
	ctrl |= bits;
	acq400wr32(adev, WR_TIGA_CSR, ctrl);
	return ctrl;
}
static inline u32 tiga_ctrl_clr(struct acq400_dev *adev, unsigned bits){
	u32 ctrl = acq400rd32(adev, WR_TIGA_CSR);
	ctrl &= ~bits;
	acq400wr32(adev, WR_TIGA_CSR, ctrl);
	return ctrl;
}

static void wrtt_client_isr_action(struct WrClient* cli, u32 ts)
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
		wrtt_client_isr_action(&sc_dev->wrtt_client0, acq400rd32(adev, WR_CUR_VERNR));
	}
	if (int_sta&WR_CTRL_TT1_STA){
		wrtt_client_isr_action(&sc_dev->wrtt_client1, acq400rd32(adev, WR_CUR_VERNR));
	}
	if (int_sta&WR_CTRL_TS_STA){
		wrtt_client_isr_action(&sc_dev->ts_client, acq400rd32(adev, WR_TAI_STAMP));
	}
	acq400wr32(adev, WR_CTRL, int_sta);
	return IRQ_HANDLED;	/* canned */
}


static irqreturn_t wr_ts_tiga_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_tiga_dev* tiga_dev = container_of(adev, struct acq400_tiga_dev, sc_dev.adev);
	int handled = 0;
	u32 int_sta = acq400rd32(adev, WR_TIGA_CSR);

	dev_dbg(DEVP(adev), "%s %08x", __FUNCTION__, int_sta);

	if (int_sta&WR_TIGA_CSR_TS_ST_SHADOW){
		wrtt_client_isr_action(&tiga_dev->sc_dev.ts_client, acq400rd32(adev, WR_TAI_STAMP));
		handled = 1;
	}
	if (int_sta&(WR_TIGA_CSR_TS_MASK<<WR_TIGA_CSR_TS_ST_SHL)){
		int isite;
		for (isite = 0; isite < 6; ++isite){
			if (int_sta&(1<<(isite+WR_TIGA_CSR_TS_ST_SHL))){
				wrtt_client_isr_action(tiga_dev->ts_clients+isite,
						acq400rd32(adev, WR_TS_S(isite+1)));
			}
		}
		handled = 1;
	}
	if (int_sta&(WR_TIGA_CSR_TS_MASK<<WR_TIGA_CSR_TT_ST_SHL)){
		int isite;
		for (isite = 0; isite < 6; ++isite){
			if (int_sta&(1<<(isite+WR_TIGA_CSR_TT_ST_SHL))){
				wrtt_client_isr_action(tiga_dev->tt_clients+isite,
						acq400rd32(adev, WR_CUR_VERNR));
			}
		}
		handled = 1;
	}
	if (handled){
		acq400wr32(adev, WR_TIGA_CSR, int_sta);
		return IRQ_HANDLED;
	}else{
		return wr_ts_isr(irq, dev_id);
	}
}


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

void init_tiga_scdev(struct acq400_dev* adev)
{
	struct acq400_tiga_dev* tiga_dev = container_of(adev, struct acq400_tiga_dev, sc_dev.adev);
	int isite;
	for (isite = 0; isite < 6; ++isite){
		init_waitqueue_head(&tiga_dev->ts_clients[isite].wc_waitq);
		init_waitqueue_head(&tiga_dev->tt_clients[isite].wc_waitq);
	}
	if (wr_ts_inten){
		tiga_ctrl_set(adev, WR_TIGA_CSR_TS_MASK<<WR_TIGA_CSR_TS_EN_SHL);
	}
	if (wr_tt_inten){
		tiga_ctrl_set(adev, WR_TIGA_CSR_TS_MASK<<WR_TIGA_CSR_TT_EN_SHL);
	}
}
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
	if (IS_ACQ2106_TIGA(adev)){
		init_tiga_scdev(adev);
	}
}

int acq400_wr_init_irq(struct acq400_dev* adev)
{
	int rc;
	int irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET);
	if (irq <= 0){
		return 0;
	}

	rc = devm_request_irq(DEVP(adev), irq, IS_ACQ2106_TIGA(adev)? wr_ts_tiga_isr: wr_ts_isr,
			IRQF_NO_THREAD, "wr_ts", adev);
	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d\n", irq);
		return 0;
	}

	irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET+1);
	if (irq <= 0){
		return 0;
	}

	rc = devm_request_irq(DEVP(adev), irq, wr_pps_isr, IRQF_NO_THREAD, "wr_pps", adev);
	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d\n", irq);
		return 0;
	}

	init_scdev(adev);
	return rc;
}

#define IS_MINOR_TIGA_TS(minor) \
		((minor) >= ACQ420_MINOR_TIGA_TS_1 && (minor) <  ACQ420_MINOR_TIGA_TS_1+WR_TIGA_REGCOUNT)

#define IS_MINOR_TIGA_TT(minor) \
		((minor) >= ACQ420_MINOR_TIGA_TT_1 && (minor) <  ACQ420_MINOR_TIGA_TT_1+WR_TIGA_REGCOUNT)

#define IS_MINOR_TIGA_TTB(minor) \
		((minor) >= ACQ420_MINOR_TIGA_TTB_1 && (minor) <  ACQ420_MINOR_TIGA_TTB_1+WR_TIGA_REGCOUNT)

struct WrClient *_tiga_getWCfromMinor(struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	unsigned minor = pdesc->minor;
	struct acq400_tiga_dev* tiga_dev = container_of(adev, struct acq400_tiga_dev, sc_dev.adev);


	if (IS_MINOR_TIGA_TS(minor)){
		return tiga_dev->ts_clients + (minor-ACQ420_MINOR_TIGA_TS_1);
	}else if (IS_MINOR_TIGA_TT(minor)){
		return tiga_dev->tt_clients + (minor-ACQ420_MINOR_TIGA_TT_1);
	}else if (IS_MINOR_TIGA_TTB(minor)){
		return tiga_dev->tt_clients + (minor-ACQ420_MINOR_TIGA_TTB_1);
	}else{
		dev_err(DEVP(adev), "getWCfromMinor: BAD MINOR %u", minor);
		return 0;
	}
}


struct WrClient *getWCfromMinor(struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	unsigned minor = pdesc->minor;


	if (minor >= ACQ420_MINOR_TIGA_TS_1){
		return _tiga_getWCfromMinor(file);
	}else{
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

#define PD_REG(pdesc) (pdesc->client_private)

ssize_t acq400_wr_read_cur(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	int rc;

	if (count < sizeof(u32)){
		return -EINVAL;
	}else{
		u32 tmp = acq400rd32(adev, PD_REG(pdesc));
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

	if (count < sizeof(u32)){
		return -EINVAL;
	}
	dev_dbg(DEVP(adev), "acq400_wr_write() ");

	rc = copy_from_user(&tmp, buf, sizeof(u32));

	if (rc){
		return -1;
	}else{
		acq400wr32(adev, PD_REG(pdesc), tmp);
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
	struct acq400_path_descriptor* pdesc = PD(file);

	switch(pdesc->minor){
	case ACQ400_MINOR_WR_CUR:
		PD_REG(pdesc) = WR_CUR_VERNR;
		file->f_op = &acq400_fops_wr_cur;
		break;
	case ACQ400_MINOR_WR_CUR_TAI:
		PD_REG(pdesc) =  WR_TAI_CUR_L;
		file->f_op = &acq400_fops_wr_cur;
		break;
	case ACQ400_MINOR_ADC_SAMPLE_COUNT:
		PD_REG(pdesc) = ADC_SAMPLE_CTR;
		file->f_op = &acq400_fops_wr_cur;
		break;
	case ACQ400_MINOR_WR_CUR_TRG0:
		PD_REG(pdesc) =  WR_TAI_TRG0;
		file->f_op = &acq400_fops_wr_trg;
		break;
	case ACQ400_MINOR_WR_CUR_TRG1:
		PD_REG(pdesc) =  WR_TAI_TRG1;
		file->f_op = &acq400_fops_wr_trg;
		break;
	default:
		dev_dbg(DEVP(ACQ400_DEV(file)), "acq400_wr_open() %d minor %d ", __LINE__, PD(file)->minor);
		file->f_op = &acq400_fops_wr;
		break;
	}
	return file->f_op->open(inode, file);
}



int _acq400_tiga_open(struct inode *inode, struct file *file)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	unsigned minor = pdesc->minor;
	struct WrClient *wc = _tiga_getWCfromMinor(file);

	dev_dbg(DEVP(adev), "_acq400_tiga_open() %d minor %d flags %x", __LINE__, PD(file)->minor, file->f_flags);

	if (wc == 0){
		return -ENODEV;
	}else if (!IS_MINOR_TIGA_TS(minor) && (file->f_flags & O_WRONLY)) {		// only ts is writeable
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



int acq400_tiga_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_wr = {
			.open = _acq400_tiga_open,
			.read = acq400_wr_read,
			.release = acq400_wr_release_exclusive
	};
	static struct file_operations acq400_fops_wr_trg = {
			.open = open_ok,
			.read = acq400_wr_read_cur,
			.write = acq400_wr_write,
			.release = acq400_wr_release
	};
	static struct file_operations acq400_fops_wr_trg_block = {
			.open = _acq400_tiga_open,
			.read = acq400_wr_read,
			.write = acq400_wr_write,
			.release = acq400_wr_release_exclusive
	};
	struct acq400_path_descriptor* pdesc = PD(file);
	int minor = pdesc->minor;

	if (minor >= ACQ420_MINOR_TIGA_TS_1 && minor <  ACQ420_MINOR_TIGA_TS_1+WR_TIGA_REGCOUNT){
		PD_REG(pdesc) = WR_TS_S(minor-ACQ420_MINOR_TIGA_TS_1+1);
		file->f_op = &acq400_fops_wr;
	}else if (minor >= ACQ420_MINOR_TIGA_TT_1 && minor <  ACQ420_MINOR_TIGA_TT_1+WR_TIGA_REGCOUNT){
		PD_REG(pdesc) = WR_TT_S(minor-ACQ420_MINOR_TIGA_TT_1+1);
		file->f_op = &acq400_fops_wr_trg;
	}else if (minor >= ACQ420_MINOR_TIGA_TTB_1 && minor <  ACQ420_MINOR_TIGA_TTB_1+WR_TIGA_REGCOUNT){
		PD_REG(pdesc) = WR_TT_S(minor-ACQ420_MINOR_TIGA_TT_1+1);
		file->f_op = &acq400_fops_wr_trg_block;
	}else{
		return -ENODEV;
	}
	dev_dbg(PDEV(file), "%s minor %d REG 0x%04x", __FUNCTION__, minor, PD_REG(pdesc));

	return file->f_op->open(inode, file);
}

