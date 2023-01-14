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
    size_t end = str.find_last_not_of(" ");
    if (start == string::npos && end == string::npos) return "";
    return str.substr(start, end + 1 - start);
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
myStrGetTok(const string& str, string& tok, size_t pos = 0,
            const char del = ' ') {
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
myStrGetTok(const string& str, string& tok, size_t pos,
            const string& del) {
    size_t begin = str.find_first_not_of(del, pos);
    if (begin == string::npos) {
        tok = "";
        return begin;
    }
    size_t end = str.find_first_of(del, begin);
    tok = str.substr(begin, end - begin);
    return end;
}

// Parse the string "str" for the token "tok", beginning at position "pos",
// with delimiter "del". The leading "del" will be skipped.
// Return "string::npos" if not found. Return the past to the end of "tok"
// (i.e. "del" or string::npos) if found.
// This function will treat '\ ' as a space in the token. That is, "a\ b" is one token ("a b") and not two
size_t
myStrGetTok2(const string& str, string& tok, size_t pos = 0,
             const char del = ' ') {
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

// Convert string "str" to integer "num". Return false if str does not appear
// to be a number
bool myStr2Int(const string& str, int& num) {
    num = 0;
    size_t i = 0;
    int sign = 1;
    if (str[0] == '-') {
        sign = -1;
        i = 1;
    }
    bool valid = false;
    for (; i < str.size(); ++i) {
        if (isdigit(str[i])) {
            num *= 10;
            num += int(str[i] - '0');
            valid = true;
        } else
            return false;
    }
    num *= sign;
    return valid;
}

// Convert string "str" to unsigned integer "unsnum". Return false if str does not appear
// to be an unsigned number
bool myStr2Uns(const string& str, unsigned& unsnum) {
    int num = 0;
    bool isNum = myStr2Int(str, num);
    if (!isNum || num < 0)
        return false;
    unsnum = (unsigned int)num;
    return true;
}

// A template interface for std::stoXXX(const string& str, size_t* pos = nullptr),
// All the dirty compile-time checking happens here.
template <class T>
requires std::floating_point<T>
    T stoFloatType(const string& str, size_t* pos) {
    try {
        if constexpr (std::is_same<T, double>::value) {
            return std::stod(str, pos);
        } else if constexpr (std::is_same<T, float>::value) {
            return std::stof(str, pos);
        } else if constexpr (std::is_same<T, long double>::value) {
            return std::stold(str, pos);
        } else {
            throw std::invalid_argument("Not a floating point type");
        }
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(e.what());
    } catch (const std::out_of_range& e) {
        throw std::out_of_range(e.what());
    }

    return 0.;  // silences compiler warnings
}
// Generic template for `myStr2<Float|Double|LongDouble>`
// If `str` is a string of decimal number, return true and set `f` to the corresponding number.
// Otherwise return 0 and set `f` to 0.
template <class T>
requires std::floating_point<T>
bool myStr2FloatType(const string& str, T& f) {
    f = 0;
    size_t i;
    try {
        f = stoFloatType<T>(str, &i);
    } catch (const std::exception& e) {
        return false;
    }
    // Check if str have un-parsable parts
    if (i != str.size()) {
        f = 0;
        return false;
    }
    return true;
}

template bool myStr2FloatType<float>(const string&, float&);
template bool myStr2FloatType<double>(const string&, double&);
template bool myStr2FloatType<long double>(const string&, long double&);

bool myStr2Float(const string& str, float& f) {
    return myStr2FloatType<float>(str, f);
}

bool myStr2Double(const string& str, double& f) {
    return myStr2FloatType<double>(str, f);
}

bool myStr2LongDouble(const string& str, long double& f) {
    return myStr2FloatType<long double>(str, f);
}

// Valid var name is ---
// 1. starts with [a-zA-Z_]
// 2. others, can only be [a-zA-Z0-9_]
// return false if not a var name
bool isValidVarName(const string& str) {
    size_t n = str.size();
    if (n == 0) return false;
    if (!isalpha(str[0]) && str[0] != '_')
        return false;
    for (size_t i = 1; i < n; ++i)
        if (!isalnum(str[i]) && str[i] != '_')
            return false;
    return true;
}
