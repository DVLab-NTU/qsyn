/**************************************************
  PackageName  [ zx ]
  Synopsis     [ Implements the split function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <vector>

#include "./zx_def.hpp"

namespace qsyn {

namespace zx {

std::vector<ZXVertexList> kl_partition(ZXGraph const& graph, size_t n_partitions);

}

}  // namespace qsyn