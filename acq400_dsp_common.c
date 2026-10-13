/*
 * acq400_dsp_common.c
 *
 *  Created on: 18 Oct 2024
 *      Author: pgm
 */

#include <linux/cdev.h>

#include "acq400_dsp.h"
#include "acq400.h"

int acq400dsp_devicetree_init(struct platform_device *pdev,
			      struct device_node *of_node)
{
	if (of_node == 0) {
		return -1;
	} else {
		int ii;
		unsigned site = -1;
		const char *name = 0;
		if (of_property_read_string(of_node, "name", &name) < 0) {
			dev_warn(&pdev->dev,
				 "warning: name NOT specified in DT");
		}
		if (of_property_read_u32(of_node, "site", &site) < 0) {
			dev_warn(&pdev->dev,
				 "warning: site NOT specified in DT");
		}
		for (ii = 0; ii < pdev->num_resources; ++ii) {
			if (name) {
				pdev->resource[ii].name = name;
			}
		}
		if (site != -1) {
			pdev->id = site;
		}
		return 0;
	}
}
