/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = HadamardRule::MatchType;

std::vector<MatchType> HadamardRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<size_t, size_t> id2idx;

    size_t count = 0;
    for (auto const& v : graph.get_vertices()) {
        id2idx[v->get_id()] = count;
        count++;
    }
    // Find all H-boxes
    std::vector<bool> taken(graph.get_num_vertices(), false);
    std::vector<bool> in_matches(graph.get_num_vertices(), false);

    for (auto const& v : graph.get_vertices()) {
        if (v->get_type() == VertexType::h_box && v->get_num_neighbors() == 2) {
            NeighborPair nbp0 = v->get_first_neighbor();
            NeighborPair nbp1 = v->get_second_neighbor();
            size_t n0 = id2idx[nbp0.first->get_id()], n1 = id2idx[nbp1.first->get_id()];
            if (taken[n0] || taken[n1]) continue;
            if (!in_matches[n0] && !in_matches[n1]) {
                matches.emplace_back(v);
                in_matches[id2idx[v->get_id()]] = true;
                taken[n0] = true;
                taken[n1] = true;
            }
        }
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

        for (auto& [neighbor, edgeType] : v->get_neighbors()) {
            neighbor_vertices.emplace_back(neighbor);
            neighbor_edge_types.emplace_back(edgeType);
        }

        op.edgesToAdd.emplace_back(
            std::make_pair(neighbor_vertices[0], neighbor_vertices[1]),
            neighbor_edge_types[0] == neighbor_edge_types[1] ? EdgeType::hadamard : EdgeType::simple);
        // TODO: Correct for the sqrt(2) difference in H-boxes and H-edges
    }

    _update(graph, op);
}
