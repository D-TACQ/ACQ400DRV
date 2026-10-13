/* ------------------------------------------------------------------------- */
/** @file lk-shim.h hides API changes for a range of Linux Kernels.        */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2010 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * lk-shim.h
 *
 *  Created on: Mar 13, 2013
 *      Author: pgm
 */

#ifndef LK_SHIM_H_
#define LK_SHIM_H_

#include <linux/version.h>

#define CLASS_DEVICE device
#define CLASS_DEVICE_CREATE device_create
#define CLASS_DEVICE_ATTR DEVICE_ATTR
#define CLASS_DEVICE_CREATE_FILE DEVICE_CREATE_FILE

#endif /* LK_SHIM_H_ */
