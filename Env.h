/*
 * Env.h
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */

#ifndef ENV_H_
#define ENV_H_


#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <map>

class Env {
	std::map<std::string,std::string> _env;
public:

	Env(const char* fname);
	std::string& operator() (std::string key);
};




#endif /* ENV_H_ */
