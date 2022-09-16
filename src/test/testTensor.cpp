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

#include "catch2/catch.hpp"
// #include "tensor.h"

// TEST_CASE("xtensor-blas", "[Tensor]") {
//     using namespace std::complex_literals;
//     // Z-spider
//     xt::xarray<std::complex<double>> a = xt::zeros<std::complex<double>>(std::vector<size_t>(3, 2));
//     a[std::vector<size_t>(3, 0)] = (1.0 + 0.0i);
//     a[std::vector<size_t>(3, 1)] = (1.0 + 0.0i);
//     // X-spider (*sqrt2)
//     xt::xarray<std::complex<double>> b = xt::zeros<std::complex<double>>(std::vector<size_t>(3, 2));
//     b(0, 0, 0) = 1.0;
//     b(0, 1, 1) = 1.0;
//     b(1, 0, 1) = 1.0;
//     b(1, 1, 0) = 1.0;
//     // ZX connection
//     auto c = xt::linalg::tensordot(a, b, {0}, {0});
//     // std::cout << c << std::endl;
//     // matrixfy
//     xt::xarray<std::complex<double>> d = xt::transpose(c, {0, 2, 1, 3});
//     auto e = d.reshape({4, 4});
//     // std::cout << e << std::endl;
// }

TEST_CASE("Tensor", "[Tensor]") {
    using DataType = double;
    Tensor<DataType> a = Tensor<DataType>::zspider(4, Phase(1, 2));
    
    std::cout << a << std::endl;
}
// nested_initializer_list_t<int, 2> t = {{1, 2, 3}, {4, 5, 6}};
// Tensor<int> t1(t);

// TEST_CASE("Tensors are initiated correctly", "[Tensor]") {
//     Tensor t1({2, 3, 4}, 0);
//     std::cout << t1 << std::endl;
//     for (size_t i = 0; i < 2; ++i) {
//         for (size_t j = 0; j < 3; ++j) {
//             for (size_t k = 0; k < 4; ++k) {
//                 t1({i, j, k}) = i * 12 + j * 4 + k;
//             }
//         }
//     }
//     std::cout << t1 << std::endl;
//     t1.transpose(0, 2);
//     std::cout << t1 << std::endl;
//     t1.reshape({4, 6});
//     std::cout << t1 << std::endl;

//     std::cout << t1.getShapeString() << std::endl;
//     std::cout << t1.getStridesString() << std::endl;
// }
