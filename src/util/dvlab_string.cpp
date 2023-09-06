/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Customized string processing functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "util/util.hpp"

using namespace std;

namespace dvlab {

namespace str {

/**
 * @brief
 *
 * @param input the string to strip quotes
 * @param output if the function returns true, output the stripped string; else output the same string as input.
 * @return true if quotation marks are paired;
 * @return false if quotation marks are unpaired.
 */
std::optional<std::string> strip_quotes(std::string const& input) {
    if (input == "") {
        return input;
    }

    std::string output = input;

    vector<string> outside;
    vector<string> inside;

    auto find_quote = [&output](char quote) -> size_t {
        size_t pos = 0;
        pos = output.find_first_of(quote);
        if (pos == string::npos) return pos;
        // if the quote is after a backslash, it should be read verbatim, so we need to skip it.
        while (pos != 0 && output[pos - 1] == '\\') {
            pos = output.find_first_of(quote, pos + 1);
            if (pos == string::npos) return pos;
        }
        return pos;
    };

    while (output.size()) {
        size_t pos = min(find_quote('\"'), find_quote('\''));

        outside.emplace_back(output.substr(0, pos));
        if (pos == string::npos) break;

        char delim = output[pos];

        output = output.substr(pos + 1);
        if (pos != string::npos) {
            size_t closing_quote_pos = find_quote(delim);

            if (closing_quote_pos == string::npos) {
                return std::nullopt;
            }

            inside.emplace_back(output.substr(0, closing_quote_pos));
            output = output.substr(closing_quote_pos + 1);
        }
    }

    // 2. inside  ' ' --> "\ "
    //    both side \' --> ' , \" --> "

    auto remove_quotes = [](vector<string>& strs) {
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

    remove_quotes(outside);
    remove_quotes(inside);

    // 3. recombine
    output = "";
    for (size_t i = 0; i < outside.size(); ++i) {
        output += outside[i];
        if (i < inside.size()) {
            output += inside[i];
        }
    }

    return output;
}

/**
 * @brief strip the leading whitespaces of a string
 *
 * @param str
 */
string strip_leading_spaces(string const& str) {
    size_t start = str.find_first_not_of(" \t\n\v\f\r");
    if (start == string::npos) return "";
    return str.substr(start);
}

/**
 * @brief strip the leading and trailing whitespaces of a string
 *
 * @param str
 */
string strip_spaces(string const& str) {
    size_t start = str.find_first_not_of(" \t\n\v\f\r");
    size_t end = str.find_last_not_of(" \t\n\v\f\r");
    if (start == string::npos && end == string::npos) return "";
    return str.substr(start, end + 1 - start);
}

/**
 * @brief Return true if the `pos`th character in `str` is escaped, i.e., preceded by a single backslash
 *
 * @param str
 * @param pos
 * @return true
 * @return false
 */
bool is_escaped_char(std::string const& str, size_t pos) {
    return pos > 0 && str[pos - 1] == '\\' && (pos == 1 || str[pos - 2] != '\\');
}

/**
 * @brief Remove brackets and strip the spaces
 *
 * @param str
 * @param left {, [, (
 * @param right }, ], )
 * @return string
 */
string remove_brackets(std::string const& str, char const left, char const right) {
    size_t last_found = str.find_last_of(right);
    size_t first_found = str.find_first_of(left);
    return strip_spaces(str.substr(first_found + 1, last_found - first_found - 1));
}

// Parse the string "str" for the token "tok", beginning at position "pos",
// with delimiter "delim". The leading "delim" will be skipped.
// Return "string::npos" if not found. Return the past to the end of "tok"
// (i.e. "delim" or string::npos) if found.
// This function will not treat '\ ' as a space in the token. That is, "a\ b" is two token ("a\", "b") and not one
size_t
str_get_token(string const& str, string& tok, size_t pos, string const& delim) {
    size_t begin = str.find_first_not_of(delim, pos);
    if (begin == string::npos) {
        tok = "";
        return begin;
    }
    size_t end = str.find_first_of(delim, begin);
    tok = str.substr(begin, end - begin);
    return end;
}

size_t str_get_token(string const& str, string& tok, size_t pos, char const delim) {
    return str_get_token(str, tok, pos, string(1, delim));
}

std::string to_lower_string(std::string const& str) {
    std::string ret = str;
    for_each(ret.begin(), ret.end(), [](char& ch) { ch = ::tolower(ch); });
    return ret;
};

std::string to_upper_string(std::string const& str) {
    std::string ret = str;
    for_each(ret.begin(), ret.end(), [](char& ch) { ch = ::toupper(ch); });
    return ret;
};

std::vector<std::string> split(std::string const& str, std::string const& delim = " ") {
    std::vector<std::string> result;
    string token;
    size_t pos = str_get_token(str, token, 0, delim);
    while (token.size()) {
        result.emplace_back(token);
        pos = str_get_token(str, token, pos, delim);
    }

    return result;
}

std::string join(std::string const& infix, std::span<std::string> strings) {
    std::string result = *strings.begin();

    for (auto& str : strings.subspan(1)) {
        result += infix + str;
    }

    return result;
}

//---------------------------------------------
// number parsing
//---------------------------------------------

}  // namespace str

}  // namespace dvlab