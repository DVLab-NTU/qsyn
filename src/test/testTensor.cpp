/****************************************************************************
  FileName     [ testPhase.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for Phase class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
// #include <xtensor/xarray.hpp>
// #include <xtensor/xio.hpp>
// #include <xtensor-blas/xlinalg.hpp>
#include <complex>
#include <cmath>
#include "tensor.h"
#include "util.h"

#include "catch2/catch.hpp"
// #include "tensor.h"
using namespace std::literals::complex_literals;

TEST_CASE("Z-Spider initiation", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    auto m = GENERATE(Phase(0), Phase(1), Phase(1, 4), Phase(0.00000001));
    Tensor<double> tensor = Tensor<double>::zspider(n, m);

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
            }
            else if (i == (intPow(2, n) - 1)) {
                REQUIRE(tensor[id] == all1 * std::pow(2, 0.25 * (n - 2)));
            }
            else {
                REQUIRE(tensor[id] == 0.0);
            }
        }
    }
}

TEST_CASE("X-Spider initiation", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    auto m = GENERATE(Phase(0), Phase(1), Phase(1, 4), Phase(0.00000001));
    Tensor<double> tensor = Tensor<double>::xspider(n, m);
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
            }
            else {
                REQUIRE(tensor[id] == odd * std::pow(2, 0.25 * (n - 2)));
            }
        }
    }
}

TEST_CASE("H-Box initiation from phase", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    auto m = GENERATE(std::exp(1.0i*Phase(0).toDouble()), 
                      std::exp(1.0i*Phase(1,4).toDouble()), 
                      std::exp(1.0i*Phase(0.00000001).toDouble()), 
                      2.0, -1., 0.00000001 + 0.000000001i);
    Tensor<double> tensor = Tensor<double>::hbox(n, m);

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
            }
            else {
                REQUIRE(tensor[id] == std::pow(2, -0.25 * n));
            }
        }
    }
}

TEST_CASE("Default Parameters for Tensor generators", "[Tensor]") {
    auto n = GENERATE(0, 1, 4, 9);
    REQUIRE(Tensor<double>::zspider(n) == Tensor<double>::zspider(n, 0));
    REQUIRE(Tensor<double>::xspider(n) == Tensor<double>::xspider(n, 0));
    REQUIRE(Tensor<double>::hbox(n) == Tensor<double>::hbox(n, -1.));
}

// TEST_CASE("Tensordot", "[Tensor]") {
//     Tensor<double> a = Tensor<double>::zspider(3, 0);
//     Tensor<double> b = Tensor<double>::xspider(3, 0);

//     auto c = Tensor<double>::tensordot(a, b, {2}, {0});
//     auto d = c.toMatrix({0, 2}, {1, 3});
//     // std::cout << d << std::endl;

//     Tensor<double> f = Tensor<double>::zspider(4, 0);
//     auto g = Tensor<double>::selfTensordot(f, {1}, {3});
//     std::cout << g - Tensor<double>::zspider(2, 0) << std::endl;

//     REQUIRE(g == Tensor<double>::zspider(2, 0));
//     Tensor<double> h = Tensor<double>::cnz(2);
//     std::cout << h.toMatrix({0, 2, 4}, {1, 3, 5}) << std::endl;
//     Tensor<double> k = Tensor<double>::cnx(2);
//     std::cout << k.toMatrix({0, 2, 4}, {1, 3, 5}) << std::endl;
// }
