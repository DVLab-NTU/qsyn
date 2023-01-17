/****************************************************************************
  FileName     [ zxRules.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class ZXRule member function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRules.h"

#include <cstddef>  // for size_t

using namespace std;

extern size_t verbose;

/**
 * @brief Reset remove vertices, remove edges, and edge table
 *
 */
void ZXRule::reset() {
    _matchTypeVecNum = 0;
    _removeVertices.clear();
    _removeEdges.clear();
    _edgeTableKeys.clear();
    _edgeTableValues.clear();
}
