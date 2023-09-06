/****************************************************************************
  FileName     [ util.cpp ]
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

using namespace std;

//----------------------------------------------------------------------
//    Global functions in util
//----------------------------------------------------------------------
#ifndef NDEBUG
void dvlab::detail::dvlab_assert_impl(const char* expr_str, bool expr, const char* file, int line, const char* msg) {
    if (!expr) {
        fprintf(stderr, "Assertion failed:\t%s\n", msg);
        fprintf(stderr, "Expected:\t%s\n", expr_str);
        fprintf(stderr, "Source:\t\t%s, line %d\n", file, line);
        abort();
    }
}
#endif

size_t intPow(size_t base, size_t n) {
    if (n == 0) return 1;
    if (n == 1) return base;
    size_t tmp = intPow(base, n / 2);
    if (n % 2 == 0)
        return tmp * tmp;
    else
        return base * tmp * tmp;
}

TqdmWrapper::TqdmWrapper(size_t total, bool show)
    : _counter(0), _total(total), _tqdm(make_unique<tqdm>(show)) {}

TqdmWrapper::TqdmWrapper(int total, bool show) : TqdmWrapper(static_cast<size_t>(total), show) {}

TqdmWrapper::~TqdmWrapper() {
    _tqdm->finish();
}

void TqdmWrapper::add() {
    _tqdm->progress(_counter++, _total);
}

namespace dvlab {

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

}  // namespace dvlab