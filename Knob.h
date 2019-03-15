/*
 * Knob.h
 *
 *  Created on: 23 Jan 2019
 *      Author: pgm
 */

#ifndef KNOB_H_
#define KNOB_H_

class Knob {
	char *kpath;
public:
	Knob(const char* abs_path);
	Knob(int site, const char* knob);
	virtual ~Knob();

	int get(unsigned *value);
	int get(char *value);

	int set(int value);
	int set(const char* value);
	int setX(unsigned value);
};



#endif /* KNOB_H_ */
