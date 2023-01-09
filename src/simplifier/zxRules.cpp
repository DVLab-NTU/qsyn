/****************************************************************************
  FileName     [ zxRules.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Definition for each rules in zxRules.h ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRules.h"

#include <iostream>
#include <unordered_map>
#include <vector>
using namespace std;

extern size_t verbose;

void ZXRule::reset() {
    _matchTypeVecNum = 0;
    _removeVertices.clear();
    _removeEdges.clear();
    _edgeTableKeys.clear();
    _edgeTableValues.clear();
}
