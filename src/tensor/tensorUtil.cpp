/****************************************************************************
  FileName     [ tensorUtil.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define Tensor base class interface member function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "tensorUtil.h"

#include <iostream>

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
TensorAxisList concatAxisList(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    TensorAxisList ax = ax1;
    ax.insert(ax.end(), ax2.begin(), ax2.end());
    return ax;
}

/**
 * @brief Print the axis list
 *
 * @param ax
 */
void printAxisList(const TensorAxisList& ax) {
    if (!ax.empty()) {
        std::cout << *ax.begin();
        std::for_each(ax.begin() + 1, ax.end(), [](const size_t& id) { std::cout << " " << id; });
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
bool isDisjoint(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    return std::find_first_of(ax1.begin(), ax1.end(), ax2.begin(), ax2.end()) == ax1.end();
}