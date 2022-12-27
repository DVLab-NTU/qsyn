/****************************************************************************
  FileName     [ util.h ]
  PackageName  [ util ]
  Synopsis     [ Define the prototypes of the exported utility functions ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef UTIL_H
#define UTIL_H

#include <concepts>
#include <istream>
#include <vector>

#include "myUsage.h"
#include "rnGen.h"

// Extern global variable defined in util.cpp
extern RandomNumGen rnGen;
extern MyUsage myUsage;

// In myString.cpp
extern bool stripQuotes(const std::string& input, std::string& output);
extern string stripQuotationMarksInternal(const std::string& str);
extern int myStrNCmp(const std::string& s1, const std::string& s2, unsigned n);
extern size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos = 0,
                          const char del = ' ');
extern size_t myStrGetTok2(const std::string& str, std::string& tok, size_t pos = 0,
                           const char del = ' ');
extern bool myStr2Int(const std::string& str, int& num);
extern bool myStr2Uns(const std::string& str, unsigned& num);

template <class T>
requires std::floating_point<T>
extern bool myStr2FloatType(const std::string& str, T& f);

extern bool myStr2Float(const std::string& str, float& f);
extern bool myStr2Double(const std::string& str, double& f);
extern bool myStr2LongDouble(const std::string& str, long double& f);

extern bool isValidVarName(const std::string& str);

// In myGetChar.cpp
extern char myGetChar(istream&);
extern char myGetChar();

// In util.cpp
extern int listDir(vector<std::string>&, const std::string&, const std::string&);
extern size_t getHashSize(size_t s);
extern size_t intPow(size_t base, size_t n);

template <typename T>
bool contains(const std::vector<T>& vec, const T& t) {
    return (std::find(vec.begin(), vec.end(), t) != vec.end());
}

template <typename T>
size_t findIndex(const std::vector<T>& vec, const T& t) {
    return std::find(vec.begin(), vec.end(), t) - vec.begin();
}

// Other utility template functions
template <class T>
void clearList(T& l) {
    T tmp;
    l.swap(tmp);
}

template <class T, class D>
void removeData(T& l, const D& d) {
    size_t des = 0;
    for (size_t i = 0, n = l.size(); i < n; ++i) {
        if (l[i] != d) {  // l[i] will be kept, so des should ++
            if (i != des) l[des] = l[i];
            ++des;
        }
        // else l[i] == d; to be removed, so des won't ++
    }
    l.resize(des);
}

#endif  // UTIL_H
