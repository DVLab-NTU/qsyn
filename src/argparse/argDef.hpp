/****************************************************************************
  FileName     [ argDef.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <span>
#include <string>

namespace ArgParse {

struct Token {
    Token(std::string const& tok)
        : token{tok}, parsed{false} {}
    std::string token;
    bool parsed;
};

using TokensView = std::span<Token>;

template <typename T>
std::string typeString(T const&);
template <typename T>
bool parseFromString(T& val, std::string const& token);

template <typename T>
concept ValidArgumentType = requires(T t) {
    { typeString(t) } -> std::same_as<std::string>;
    { parseFromString(t, std::string{}) } -> std::same_as<bool>;
};

template <typename ArithT>
requires std::is_arithmetic_v<ArithT>
bool parseFromString(ArithT& val, std::string const& token) { return myStr2Number<ArithT>(token, val); }

// NOTE - keep in the header to shadow the previous definition (bool is arithmetic type)
template <>
bool parseFromString(bool& val, std::string const& token);

struct DummyArgType {};

}  // namespace ArgParse