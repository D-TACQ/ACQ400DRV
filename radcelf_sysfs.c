/* ------------------------------------------------------------------------- */
/* radcelf_sysfs.c ACQ420_FMC						     */
/* ------------------------------------------------------------------------- */
/* radcelf_sysfs.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 4 Jun 2017  			/ User: pgm
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

#include "acq400.h"
#include "acq400_sysfs.h"


int dds_strobe_msec = 1;
module_param(dds_strobe_msec, int, 0644);
MODULE_PARM_DESC(dds_strobe_msec, "STROBE HI TIME in msec");


MAKE_BITS(ddsA_gps_sync_chirp,  RAD_DDS_A, MAKE_BITS_FROM_MASK, DDS_GPS_SYNC_CHIRP);
MAKE_BITS(ddsA_gps_engage_hold, RAD_DDS_A, MAKE_BITS_FROM_MASK, DDS_GPS_ENGAGE_HOLD);
MAKE_BITS(ddsA_gps_arm_pps,     RAD_DDS_A, MAKE_BITS_FROM_MASK, DDS_GPS_ARM_PPS);

MAKE_BITS(ddsB_gps_sync_chirp,  RAD_DDS_B, MAKE_BITS_FROM_MASK, DDS_GPS_SYNC_CHIRP);
MAKE_BITS(ddsB_gps_engage_hold, RAD_DDS_B, MAKE_BITS_FROM_MASK, DDS_GPS_ENGAGE_HOLD);
MAKE_BITS(ddsB_gps_arm_pps,     RAD_DDS_B, MAKE_BITS_FROM_MASK, DDS_GPS_ARM_PPS);

MAKE_BITS(clkd_hard_reset, RAD_CTL, MAKE_BITS_FROM_MASK, RAD_CTL_CLKD_RESET);
MAKE_BITS(ddsX_hard_reset, RAD_CTL, MAKE_BITS_FROM_MASK, RAD_CTL_DDS_RESET);
MAKE_BITS(ddsA_upd_clk_fpga, RAD_DDS_A, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK_FPGA);
MAKE_BITS(ddsB_upd_clk_fpga, RAD_DDS_B, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK_FPGA);
MAKE_BITS(ddsC_upd_clk_fpga, RAD_DDS_C, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK_FPGA);

MAKE_BITS(ddsA_upd_clk, RAD_DDS_A, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK);
MAKE_BITS(ddsB_upd_clk, RAD_DDS_B, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK);
MAKE_BITS(ddsC_upd_clk, RAD_DDS_C, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK);
MAKE_BITS(ddsAB_upd_clk, RAD_DDS_AB, MAKE_BITS_FROM_MASK, RAD_DDS_UPD_CLK);


MAKE_BITS(ddsA_clk_OEn, RAD_DDS_A, MAKE_BITS_FROM_MASK, RAD_DDS_CLK_OEn);
MAKE_BITS(ddsB_clk_OEn, RAD_DDS_B, MAKE_BITS_FROM_MASK, RAD_DDS_CLK_OEn);
MAKE_BITS(ddsC_clk_OEn, RAD_DDS_C, MAKE_BITS_FROM_MASK, RAD_DDS_CLK_OEn);

MAKE_BITS(ddsA_OSK, RAD_DDS_A, MAKE_BITS_FROM_MASK, RAD_DDS_OSK);
MAKE_BITS(ddsB_OSK, RAD_DDS_B, MAKE_BITS_FROM_MASK, RAD_DDS_OSK);
MAKE_BITS(ddsC_OSK, RAD_DDS_C, MAKE_BITS_FROM_MASK, RAD_DDS_OSK);

MAKE_BITS(ddsA_BPSK, RAD_DDS_A, MAKE_BITS_FROM_MASK, RAD_DDS_BPSK);
MAKE_BITS(ddsB_BPSK, RAD_DDS_B, MAKE_BITS_FROM_MASK, RAD_DDS_BPSK);
MAKE_BITS(ddsC_BPSK, RAD_DDS_C, MAKE_BITS_FROM_MASK, RAD_DDS_BPSK);


void _acq400_spi_strobe(struct acq400_dev *adev, const int REG)
{
	u32 ctrl = acq400rd32(adev, REG);

	dev_dbg(DEVP(adev), "_acq400_spi_strobe %x", REG);

	ctrl &= ~RAD_DDS_UPD_CLK;
	acq400wr32(adev, REG, ctrl);
	acq400wr32(adev, REG, ctrl|RAD_DDS_UPD_CLK);
	msleep(dds_strobe_msec);
	acq400wr32(adev, REG, ctrl);
}
static ssize_t store_strobe(
	struct device * dev,
	struct device_attribute *attr,
	const char * buf,
	size_t count,
	const int REG)
{
	struct acq400_dev *adev = acq400_devices[dev->id];
	unsigned strobe;

	if (sscanf(buf, "%u", &strobe) == 1){
		if (strobe){
			_acq400_spi_strobe(adev, REG);
		}
		return count;
	}else{
		return -1;
	}
}

#define MAKE_DDS_STROBE(DDS)						\
static ssize_t store_strobe##DDS(					\
	struct device * dev,						\
	struct device_attribute *attr,					\
	const char * buf,						\
	size_t count)							\
{									\
	return store_strobe(dev, attr, buf, count, RAD_DDS_##DDS);	\
}									\
static DEVICE_ATTR(strobe##DDS, S_IWUSR, 0, store_strobe##DDS)

MAKE_DDS_STROBE(A);
MAKE_DDS_STROBE(B);
MAKE_DDS_STROBE(C);
MAKE_DDS_STROBE(AB);

void acq400_spi_strobe(void *clidata, int cs, int mode)
{
	struct acq400_dev* adev = (struct acq400_dev*)clidata;
	switch (mode){
	case SPI_STROBE_GROUP:
		if (cs==0||cs==1){
			_acq400_spi_strobe(adev, RAD_DDS_AB);
			break;
		}
		/* fall thru */
	case SPI_STROBE_SELF:
		switch(cs){
		case 0:
			_acq400_spi_strobe(adev, RAD_DDS_A);
			break;
		case 1:
			_acq400_spi_strobe(adev, RAD_DDS_B);
			break;
		case 2:
			_acq400_spi_strobe(adev, RAD_DDS_C);
			break;
		default:
			;
		}
	default:
		;
	}
}

EXPORT_SYMBOL_GPL(acq400_spi_strobe);

SCOUNT_KNOB_FIELD(clk_pps_latch, RAD_CLK_PPS_LATCH, 0x0fffffff);

const struct attribute *sysfs_radcelf_attrs[] = {
	&dev_attr_ddsA_gps_sync_chirp.attr,
	&dev_attr_ddsA_gps_engage_hold.attr,
	&dev_attr_ddsA_gps_arm_pps.attr,

	&dev_attr_ddsB_gps_sync_chirp.attr,
	&dev_attr_ddsB_gps_engage_hold.attr,
	&dev_attr_ddsB_gps_arm_pps.attr,

	&dev_attr_clkd_hard_reset.attr,
	&dev_attr_ddsX_hard_reset.attr,
	&dev_attr_ddsA_upd_clk_fpga.attr,
	&dev_attr_ddsB_upd_clk_fpga.attr,
	&dev_attr_ddsC_upd_clk_fpga.attr,

	&dev_attr_ddsA_upd_clk.attr,
	&dev_attr_ddsB_upd_clk.attr,
	&dev_attr_ddsC_upd_clk.attr,
	&dev_attr_ddsAB_upd_clk.attr,

	&dev_attr_ddsA_clk_OEn.attr,
	&dev_attr_ddsB_clk_OEn.attr,
	&dev_attr_ddsC_clk_OEn.attr,

	&dev_attr_ddsA_OSK.attr,
	&dev_attr_ddsB_OSK.attr,
	&dev_attr_ddsC_OSK.attr,

	&dev_attr_ddsA_BPSK.attr,
	&dev_attr_ddsB_BPSK.attr,
	&dev_attr_ddsC_BPSK.attr,

	&dev_attr_strobeA.attr,
	&dev_attr_strobeB.attr,
	&dev_attr_strobeC.attr,
	&dev_attr_strobeAB.attr,

	&dev_attr_scount_clk_pps_latch.attr,
	NULL
};

extern const struct attribute *sysfs_sc_remaining_clocks[];

void sysfs_radcelf_create_files(struct device *dev){
	struct acq400_dev *adev0 = acq400_devices[0];

	printk("sysfs_radcelf_create_files() RAD_DDS_UPD_CLK_FPGA %08x\n", RAD_DDS_UPD_CLK_FPGA);
	printk("sysfs_radcelf_create_files() RAD_DDS_UPD_CLK %08x\n", RAD_DDS_UPD_CLK);


	if (sysfs_create_files(&dev->kobj, sysfs_radcelf_attrs)){
		dev_err(dev, "failed to create sysfs");
	}
	if (sysfs_create_files(&adev0->pdev->dev.kobj, sysfs_sc_remaining_clocks)){
		dev_err(&adev0->pdev->dev, "failed to create sysfs");
	}
}
