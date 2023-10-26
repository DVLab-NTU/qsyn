/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Customized string processing functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./dvlab_string.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <concepts>
#include <cstddef>

namespace dvlab {

namespace str {

/**
 * @brief strip the leading whitespaces of a string
 *
 * @param str
 */
std::string trim_leading_spaces(std::string_view str) {
    auto const start = str.find_first_not_of(" \t\n\v\f\r");
    if (start == std::string::npos) return "";
    return std::string{str.substr(start)};
}

/**
 * @brief strip the leading and trailing whitespaces of a string
 *
 * @param str
 */
std::string trim_spaces(std::string_view str) {
    auto const start = str.find_first_not_of(" \t\n\v\f\r");
    auto const end   = str.find_last_not_of(" \t\n\v\f\r");
    if (start == std::string::npos && end == std::string::npos) return "";
    return std::string{str.substr(start, end + 1 - start)};
}

/**
 * @brief Remove brackets and strip the spaces
 *
 * @param str
 * @param left {, [, (
 * @param right }, ], )
 * @return string
 */
std::string remove_brackets(std::string const& str, char const left, char const right) {
    auto const last_found  = str.find_last_of(right);
    auto const first_found = str.find_first_of(left);
    return trim_spaces(str.substr(first_found + 1, last_found - first_found - 1));
}

// Parse the string "str" for the token "tok", beginning at position "pos",
// with delimiter "delim". The leading "delim" will be skipped.
// Return "string::npos" if not found. Return the past to the end of "tok"
// (i.e. "delim" or string::npos) if found.
// This function will not treat '\ ' as a space in the token. That is, "a\ b" is two token ("a\", "b") and not one
size_t
str_get_token(std::string_view str, std::string& tok, size_t pos, std::string const& delim) {
    auto const begin = str.find_first_not_of(delim, pos);
    if (begin == std::string::npos) {
        tok = "";
        return begin;
    }
    auto const end = str.find_first_of(delim, begin);
    tok            = str.substr(begin, end - begin);
    return end;
}

size_t str_get_token(std::string_view str, std::string& tok, size_t pos, char const delim) {
    return str_get_token(str, tok, pos, std::string(1, delim));
}

/**
 * @brief type-safe conversion to lower case character
 *
 * @param ch
 * @return char
 */
char tolower(char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

/**
 * @brief type-safe conversion to upper case character
 *
 * @param ch
 * @return char
 */
char toupper(char ch) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
}

/**
 * @brief type-safe conversion to lower case string
 *
 * @param str
 * @return std::string
 */
std::string tolower_string(std::string_view str) {
    std::string ret{str};
    std::for_each(ret.begin(), ret.end(), [](char& ch) { ch = dvlab::str::tolower(ch); });
    return ret;
};

/**
 * @brief type-safe conversion to upper case string
 *
 * @param str
 * @return std::string
 */
std::string toupper_string(std::string_view str) {
    std::string ret{str};
    std::for_each(ret.begin(), ret.end(), [](char& ch) { ch = dvlab::str::toupper(ch); });
    return ret;
};

//---------------------------------------------
// number parsing
//---------------------------------------------

}  // namespace str

}  // namespace dvlab
