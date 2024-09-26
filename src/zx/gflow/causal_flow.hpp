/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define class CausalFlow structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "zx/zxgraph.hpp"

namespace qsyn::zx {
// 3-tuple containing an order labelling, the successor function of the flow, and the maximum depth reached.

struct CausalFlow {
    std::unordered_map<size_t, size_t> order;      // vertex ID to order labelling
    std::unordered_map<size_t, size_t> successor;  // vertex ID to successor vertex ID
    size_t depth;                                  // maximum depth reached
};

std::optional<CausalFlow> causal_flow(ZXGraph const& g);

}  // namespace qsyn::zx
