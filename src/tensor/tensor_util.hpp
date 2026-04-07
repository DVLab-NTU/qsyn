/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define Tensor base class interface ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <cstddef>
#include <vector>
#include <xtensor/containers/xstorage.hpp>

namespace qsyn::tensor {

using TensorShape    = xt::svector<size_t>;
using TensorIndex    = std::vector<size_t>;
using TensorAxisList = std::vector<size_t>;

TensorAxisList concat_axis_list(TensorAxisList const& ax1, TensorAxisList const& ax2);
void print_axis_list(TensorAxisList const& ax);

bool is_disjoint(TensorAxisList const& ax1, TensorAxisList const& ax2);

}  // namespace qsyn::tensor
