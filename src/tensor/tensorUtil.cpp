/****************************************************************************
  FileName     [ tensorUtil.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Definition of the Tensor base class interface ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "tensorUtil.h"

//------------------------------
// Helper functions
//------------------------------

// Concat Two Axis Orders
TensorAxisList concatAxisList(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    TensorAxisList ax = ax1;
    ax.insert(ax.end(), ax2.begin(), ax2.end());
    return ax;
}

// Print the axis list
void printAxisList(const TensorAxisList& ax) {
    if (!ax.empty()) {
        std::cout << *ax.begin();
        std::for_each(ax.begin() + 1, ax.end(), [](const size_t& id) { std::cout << " " << id; });
    }
    std::cout << std::endl;
}

// Returns true if two axis lists are disjoint
bool isDisjoint(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    return std::find_first_of(ax1.begin(), ax1.end(), ax2.begin(), ax2.end()) == ax1.end();
}