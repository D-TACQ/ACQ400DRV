/* ------------------------------------------------------------------------- */
/* zynq_peripheral_spi_shim.c ACQ420_FMC						     */
/*
 * zynq_peripheral_spi_shim.c
 *
 *  Created on: 13 Nov 2015
 *      Author: pgm
 */

/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2015 Peter Milne, D-TACQ Solutions Ltd                    *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

int spi_goslow_usec_kludge;
module_param(spi_goslow_usec_kludge, int, 0644);


extern void acq400_set_peripheral_SPI_CS(unsigned csword);
extern void acq400_set_peripheral_SPI_chipboard(unsigned csword);
extern int (*zynq_spi_cs_hook)(int ch, int cs, int is_high);

extern int zynq_spi_goslow_usec_kludge;

int zynq_spi_cs(int ch, int cs, int is_high)
{
	acq400_set_peripheral_SPI_CS(is_high? 0x7: cs);
	return 0;
}

int zynq_spi_cs_cb(int ch, int cs, int is_high)
{
	acq400_set_peripheral_SPI_chipboard(is_high? 0x70|(cs&0x0f): cs);
	return 0;
}

void acq480_hook_spi(void) {
	printk("acq480_hook_spi() ZYNQ SPI workaround\n");
	zynq_spi_cs_hook = zynq_spi_cs;
}

void acq480_hook_spi_cb(void) {
	printk("acq480_hook_spi_cb() ZYNQ SPI workaround\n");
	zynq_spi_cs_hook = zynq_spi_cs_cb;
	if (spi_goslow_usec_kludge){
		zynq_spi_goslow_usec_kludge = spi_goslow_usec_kludge;
		printk("acq480_hook_spi_cb() zynq_spi_goslow_usec_kludge set %d\n",
						zynq_spi_goslow_usec_kludge);
	}
}

