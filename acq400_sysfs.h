/* ------------------------------------------------------------------------- */
/* acq400_sysfs.h  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 16 Nov 2016  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2016 Peter Milne, D-TACQ Solutions Ltd         *
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

#define DEVICE_CREATE_FILE(dev, attr) 							\
	do {										\
		if (device_create_file(dev, attr)){ 					\
			dev_warn(dev, "%s:%d device_create_file", __FILE__, __LINE__); 	\
		} 									\
	} while (0)


ssize_t acq400_show_triplet(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned T1,
	unsigned T2,
	unsigned T3);

#define SHLMASK(x, f) (((x)<<getSHL(f))&(f))

ssize_t acq400_store_triplet(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned T1,
		unsigned T2,
		unsigned T3);

#define MAKE_TRIPLET(NAME, REG, T1, T2, T3)				\
static ssize_t show_bits##NAME(						\
	struct device *d,						\
	struct device_attribute *a,					\
	char *b)							\
{									\
	return acq400_show_triplet(d, a, b, REG, T1, T2, T3);		\
}									\
static ssize_t store_bits##NAME(					\
	struct device * d,						\
	struct device_attribute *a,					\
	const char * b,							\
	size_t c)							\
{									\
	return acq400_store_triplet(d, a, b, c, REG, T1, T2, T3);   	\
}									\
static DEVICE_ATTR(NAME, S_IRUGO|S_IWUSR, show_bits##NAME, store_bits##NAME)
#define MASKSHR(x, f) (((x)&(f)) >> getSHL(f))

#define MAKE_BITS_FROM_MASK	0xdeadbeef


ssize_t acq400_show_bits(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK);

ssize_t acq400_show_bitN(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK);

ssize_t acq400_store_bits(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK, unsigned show_warning);

ssize_t acq400_store_bitN(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK, unsigned show_warning);

#define MAKE_BITS_RO(NAME, REG, SHL, MASK)				\
static ssize_t show_bits##NAME(						\
	struct device *d,						\
	struct device_attribute *a,					\
	char *b)							\
{									\
	unsigned shl = getSHL(MASK);					\
	if (shl){							\
		return acq400_show_bits(d, a, b, REG, shl, (MASK)>>shl);\
	}else{								\
		return acq400_show_bits(d, a, b, REG, SHL, MASK);	\
	}								\
}									\
static DEVICE_ATTR(NAME, S_IRUGO, show_bits##NAME, 0)

/* active low. Valid for single bit only */
#define MAKE_BIT_RON(NAME, REG, SHL, MASK)				\
static ssize_t show_bits##NAME(						\
	struct device *d,						\
	struct device_attribute *a,					\
	char *b)							\
{									\
	unsigned shl = getSHL(MASK);					\
	if (shl){							\
		return acq400_show_bitN(d, a, b, REG, shl, (MASK)>>shl);\
	}else{								\
		return acq400_show_bitN(d, a, b, REG, SHL, MASK);	\
	}								\
}									\
static DEVICE_ATTR(NAME, S_IRUGO, show_bits##NAME, 0)

/* active low. Valid for single bit only */
#define MAKE_BIT_N(NAME, REG, SHL, MASK, WD)				\
static ssize_t store_bitN##NAME(					\
	struct device * d,						\
	struct device_attribute *a,					\
	const char * b,							\
	size_t c)							\
{									\
	unsigned shl = getSHL(MASK);					\
	if (shl){							\
		return acq400_store_bitN(d, a, b, c, REG, shl, (MASK)>>shl, WD);\
	}else{								\
		return acq400_store_bitN(d, a, b, c, REG, SHL, MASK, WD);\
	}								\
}									\
static ssize_t show_bitN##NAME(						\
	struct device *d,						\
	struct device_attribute *a,					\
	char *b)							\
{									\
	unsigned shl = getSHL(MASK);					\
	if (shl){							\
		return acq400_show_bitN(d, a, b, REG, shl, (MASK)>>shl);\
	}else{								\
		return acq400_show_bitN(d, a, b, REG, SHL, MASK);	\
	}								\
}									\
static DEVICE_ATTR(NAME, S_IRUGO|S_IWUSR, show_bitN##NAME, store_bitN##NAME)


#define _MAKE_BITS(NAME, REG, SHL, MASK, WD)				\
static ssize_t show_bits##NAME(						\
	struct device *d,						\
	struct device_attribute *a,					\
	char *b)							\
{									\
	unsigned shl = getSHL(MASK);					\
	if (shl){							\
		return acq400_show_bits(d, a, b, REG, shl, (MASK)>>shl);\
	}else{								\
		return acq400_show_bits(d, a, b, REG, SHL, MASK);	\
	}								\
}									\
static ssize_t store_bits##NAME(					\
	struct device * d,						\
	struct device_attribute *a,					\
	const char * b,							\
	size_t c)							\
{									\
	unsigned shl = getSHL(MASK);					\
	if (shl){							\
		return acq400_store_bits(d, a, b, c, REG, shl, (MASK)>>shl, WD);\
	}else{								\
		return acq400_store_bits(d, a, b, c, REG, SHL, MASK, WD);\
	}								\
}									\
static DEVICE_ATTR(NAME, S_IRUGO|S_IWUSR, show_bits##NAME, store_bits##NAME)


#define MAKE_BITS(NAME, REG, SHL, MASK)	_MAKE_BITS(NAME, REG, SHL, MASK, 0)
#define DEPRECATED_MAKE_BITS(NAME, REG, SHL, MASK)	_MAKE_BITS(NAME, REG, SHL, MASK, 1)

