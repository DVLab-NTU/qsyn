/****************************************************************************
  FileName     [ testPhase.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for Phase class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include "tensor.h"

#include <iostream>
#include <string>
#include <sstream>
#include <exception>

#include "catch2/catch.hpp"

// nested_initializer_list_t<int, 2> t = {{1, 2, 3}, {4, 5, 6}};
// Tensor<int> t1(t);

TEST_CASE("Tensors are initiated correctly", "[Tensor]") {
    Tensor t1({2, 3, 4}, 0);
    std::cout << t1 << std::endl;
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            for (size_t k = 0; k < 4; ++k) {
                t1({i, j, k}) = i * 12 + j * 4 + k;
            }
        }
    }
    std::cout << t1 << std::endl;
    t1.transpose(0, 2);
    std::cout << t1 << std::endl;
    t1.reshape({4, 6});
    std::cout << t1 << std::endl;

    std::cout << t1.getShapeString() << std::endl;
    std::cout << t1.getStridesString() << std::endl;
}

