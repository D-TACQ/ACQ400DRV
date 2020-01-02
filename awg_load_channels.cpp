/*
 * awg_load_channels.cpp
 *
 *  Created on: 2 Jan 2020
 *      Author: pgm
 */
/* awg_load_channels.cpp  D-TACQ ACQ400 mux and load AWG channels from channel files in /dev/shm/
 * Project: ACQ420_FMC
 * Created: 2 Jan 2020  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2020 Peter Milne, D-TACQ Solutions Ltd                    *
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
\* ------------------------------------------------------------------------- */



#include <stdio.h>


#include "popt.h"


namespace G{
	int nchan = 8;
	int data32 = 1;
	int port = 54202;
};

struct poptOption opt_table[] = {
{ "nchan", 'n', POPT_ARG_INT, &G::nchan, 0, "number of AWG channels" },
{ "data32", 'd', POPT_ARG_INT, &G::data32, 0, "set 32 bit data"},
{ "port", 'p', POPT_ARG_INT, &G::port, 0, "port to load mux data at" },
POPT_AUTOHELP
POPT_TABLEEND
};

void ui(int argc, const char** argv)
{

}

void load(void) {

}

int main(int argc, const char** argv)
{
	ui(argc, argv);
	load();
	return 0;
}
