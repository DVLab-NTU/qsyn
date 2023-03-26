/****************************************************************************
  FileName     [ myString.cpp ]
  PackageName  [ util ]
  Synopsis     [ Customized string processing functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <ctype.h>  // for tolower, etc.

#include <cassert>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "util.h"

using namespace std;

// Remove quotation marks and replace the ' ' between the quotes to be "\ "
bool stripQuotes(const std::string& input, std::string& output) {
    output = input;
    if (input == "") {
        return true;
    }

    const string refStr = input;
    vector<string> outside;
    vector<string> inside;

    auto findQuote = [&output](char quote) -> size_t {
        size_t pos = 0;
        pos = output.find_first_of(quote);
        while (pos != 0 && output[pos - 1] == '\\') {
            pos = output.find_first_of(quote, pos + 1);
        }
        return pos;
    };

    while (output.size()) {
        size_t doubleQuote = findQuote('\"');
        size_t singleQuote = findQuote('\'');
        size_t pos = min(doubleQuote, singleQuote);
        char delim = output[pos];
        outside.emplace_back(output.substr(0, pos));
        if (pos == string::npos) break;
        output = output.substr(pos + 1);
        if (pos != string::npos) {
            size_t closingQuote = findQuote(delim);

            if (closingQuote == string::npos) {
                output = refStr;
                return false;
            }

            inside.emplace_back(output.substr(0, closingQuote));
            output = output.substr(closingQuote + 1);
        }
    }

    // 2. inside  ' ' --> "\ "
    //    both side \' --> ' , \" --> "

    auto deformatQuotes = [](vector<string>& strs) {
        for (auto& str : strs) {
            size_t pos = 0;
            while ((pos = str.find("\\\"", pos)) != string::npos) {
                str = str.substr(0, pos) + str.substr(pos + 1);
            }
            pos = 0;
            while ((pos = str.find("\\\'", pos)) != string::npos) {
                str = str.substr(0, pos) + str.substr(pos + 1);
            }
        }
    };

    for (auto& str : inside) {
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == ' ') {
                str.insert(i, "\\");
                i++;
            }
        }
    }

    deformatQuotes(outside);
    deformatQuotes(inside);

    // 3. recombine
    output = "";
    for (size_t i = 0; i < outside.size(); ++i) {
        output += outside[i];
        if (i < inside.size()) {
            output += inside[i];
        }
    }

    return true;
}

/**
 * @brief strip the leading and trailing whitespaces of a string
 *
 * @param str
 */
string stripWhitespaces(const string& str) {
    size_t start = str.find_first_not_of(" \t\n\v\f\r");
    size_t end = str.find_last_not_of(" \t\n\v\f\r");
    if (start == string::npos && end == string::npos) return "";
    return str.substr(start, end + 1 - start);
}

/**
 * @brief Strip leading spaces and comments
 *
 * @param line
 * @return string
 */
string stripLeadingSpacesAndComments(string& line) {
    size_t firstNonSpace = line.find_first_not_of(" ");
    size_t commentStart = line.find("//");
    if (firstNonSpace == string::npos) return "";
    if (firstNonSpace == commentStart) return "";

    return line.substr(firstNonSpace, commentStart - firstNonSpace);
}

/**
 * @brief Remove brackets and strip the spaces
 *
 * @param str
 * @param left {, [, (
 * @param right }, ], )
 * @return string
 */
string removeBracket(const std::string& str, const char left, const char right) {
    size_t lastfound = str.find_last_of(right);
    size_t firstfound = str.find_first_of(left);
    return stripWhitespaces(str.substr(firstfound + 1, lastfound - firstfound - 1));
}
// 1. strlen(s1) must >= n
// 2. The first n characters of s2 are mandatory, they must be case-
//    insensitively compared to s1. Return less or greater than 0 if unequal.
// 3. The rest of s2 are optional. Return 0 if EOF of s2 is encountered.
//    Otherwise, perform case-insensitive comparison until non-equal result
//    presents.
//
int myStrNCmp(const string& s1, const string& s2, unsigned n) {
    assert(n > 0);
    unsigned n2 = s2.size();
    if (n2 == 0) return -1;
    unsigned n1 = s1.size();
    assert(n1 >= n);
    for (unsigned i = 0; i < n1; ++i) {
        if (i == n2)
            return (i < n) ? 1 : 0;
        char ch1 = (isupper(s1[i])) ? tolower(s1[i]) : s1[i];
        char ch2 = (isupper(s2[i])) ? tolower(s2[i]) : s2[i];
        if (ch1 != ch2)
            return (ch1 - ch2);
    }
    return (n1 - n2);
}

// Parse the string "str" for the token "tok", beginning at position "pos",
// with delimiter "del". The leading "del" will be skipped.
// Return "string::npos" if not found. Return the past to the end of "tok"
// (i.e. "del" or string::npos) if found.
// This function will not treat '\ ' as a space in the token. That is, "a\ b" is two token ("a\", "b") and not one
size_t
myStrGetTok(const string& str, string& tok, size_t pos, const string& del) {
    size_t begin = str.find_first_not_of(del, pos);
    if (begin == string::npos) {
        tok = "";
        return begin;
    }
    size_t end = str.find_first_of(del, begin);
    tok = str.substr(begin, end - begin);
    return end;
}

size_t
myStrGetTok(const string& str, string& tok, size_t pos, const char del) {
    return myStrGetTok(str, tok, pos, string(1, del));
}

// Parse the string "str" for the token "tok", beginning at position "pos",
// with delimiter "del". The leading "del" will be skipped.
// Return "string::npos" if not found. Return the past to the end of "tok"
// (i.e. "del" or string::npos) if found.
// This function will treat '\ ' as a space in the token. That is, "a\ b" is one token ("a b") and not two
size_t
myStrGetTok2(const string& str, string& tok, size_t pos, const std::string& del) {
    size_t begin = str.find_first_not_of(del, pos);
    if (begin == string::npos) {
        tok = "";
        return begin;
    }
    size_t end = str.find_first_of(del, begin);
    tok = str.substr(begin, end - begin);
    if (tok.back() == '\\') {
        string tok2;
        end = myStrGetTok2(str, tok2, end);
        tok = tok.substr(0, tok.size() - 1) + ' ' + tok2;
    }
    return end;
}

std::string toLowerString(std::string const& str) {
    std::string ret = str;
    for_each(ret.begin(), ret.end(), [](char& ch) { ch = ::tolower(ch); });
    return ret;
};

std::string toUpperString(std::string const& str) {
    std::string ret = str;
    for_each(ret.begin(), ret.end(), [](char& ch) { ch = ::toupper(ch); });
    return ret;
};

size_t countUpperChars(std::string const& str) {
    size_t cnt = 0;
    for (auto& ch : str) {
        if (::islower(ch)) return cnt;
        ++cnt;
    }
    return str.size();
};

//---------------------------------------------
// number parsing
//---------------------------------------------

/**
 * @brief An indirection layer for std::stoXXX(const string& str, size_t* pos = nullptr).
 *        All the dirty compile-time checking happens here.
 *
 * @tparam T
 * @param str
 * @param pos
 * @return requires
 */
template <class T>
requires Arithmetic<T>
T stoNumber(const string& str, size_t* pos) {
    try {
        // floating point types
        if constexpr (std::is_same<T, double>::value) return std::stod(str, pos);
        if constexpr (std::is_same<T, float>::value) return std::stof(str, pos);
        if constexpr (std::is_same<T, long double>::value) return std::stold(str, pos);

        // signed integer types
        if constexpr (std::is_same<T, int>::value) return std::stoi(str, pos);
        if constexpr (std::is_same<T, long>::value) return std::stol(str, pos);
        if constexpr (std::is_same<T, long long>::value) return std::stoll(str, pos);

        // unsigned integer types
        if constexpr (std::is_same<T, unsigned>::value) {
            if (stripWhitespaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            unsigned long result = std::stoul(str, pos);  // NOTE - for some reason there isn't stou (lol)
            if (result > std::numeric_limits<unsigned>::max()) {
                throw std::out_of_range("stou");
            }
            return (unsigned)result;
        }
        if constexpr (std::is_same<T, unsigned long>::value) {
            if (stripWhitespaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            return std::stoul(str, pos);
        }
        if constexpr (std::is_same<T, unsigned long long>::value) {
            if (stripWhitespaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            return std::stoull(str, pos);
        }

        throw std::invalid_argument("unsupported type");
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(e.what());
    } catch (const std::out_of_range& e) {
        throw std::out_of_range(e.what());
    }

    return 0.;  // silences compiler warnings
}

/**
 * @brief If `str` is a number of type T, parse it and return true;
 *        otherwise, return false.
 *
 * @tparam T
 * @param str
 * @param f
 * @return requires
 */
template <class T>
requires Arithmetic<T>
bool myStr2Number(const string& str, T& f) {
    size_t i;
    try {
        f = stoNumber<T>(str, &i);
    } catch (const std::exception& e) {
        return false;
    }
    // Check if str have un-parsable parts
    return (i == str.size());
}

#define MYSTR2NUMBER_INSTANTIATION(_type) \
    template bool myStr2Number<_type>(const string&, _type&)

MYSTR2NUMBER_INSTANTIATION(float);
MYSTR2NUMBER_INSTANTIATION(double);
MYSTR2NUMBER_INSTANTIATION(long double);

MYSTR2NUMBER_INSTANTIATION(int);
MYSTR2NUMBER_INSTANTIATION(long);
MYSTR2NUMBER_INSTANTIATION(long long);

MYSTR2NUMBER_INSTANTIATION(unsigned);
MYSTR2NUMBER_INSTANTIATION(unsigned long);
MYSTR2NUMBER_INSTANTIATION(unsigned long long);

// no need to instantiate for size_t: it is just an type alias