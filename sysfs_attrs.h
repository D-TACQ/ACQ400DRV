/*
 * sysfs_attrs.h
 *
 *  Created on: 16 Feb 2019
 *      Author: pgm
 */

#ifndef SYSFS_ATTRS_H_
#define SYSFS_ATTRS_H_

extern const struct attribute *sysfs_v2f_attrs[];
extern const struct attribute *atd_attrs[];
extern const struct attribute *dtd_attrs[];
extern const struct attribute *sysfs_diobiscuit_attrs[];
extern const struct attribute *sysfs_qen_attrs[];
extern const struct attribute **acq480_attrs;
extern const struct attribute *acq480_ffir_attrs[];
extern const struct attribute *sysfs_acq1014_attrs[];
extern const struct attribute *bolo8_attrs[];

extern const struct attribute *playloop_attrs[];
extern const struct attribute* dacspi_attrs[];
extern const struct attribute *ao428_attrs[];
extern const struct attribute *ao42S_attrs[];
extern const struct attribute *ao420_attrs[];

extern const struct attribute *cpsc2_dac_attrs[];
extern const struct attribute *cpsc2_com_attrs[];

#define ao420_half_436_attrs &ao420_attrs[8]

extern const struct attribute *ao424_attrs[];
extern const struct attribute *acq436_upper_half_attrs_master[];
#define acq436_upper_half_attrs &acq436_upper_half_attrs_master[1]

extern const struct attribute *dio432_attrs[];

extern const struct attribute *pig_celf_attrs[];
extern const struct attribute *acq400t_attrs[];

extern struct device_attribute dev_attr_bank_mask;


extern const struct attribute *acq423_attrs[];

extern void sysfs_radcelf_create_files(struct device *dev);

#endif /* SYSFS_ATTRS_H_ */
