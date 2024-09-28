/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./zxgraph.hpp"

namespace qsyn::zx {

void toggle_vertex(ZXGraph& graph, size_t v_id);
std::optional<size_t> add_identity_vertex(
    ZXGraph& graph, size_t left_id, size_t right_id, EdgeType etype_to_left);
void gadgetize_phase(
    ZXGraph& graph, size_t v_id, Phase const& keep_phase = Phase(0));

}  // namespace qsyn::zx
