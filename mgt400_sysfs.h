/*
 * mgt400_sysfs.h
 *
 *  Created on: 18 May 2022
 *      Author: pgm
 */

#ifndef MGT400_SYSFS_H_
#define MGT400_SYSFS_H_

ssize_t mgt400_show_dnum(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK);
/** store field signed decimal */
ssize_t mgt400_store_dnum(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK);

ssize_t mgt400_show_u(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK);
/** store field signed decimal */
ssize_t mgt400_store_u(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK);

ssize_t mgt400_show_bits(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK);

ssize_t mgt400_show_bitN(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK);

ssize_t mgt400_store_bits(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK, unsigned show_warning);

#define SHOW_BITS	mgt400_show_bits
#define STORE_BITS	mgt400_store_bits
/*
#define SHOW_DNUM	mgt400_show_dnum
#define STORE_DNUM	mgt400_store_dnum
*/
#define SHOW_DNUM	mgt400_show_u
#define STORE_DNUM	mgt400_store_u



#endif /* MGT400_SYSFS_H_ */
