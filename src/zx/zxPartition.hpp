/**************************************************
  FileName     [ zxPartition.h ]
  PackageName  [ zx ]
  Synopsis     [ Implements the split function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <vector>

#include "./zxDef.hpp"

std::vector<ZXVertexList> klPartition(const ZXGraph& graph, size_t numPartitions);
