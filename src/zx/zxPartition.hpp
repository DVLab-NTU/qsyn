/**************************************************
  FileName     [ zxPartition.h ]
  PackageName  [ graph ]
  Synopsis     [ Implements the split function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_PARTITION_H
#define ZX_PARTITION_H

#include <vector>

#include "./zxDef.hpp"

std::vector<ZXVertexList> klPartition(const ZXGraph& graph, size_t numPartitions);

#endif
