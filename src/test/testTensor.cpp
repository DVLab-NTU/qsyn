/****************************************************************************
  FileName     [ testTensor.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for QTensor class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cmath>
#include <complex>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <xtensor/xarray.hpp>

#include "qtensor.h"
#include "util.h"

// --- Always put catch2/catch.hpp after all other header files ---
#include "catch2/catch.hpp"
// ----------------------------------------------------------------
using namespace std::literals::complex_literals;

TEST_CASE("Brace Initialization", "[Tensor]") {
    QTensor<double> t1 = {{1. + 0.i, 0. + 0.i}, {0. + 0.i, 1. + 0.i}};
    REQUIRE(t1 == QTensor<double>::zspider(2, 0));
}
TEST_CASE("Z-Spider initiation", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    auto m = GENERATE(Phase(0), Phase(1), Phase(1, 4), Phase(0.00000001));
    QTensor<double> tensor = QTensor<double>::zspider(n, m);

    std::complex<double> all0 = 1.;
    std::complex<double> all1 = std::exp(1.0i * m.toDouble());
    if (n == 0) {
        REQUIRE(tensor() == (all0 + all1) * std::pow(2, 0.25 * (n - 2)));
    } else {
        for (size_t i = 0; i < intPow(2, n); ++i) {
            TensorIndex id;
            for (int j = 0; j < n; ++j) {
                id.emplace_back((i >> j) % 2);
            }
            if (i == 0) {
                REQUIRE(tensor[id] == all0 * std::pow(2, 0.25 * (n - 2)));
            } else if (i == (intPow(2, n) - 1)) {
                REQUIRE(tensor[id] == all1 * std::pow(2, 0.25 * (n - 2)));
            } else {
                REQUIRE(tensor[id] == 0.0);
            }
        }
    }
}

TEST_CASE("X-Spider initiation", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    auto m = GENERATE(Phase(0), Phase(1), Phase(1, 4), Phase(0.00000001));
    QTensor<double> tensor = QTensor<double>::xspider(n, m);
    std::complex<double> expm = std::exp(1.0i * m.toDouble());
    std::complex<double> even = (1. + expm) / std::pow(std::sqrt(2), n);
    std::complex<double> odd = (1. - expm) / std::pow(std::sqrt(2), n);
    if (n == 0) {
        REQUIRE(tensor() == even * std::pow(2, 0.25 * (n - 2)));
    } else {
        for (size_t i = 0; i < intPow(2, n); ++i) {
            TensorIndex id;
            for (int j = 0; j < n; ++j) {
                id.emplace_back((i >> j) % 2);
            }
            if (std::accumulate(id.begin(), id.end(), 0) % 2 == 0) {
                REQUIRE(tensor[id] == even * std::pow(2, 0.25 * (n - 2)));
            } else {
                REQUIRE(tensor[id] == odd * std::pow(2, 0.25 * (n - 2)));
            }
        }
    }
}

TEST_CASE("H-Box initiation from phase", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    auto m = GENERATE(std::exp(1.0i * Phase(0).toDouble()),
                      std::exp(1.0i * Phase(1, 4).toDouble()),
                      std::exp(1.0i * Phase(0.00000001).toDouble()),
                      2.0, -1., 0.00000001 + 0.000000001i);
    QTensor<double> tensor = QTensor<double>::hbox(n, m);

    std::complex<double> all1 = m;
    if (n == 0) {
        REQUIRE(tensor() == all1);
    } else {
        for (size_t i = 0; i < intPow(2, n); ++i) {
            TensorIndex id;
            for (int j = 0; j < n; ++j) {
                id.emplace_back((i >> j) % 2);
            }
            if (i == (intPow(2, n) - 1)) {
                REQUIRE(tensor[id] == all1 * std::pow(2, -0.25 * n));
            } else {
                REQUIRE(tensor[id] == std::pow(2, -0.25 * n));
            }
        }
    }
}

TEST_CASE("Default Parameters for Tensor generators", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    REQUIRE(QTensor<double>::zspider(n) == QTensor<double>::zspider(n, 0));
    REQUIRE(QTensor<double>::xspider(n) == QTensor<double>::xspider(n, 0));
    REQUIRE(QTensor<double>::hbox(n) == QTensor<double>::hbox(n, -1.));
}