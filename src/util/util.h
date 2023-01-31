/****************************************************************************
  FileName     [ util.h ]
  PackageName  [ util ]
  Synopsis     [ Define the prototypes of the exported utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef QSYN_UTIL_H
#define QSYN_UTIL_H

#include <concepts>
#include <iosfwd>
#include <string>
#include <vector>

#include "myUsage.h"
#include "rnGen.h"

// In myString.cpp
bool stripQuotes(const std::string& input, std::string& output);
std::string stripWhitespaces(const std::string& str);
int myStrNCmp(const std::string& s1, const std::string& s2, unsigned n);
size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos = 0, const std::string& del = " \t\n\v\f\r");
size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos, const char del);
size_t myStrGetTok2(const std::string& str, std::string& tok, size_t pos = 0, const std::string& del = " \t\n\v\f\r");
bool myStr2Int(const std::string& str, int& num);
bool myStr2Uns(const std::string& str, unsigned& num);

template <class T>
requires std::floating_point<T>
bool myStr2FloatType(const std::string& str, T& f);

bool myStr2Float(const std::string& str, float& f);
bool myStr2Double(const std::string& str, double& f);
bool myStr2LongDouble(const std::string& str, long double& f);

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

#endif  // QSYN_UTIL_H
