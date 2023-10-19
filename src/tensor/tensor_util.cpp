/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define Tensor base class interface member function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tensor_util.hpp"

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
    if (!ax.empty()) {
        std::cout << *ax.begin();
        std::for_each(ax.begin() + 1, ax.end(), [](size_t const& id) { std::cout << " " << id; });
    }
    std::cout << std::endl;
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
