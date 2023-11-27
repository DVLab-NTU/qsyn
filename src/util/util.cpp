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
#include <tqdm/tqdm.hpp>
#include <vector>

#include "./usage.hpp"

//----------------------------------------------------------------------
//    Global functions in util
//----------------------------------------------------------------------
#ifndef NDEBUG
#endif

namespace dvlab {

void detail::dvlab_assert_impl(std::string_view expr_str, bool expr, std::string_view file, int line, std::string_view msg) {
    if (!expr) {
        fmt::println(stderr, "Assertion failed:\t{}", msg);
        fmt::println(stderr, "Expected:\t{}", expr_str);
        fmt::println(stderr, "Source:\t\t{}, line {}\n", file, line);
        abort();
    }
}

void detail::dvlab_abort_impl(std::string_view file, int line, std::string_view msg) {
    fmt::println(stderr, "Abort:\t{}", msg);
    fmt::println(stderr, "Source:\t\t{}, line {}\n", file, line);
    abort();
}

void detail::dvlab_unreachable_impl(std::string_view file, int line, std::string_view msg) {
    fmt::println(stderr, "Unreachable:\t{}", msg);
    fmt::println(stderr, "Source:\t\t{}, line {}\n", file, line);
    abort();
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

}  // namespace utils

TqdmWrapper::TqdmWrapper(size_t total, bool show)
    : _total{gsl::narrow<CounterType>(total)}, _tqdm{std::make_unique<tqdm>(show)} {}

TqdmWrapper::~TqdmWrapper() {
    _tqdm->finish();
}

TqdmWrapper& TqdmWrapper::operator++() {
    _tqdm->progress(_counter++, _total);
    return *this;
}

}  // namespace dvlab
