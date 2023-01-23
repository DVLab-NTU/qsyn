/****************************************************************************
  FileName     [ util.h ]
  PackageName  [ util ]
  Synopsis     [ Define the prototypes of the exported utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef UTIL_H
#define UTIL_H

#include <concepts>
#include <iosfwd>
#include <string>
#include <vector>

#include "myUsage.h"
#include "rnGen.h"

// In myString.cpp
extern bool stripQuotes(const std::string& input, std::string& output);
extern std::string stripWhitespaces(const std::string& str);
extern int myStrNCmp(const std::string& s1, const std::string& s2, unsigned n);
extern size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos = 0,
                          const char del = ' ');
extern size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos,
                          const std::string& del);
extern size_t myStrGetTok2(const std::string& str, std::string& tok, size_t pos = 0,
                           const std::string& del = " \t\n\v\f\r");
extern bool myStr2Int(const std::string& str, int& num);
extern bool myStr2Uns(const std::string& str, unsigned& num);

template <class T>
requires std::floating_point<T>
extern bool myStr2FloatType(const std::string& str, T& f);

extern bool myStr2Float(const std::string& str, float& f);
extern bool myStr2Double(const std::string& str, double& f);
extern bool myStr2LongDouble(const std::string& str, long double& f);

// In myGetChar.cpp
char myGetChar(std::istream& istr);
char myGetChar();

// In util.cpp
int listDir(std::vector<std::string>&, const std::string&, const std::string&);
size_t intPow(size_t base, size_t n);

template <typename T>
bool contains(const std::vector<T>& vec, const T& t) {
    return (std::find(vec.begin(), vec.end(), t) != vec.end());
}

template <typename T>
size_t findIndex(const std::vector<T>& vec, const T& t) {
    return std::find(vec.begin(), vec.end(), t) - vec.begin();
}

#endif  // UTIL_H
