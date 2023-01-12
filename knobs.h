/* ------------------------------------------------------------------------- */
/* knobs.h  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 1 Mar 2016  			/ User: pgm
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
\* ------------------------------------------------------------------------- */

#ifndef KNOBS_H_
#define KNOBS_H_


int getKnob(int idev, const char* knob, unsigned* value, const char* fmt = "%u");
int getKnob(int idev, const char* knob, char* value);
int setKnob(int idev, const char* knob, const char* value);
int setKnob(int idev, const char* knob, int value);

int getEtcKnob(int idev, const char* knob, unsigned* value, const char* fmt = "%u");

#endif /* KNOBS_H_ */
