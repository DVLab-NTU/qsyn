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

namespace detail {
struct heterogeneous_string_hash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(std::string_view str) const noexcept { return std::hash<std::string_view>{}(str); }
    [[nodiscard]] size_t operator()(std::string const& str) const noexcept { return std::hash<std::string>{}(str); }
    [[nodiscard]] size_t operator()(char const* const str) const noexcept { return std::hash<std::string_view>{}(str); }
};
}  // namespace detail

struct Token {
    Token(std::string_view tok)
        : token{tok} {}
    std::string token;
    bool parsed = false;
};

using TokensSpan = std::span<Token>;

template <typename T>
std::string type_string(T const&);
template <typename T>
bool parse_from_string(T& val, std::string_view token);

template <typename T>
concept valid_argument_type = requires(T t) {
    { type_string(t) } -> std::same_as<std::string>;
    { parse_from_string(t, std::string{}) } -> std::same_as<bool>;
};

template <typename T>
requires std::is_arithmetic_v<T>
bool parse_from_string(T& val, std::string_view token) {
    auto result = dvlab::str::from_string<T>(token);
    if (result.has_value()) {
        val = result.value();
        return true;
    }
    return false;
}

// NOTE - keep in the header to shadow the previous definition (bool is arithmetic type)
template <>
bool parse_from_string(bool& val, std::string_view token);

struct DummyArgType {};

}  // namespace dvlab::argparse
