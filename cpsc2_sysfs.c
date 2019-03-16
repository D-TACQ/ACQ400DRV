/*
 * cpsc2_sysfs.c
 *
 *  Created on: 16 Mar 2019
 *      Author: pgm
 */

#include <linux/ctype.h>
#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */
#include "lk-shim.h"
#include <linux/device.h>
#include <linux/user.h>

#include "cpsc2.h"
#include "acq400.h"
#include "acq400_sysfs.h"

#include "sysfs_attrs.h"

const struct attribute *cpsc2_dac_attrs[] = {
	NULL
};
const struct attribute *cpsc2_com_attrs[] = {
	NULL
};

