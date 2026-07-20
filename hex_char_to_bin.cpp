/*
 * hex_char_to_bin.cpp
 *
 *  Created on: 9 Feb 2024
 *      Author: pgm
 */

#include <string>
#include <ctype.h>

static std::string hexCharToBin( char c ) {
    switch( toupper(c) ) {
        case '0'    : return "0000";
        case '1'    : return "0001";
        case '2'    : return "0010";
        case '3'    : return "0011";
        case '4'    : return "0100";
        case '5'    : return "0101";
        case '6'    : return "0110";
        case '7'    : return "0111";
        case '8'    : return "1000";
        case '9'    : return "1001";
        case 'A'    : return "1010";
        case 'B'    : return "1011";
        case 'C'    : return "1100";
        case 'D'    : return "1101";
        case 'E'    : return "1110";
        case 'F'    : return "1111";
    }
    return "ERROR";
}


std::string hexStrToBin( const std::string & hs ) {
    std::string bin;
    for (auto it = hs.rbegin(); it != hs.rend(); ++it) {
        bin = hexCharToBin( *it ) + bin;
        //printf("%c %s %s\n", *it, hexCharToBin(*it).c_str(), bin.c_str());
    }
    return bin;
}



