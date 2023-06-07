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
#include <memory>
#include <string>
#include <vector>

#include "myConcepts.h"
#include "myUsage.h"
#include "rnGen.h"
#include "tqdm/tqdm.h"

#define IGNORE_UNUSED_RETURN_WARNING [[maybe_unused]] auto shutup =

class tqdm;
constexpr size_t ERROR_CODE = (size_t)-1;

class TqdmWrapper {
public:
    TqdmWrapper(size_t total, bool = true);
    TqdmWrapper(int total, bool = true);
    ~TqdmWrapper();

    size_t idx() const { return _counter; }
    bool done() const { return _counter == _total; }
    void add();
    TqdmWrapper& operator++() { return add(), *this; }

private:
    size_t _counter;
    size_t _total;

    // Using a pointer so we don't need to know tqdm's size in advance.
    // This way, no need to #include it in the header
    // because although tqdm works well, it's not expertly written
    // which leads to a lot of warnings.
    std::unique_ptr<tqdm> _tqdm;
};

// In myString.cpp

bool stripQuotes(const std::string& input, std::string& output);
std::string stripLeadingWhitespaces(std::string const& str);
std::string stripWhitespaces(std::string const& str);
/**
 * @brief strip comment, which starts with "//", from a string
 *
 * @param line
 * @return std::string
 */
inline std::string stripComments(std::string const& line) { return line.substr(0, line.find("//")); }
std::string removeBracket(const std::string& str, const char left, const char right);
int myStrNCmp(const std::string& s1, const std::string& s2, unsigned n);
size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos = 0, const std::string& del = " \t\n\v\f\r");
size_t myStrGetTok(const std::string& str, std::string& tok, size_t pos, const char del);
size_t myStrGetTok2(const std::string& str, std::string& tok, size_t pos = 0, const std::string& del = " \t\n\v\f\r");

template <class T>
requires Arithmetic<T>
bool myStr2Number(const std::string& str, T& f);

inline bool myStr2Float(const std::string& str, float& num) { return myStr2Number<float>(str, num); };
inline bool myStr2Double(const std::string& str, double& num) { return myStr2Number<double>(str, num); }
inline bool myStr2LongDouble(const std::string& str, long double& num) { return myStr2Number<long double>(str, num); }

inline bool myStr2Int(const std::string& str, int& num) { return myStr2Number<int>(str, num); }
inline bool myStr2Long(const std::string& str, long& num) { return myStr2Number<long>(str, num); }
inline bool myStr2LongLong(const std::string& str, long long& num) { return myStr2Number<long long>(str, num); }

inline bool myStr2Uns(const std::string& str, unsigned& num) { return myStr2Number<unsigned>(str, num); }
inline bool myStr2UnsLong(const std::string& str, unsigned long& num) { return myStr2Number<unsigned long>(str, num); }
inline bool myStr2UnsLongLong(const std::string& str, unsigned long long& num) { return myStr2Number<unsigned long long>(str, num); }

inline bool myStr2SizeT(const std::string& str, size_t& num) { return myStr2Number<size_t>(str, num); }

std::string toLowerString(std::string const& str);
std::string toUpperString(std::string const& str);
size_t countUpperChars(std::string const& str) noexcept;

// In util.cpp
std::vector<std::string> listDir(std::string const& prefix, std::string const& dir = ".");
size_t intPow(size_t base, size_t n);

std::string createTempDir(std::string const& prefix);
std::string createTempFile(std::string const& prefix);

template <typename T>
bool contains(const std::vector<T>& vec, const T& t) {
    return (std::find(vec.begin(), vec.end(), t) != vec.end());
}

template <typename T>
size_t findIndex(const std::vector<T>& vec, const T& t) {
    return std::find(vec.begin(), vec.end(), t) - vec.begin();
}

inline bool implies(bool a, bool b) { return !a || b; }

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    os << "[";
    if (v.empty()) {
        return os << "]";
    }
    for (size_t i = 0; i < v.size() - 1; ++i) {
        os << v[i] << ", ";
    }
    return os << v.back() << "]";
}

#endif  // QSYN_UTIL_H
