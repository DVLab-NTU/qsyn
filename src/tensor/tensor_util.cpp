/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define Tensor base class interface member function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tensor_util.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>

namespace qsyn::tensor {

//------------------------------
// Helper functions
//------------------------------

/**
 * @brief Concat Two Axis Orders
 *
 * @param ax1
 * @param ax2
 * @return TensorAxisList
 */
TensorAxisList concat_axis_list(TensorAxisList const& ax1, TensorAxisList const& ax2) {
    TensorAxisList ax = ax1;
    ax.insert(ax.end(), ax2.begin(), ax2.end());
    return ax;
}

/**
 * @brief Print the axis list
 *
 * @param ax
 */
void print_axis_list(TensorAxisList const& ax) {
    fmt::println("{}", fmt::join(ax, ", "));
}

/**
 * @brief Check if two axis lists are disjoint
 *
 * @param ax1
 * @param ax2
 * @return true
 * @return false
 */
bool is_disjoint(TensorAxisList const& ax1, TensorAxisList const& ax2) {
    return std::find_first_of(ax1.begin(), ax1.end(), ax2.begin(), ax2.end()) == ax1.end();
}

}  // namespace qsyn::tensor
