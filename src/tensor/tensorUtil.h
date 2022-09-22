/****************************************************************************
  FileName     [ tensorUtil.h ]
  PackageName  [ tensor ]
  Synopsis     [ Definition of the Tensor base class interface ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 9 ]
****************************************************************************/
#ifndef TENSOR_UTIL_H
#define TENSOR_UTIL_H

#include <xtensor/xstorage.hpp>

using TensorShape = xt::svector<size_t>;
using TensorIndex = std::vector<size_t>;
using TensorAxisList = std::vector<size_t>;

TensorAxisList concatAxisList(const TensorAxisList& ax1, const TensorAxisList& ax2);

bool isDisjoint(const TensorAxisList& ax1, const TensorAxisList& ax2);

#endif //TENSOR_UTIL_H