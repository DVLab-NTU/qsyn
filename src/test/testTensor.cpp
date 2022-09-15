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
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor-blas/xlinalg.hpp>

#include "catch2/catch.hpp"
// #include "tensor.h"

TEST_CASE("xtensor-blas", "[Tensor]") {
    xt::xarray<double> a = {{3, 2, 1}, {0, 4, 2}, {1, 3, 5}};
    auto d = xt::linalg::det(a);
    REQUIRE(d == 42);
}

