/*
 * split2.h
 *
 *  Created on: 17 Jan 2020
 *      Author: pgm
 *  ref and thanks:
 *  http://www.martinbroadhurst.com/how-to-split-a-string-in-c.html
 */

#ifndef SPLIT2_H_
#define SPLIT2_H_

#include <stdlib.h>

#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <vector>

class vectorIntFromString: public std::vector<int> {
public:
	void push_back(std::string ss, int base=0) {
		std::vector<int>::push_back(strtol(ss.c_str(), 0, base));
	}
};
typedef class vectorIntFromString VIS;

class vectorDoubleFromString: public std::vector<double> {
public:
	void push_back(std::string ss) {
		std::vector<double>::push_back(atof(ss.c_str()));
	}
};
typedef class vectorDoubleFromString VDS;

template <class Container>
void split2(const std::string& str, Container& cont, char delim = ' ')
{
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        cont.push_back(token);
    }
}

#endif /* SPLIT2_H_ */
