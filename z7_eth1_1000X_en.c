/* ------------------------------------------------------------------------- *
 * z7_eth1_1000X_en.c
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2019 Peter Milne, D-TACQ Solutions Ltd
 *                      <peter dot milne at D hyphen TACQ dot com>
 *                         www.d-tacq.com
 *  Created on: 21 Jun 2019
 *      Author: pgm
 *  Make slcr settings to route eth1 clocks from emio
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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/regmap.h>

#define REVID "0.1"

#define GEM1_RCLK_CTRL 0x13c
#define GEM1_CLK_CTRL 0x144

static int __init eth1_1000X_en_init(void)
{
	struct regmap *syscon;
	printk("D-TACQ z7_eth1_1000X_en %s\n", REVID);

	syscon = syscon_regmap_lookup_by_compatible("xlnx,zynq-slcr");

	if (IS_ERR(syscon)) {
		printk("ERROR: eth1_1000X_en_init unable to get syscon\n");
		return PTR_ERR(syscon);
	}

	regmap_write(syscon, GEM1_RCLK_CTRL, 0x11);
	regmap_write(syscon, GEM1_CLK_CTRL, 0x41);

	return 0;
}

static void __exit eth1_1000X_en_exit(void)
{
}

module_init(eth1_1000X_en_init);
module_exit(eth1_1000X_en_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("D-TACQ set eth1 1000X clock routing");
MODULE_AUTHOR("D-TACQ Solutions.");
MODULE_VERSION(REVID);
