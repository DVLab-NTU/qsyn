/****************************************************************************
  FileName     [ testTensor2.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for Tensor class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
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

TEST_CASE("Tensordot", "[Tensor]") {
    QTensor<double> a = QTensor<double>::zspider(3, 0);
    // a.printAxisHistory();
    QTensor<double> b = QTensor<double>::xspider(3, 0);
    // b.printAxisHistory();
    auto c = tensordot(a, b, {2}, {0});
    // c.printAxisHistory();
    REQUIRE(c.getNewAxisId(0) == 0);
    REQUIRE(c.getNewAxisId(1) == 1);
    REQUIRE(c.getNewAxisId(2) == (size_t) -1);
    REQUIRE(c.getNewAxisId(3) == (size_t) -1);
    REQUIRE(c.getNewAxisId(4) == 2);
    REQUIRE(c.getNewAxisId(5) == 3);
    c.resetAxisHistory();
    REQUIRE(c.getNewAxisId(0) == 0);
    REQUIRE(c.getNewAxisId(1) == 1);
    REQUIRE(c.getNewAxisId(2) == 2);
    REQUIRE(c.getNewAxisId(3) == 3);
    // auto d = c.toMatrix({0, 2}, {1, 3});
    // std::cout << d << std::endl;

    QTensor<double> f = QTensor<double>::zspider(4, 0);
    auto g = f.selfTensordot({1}, {3});
    REQUIRE(g.getNewAxisId(0) == 0);
    REQUIRE(g.getNewAxisId(1) == (size_t) -1);
    REQUIRE(g.getNewAxisId(2) == 1);
    REQUIRE(g.getNewAxisId(3) == (size_t) -1);
    
    // std::cout << g - QTensor<double>::zspider(2, 0) << std::endl;

    // REQUIRE(g == QTensor<double>::zspider(2, 0));
    // QTensor<double> k = QTensor<double>::cny(2);
    // std::cout << k.toMatrix({0, 2, 4}, {1, 3, 5}) << std::endl;

}
