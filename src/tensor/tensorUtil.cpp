/****************************************************************************
  FileName     [ tensorUtil.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Definition of the Tensor base class interface ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 9 ]
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

// Returns true if two axis lists are disjoint
bool isDisjoint(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    return std::find_first_of(ax1.begin(), ax1.end(), ax2.begin(), ax2.end()) == ax1.end();
}