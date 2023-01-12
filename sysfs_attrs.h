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
extern const struct attribute *ao420_attrs[];

#define ao420_half_436_attrs &ao420_attrs[8]

extern const struct attribute **get_ao424_attrs(void);
extern const struct attribute *acq436_upper_half_attrs_master[];
#define acq436_upper_half_attrs &acq436_upper_half_attrs_master[1]

extern const struct attribute *dio432_attrs[];
extern const struct attribute *dio422_attrs[];

extern const struct attribute *dio_attrs[];

extern const struct attribute *pig_celf_attrs[];
extern const struct attribute *acq400t_attrs[];

extern struct device_attribute dev_attr_bank_mask;


extern const struct attribute *acq423_attrs[];

extern void sysfs_radcelf_create_files(struct device *dev);

extern const struct attribute *acq2106_wr_attrs[];
extern const struct attribute *acq2106_tiga_attrs[];


extern const struct attribute *dio482_pg_attrs[];
extern const struct attribute *dio482_pg32_attrs[];

extern const struct attribute *acq494_attrs[];

extern const struct attribute *dio482_attrs[];
extern const struct attribute *pwm2_attrs[];

extern const struct attribute *dio482ppw_attrs[];

#endif /* SYSFS_ATTRS_H_ */
