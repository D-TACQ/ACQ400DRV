/* ------------------------------------------------------------------------- */
/* acq400_set.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 24 Sep 2016  			/ User: pgm
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

#include "acq400.h"

extern int good_sites[];
extern int good_sites_count;


int acq400_get_site(struct acq400_dev *adev, char* s)
{
	int ii;
	int site;

	if (sscanf(s, "%d", &site) != 1){
		return -1;
	}
	for(ii = 0; ii < good_sites_count; ++ii){
		if (site == good_sites[ii]){
			if (IS_ACQ1001SC(adev) && site == 4){
				/* @@todo site 3/4 frig */
				return 3;
			}else{
				return site;
			}
		}
	}

	return -1;
}

int acq400_add_set(struct acq400_dev* set[], struct acq400_dev *adev, int site)
{
	int ia = 0;
	struct acq400_dev *slave = acq400_lookupSite(site);
	for (ia = 0; ia < MAXDEVICES; ++ia){
		dev_dbg(DEVP(adev), "add_set [%d]=%p site:%d %p",
				ia, set[ia], site, slave);
		if (set[ia] == slave){
			return 0;
		}else if (set[ia] == 0){
			set[ia] = slave;
			return 0;
		}
	}
	dev_err(DEVP(adev), "ERROR: add_set() failed");
	return 1;
}

void acq400_clear_set(struct acq400_dev* set[])
{
	int ia = 0;
	for (ia = 0; ia < MAXDEVICES; ++ia){
		set[ia] = 0;
	}
}

int acq400_read_set(struct acq400_dev* set[],
		struct acq400_dev *adev, char *buf, int maxbuf)
{
	int ia = 0;
	int cursor = 0;

	for (ia = 0; ia < MAXDEVICES; ++ia){
		if (set[ia] != 0){
			int site = set[ia]->of_prams.site;
			if (cursor){
				cursor += sprintf(buf+cursor, ",%d", site);
			}else{
				cursor += sprintf(buf+cursor, "%d", site);
			}

			if (cursor > maxbuf - 3){
				break;
			}
		}
	}
	cursor += sprintf(buf+cursor, "\n");
	return cursor;
}

unsigned _acq400_convert_aggregator_set_to_register_mask(struct acq400_dev* set[])
{
	u32 mask = 0;
	int ia = 0;
	for (ia = 0; ia < MAXDEVICES; ++ia){
		if (set[ia] != 0){
			mask |= 1<<ia;
		}
	}
	return mask << AGGREGATOR_MSHIFT;
}
unsigned acq400_convert_aggregator_set_to_register_mask(struct acq400_dev *adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	return _acq400_convert_aggregator_set_to_register_mask(sc_dev->aggregator_set);
}
int acq400_add_aggregator_set(struct acq400_dev *adev, int site)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	return acq400_add_set(sc_dev->aggregator_set, adev, site);
}

int acq400_read_aggregator_set(struct acq400_dev *adev, char *buf, int maxbuf)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	return acq400_read_set(sc_dev->aggregator_set, adev, buf, maxbuf);
}


void acq400_clear_aggregator_set(struct acq400_dev *adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	acq400_clear_set(sc_dev->aggregator_set);
}
int acq400_add_distributor_set(struct acq400_dev *adev, int site)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	return acq400_add_set(sc_dev->distributor_set, adev, site);
}
void acq400_clear_distributor_set(struct acq400_dev *adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	acq400_clear_set(sc_dev->distributor_set);
}

void acq400_visit_set(struct acq400_dev *set[], void (*action)(struct acq400_dev *adev))
{
	int site;

	for (site = MAXDEVICES; --site >= 0; ){
		struct acq400_dev *adev = set[site];
		if (adev){
			dev_dbg(DEVP(adev), "visit_set [%d] %p of_site:%d", site, adev, adev->of_prams.site);
			action(adev);
		}
	}
}

void acq400_visit_set_arg(struct acq400_dev *set[], void (*action)(struct acq400_dev *adev, void* arg), void*arg)
{
	struct acq400_dev *adev;
	int mod;

	dev_dbg(0, "acq400_visit_set_arg arg %p", arg);

	/* first zero entry ends set */
	for (mod = 0; (adev = set[mod]) && mod < MAXDEVICES; ++mod){
		dev_dbg(DEVP(adev), "acq400_visit_set_arg [%d] %p of_site:%d", mod, adev, adev->of_prams.site);
		action(adev, arg);
	}
}

