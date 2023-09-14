/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define global utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "util/util.hpp"

#include <fmt/core.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "./usage.hpp"

//----------------------------------------------------------------------
//    Global functions in util
//----------------------------------------------------------------------
#ifndef NDEBUG
#endif

namespace dvlab {

void detail::dvlab_assert_impl(char const* expr_str, bool expr, char const* file, int line, char const* msg) {
    if (!expr) {
        fmt::println(stderr, "Assertion failed:\t{}", msg);
        fmt::println(stderr, "Expected:\t{}", expr_str);
        fmt::println(stderr, "Source:\t\t{}, line {}\n", file, line);
        abort();
    }
}

namespace utils {

bool expect(bool condition, std::string const& msg) {
    if (!condition) {
        if (!msg.empty()) {
            fmt::println(stderr, "{}", msg);
        }
        return false;
    }
    return true;
}

size_t int_pow(size_t base, size_t n) {
    if (n == 0) return 1;
    if (n == 1) return base;
    size_t tmp = int_pow(base, n / 2);
    if (n % 2 == 0)
        return tmp * tmp;
    else
        return base * tmp * tmp;
}

}  // namespace utils

}  // namespace dvlab