/****************************************************************************
  FileName     [ dataStructureManager.h ]
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "dataStructureManager.hpp"

#include <iostream>

namespace dvlab_utils {

namespace detail {
std::ostream& _cout = std::cout;
std::ostream& _cerr = std::cerr;
}  // namespace detail

}  // namespace dvlab_utils