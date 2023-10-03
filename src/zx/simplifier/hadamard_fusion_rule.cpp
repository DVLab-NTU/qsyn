/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Cancellation Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = HadamardFusionRule::MatchType;

std::vector<MatchType> HadamardFusionRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<size_t, size_t> id2idx;
    size_t count = 0;
    for (auto const& v : graph.get_vertices()) {
        id2idx[v->get_id()] = count;
        count++;
    }

    // Matches Hadamard-edges that are connected to H-boxes
    std::vector<bool> taken(graph.get_num_vertices(), false);

    graph.for_each_edge([&id2idx, &taken, &matches, this](EdgePair const& epair) {
        // NOTE - Only Hadamard Edges
        if (epair.second != EdgeType::hadamard) return;
        auto [neighbor_left, neighbor_right] = epair.first;
        size_t n0 = id2idx[neighbor_left->get_id()], n1 = id2idx[neighbor_right->get_id()];

        if ((taken[n0] && neighbor_left->get_type() == VertexType::h_box) || (taken[n1] && neighbor_right->get_type() == VertexType::h_box)) return;

        if (neighbor_left->get_type() == VertexType::h_box) {
            matches.emplace_back(neighbor_left);
            taken[n0] = true;
            taken[n1] = true;

            Neighbors nebs    = neighbor_left->get_neighbors();
            NeighborPair nbp0 = *(nebs.begin());
            NeighborPair nbp1 = *next(nebs.begin());
            size_t n2 = id2idx[nbp0.first->get_id()], n3 = id2idx[nbp1.first->get_id()];

            if (n2 != n0)
                taken[n2] = true;
            else
                taken[n3] = true;
        } else if (neighbor_right->get_type() == VertexType::h_box) {
            matches.emplace_back(neighbor_right);
            taken[n0] = true;
            taken[n1] = true;

            Neighbors nebs    = neighbor_left->get_neighbors();
            NeighborPair nbp0 = *(nebs.begin());
            NeighborPair nbp1 = *next(nebs.begin());
            size_t n2 = id2idx[nbp0.first->get_id()], n3 = id2idx[nbp1.first->get_id()];

            if (n2 != n0)
                taken[n2] = true;
            else
                taken[n3] = true;
        } else if (neighbor_left->get_type() != VertexType::h_box || neighbor_right->get_type() != VertexType::h_box) {
            return;
        }
    });

    graph.for_each_edge([&id2idx, &taken, &matches, this](EdgePair const& epair) {
        if (epair.second == EdgeType::hadamard) return;

        ZXVertex* neighbor_left  = epair.first.first;
        ZXVertex* neighbor_right = epair.first.second;
        size_t n0 = id2idx[neighbor_left->get_id()], n1 = id2idx[neighbor_right->get_id()];

        if (!taken[n0] && !taken[n1]) {
            if (neighbor_left->get_type() == VertexType::h_box && neighbor_right->get_type() == VertexType::h_box) {
                matches.emplace_back(neighbor_left);
                matches.emplace_back(neighbor_right);
                taken[n0] = true;
                taken[n1] = true;
            }
        }
    });

    return matches;
}

void HadamardFusionRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op = {
        .vertices_to_remove = matches,
    };

    for (size_t i = 0; i < matches.size(); i++) {
        // NOTE: Only two neighbors which is ensured
        std::vector<ZXVertex*> ns;
        std::vector<EdgeType> ets;

        for (auto& itr : matches[i]->get_neighbors()) {
            ns.emplace_back(itr.first);
            ets.emplace_back(itr.second);
        }

        op.edges_to_add.emplace_back(std::make_pair(ns[0], ns[1]), ets[0] == ets[1] ? EdgeType::hadamard : EdgeType::simple);
        // TODO: Correct for the sqrt(2) difference in H-boxes and H-edges
    }

    ZXRuleTemplate::_update(graph, op);
}
