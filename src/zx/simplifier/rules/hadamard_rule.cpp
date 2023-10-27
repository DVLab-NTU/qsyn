/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <unordered_set>

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = HadamardRule::MatchType;

std::vector<MatchType> HadamardRule::find_matches(ZXGraph const& graph) const {
    std::unordered_set<ZXVertex*> taken;
    std::vector<MatchType> matches;

    // Find all H-boxes
    for (auto const& v : graph.get_vertices()) {
        if (!v->is_hbox() || graph.get_num_neighbors(v) != 2) continue;

        auto [nv0, _0] = graph.get_first_neighbor(v);
        auto [nv1, _1] = graph.get_second_neighbor(v);

        if (taken.contains(nv0) || taken.contains(nv1)) continue;

        matches.emplace_back(v);
        taken.insert(v);
        taken.insert(nv0);
        taken.insert(nv1);
    }

    return matches;
}

void HadamardRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op = {
        .vertices_to_remove = matches,
    };

    for (auto& v : matches) {
        // NOTE: Only two neighbors which is ensured
        std::vector<ZXVertex*> neighbor_vertices;
        std::vector<EdgeType> neighbor_edge_types;

        for (auto& [neighbor, edgeType] : graph.get_neighbors(v)) {
            neighbor_vertices.emplace_back(neighbor);
            neighbor_edge_types.emplace_back(edgeType);
        }

        op.edges_to_add.emplace_back(
            std::make_pair(neighbor_vertices[0], neighbor_vertices[1]),
            neighbor_edge_types[0] == neighbor_edge_types[1] ? EdgeType::hadamard : EdgeType::simple);
        // TODO: Correct for the sqrt(2) difference in H-boxes and H-edges
    }

    _update(graph, op);
}
