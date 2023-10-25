/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define concepts and templates for argparse::ArgType ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <span>
#include <string>

#include "util/dvlab_string.hpp"

namespace dvlab::argparse {

struct Token {
    Token(std::string const& tok)
        : token{tok}, parsed{false} {}
    std::string token;
    bool parsed;
};

using TokensView = std::span<Token>;

template <typename T>
std::string type_string(T const&);
template <typename T>
bool parse_from_string(T& val, std::string const& token);

template <typename T>
concept valid_argument_type = requires(T t) {
    { type_string(t) } -> std::same_as<std::string>;
    { parse_from_string(t, std::string{}) } -> std::same_as<bool>;
};

template <typename T>
requires std::is_arithmetic_v<T>
bool parse_from_string(T& val, std::string const& token) { return dvlab::str::str_to_num<T>(token, val); }

// NOTE - keep in the header to shadow the previous definition (bool is arithmetic type)
template <>
bool parse_from_string(bool& val, std::string const& token);

struct DummyArgType {};

}  // namespace dvlab::argparse
