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

    // Matches Hadamard-edges that are connected to H-boxes
    std::unordered_set<ZXVertex*> taken;

    graph.for_each_edge([&graph, &matches, &taken](EdgePair const& epair) {
        // NOTE - Only Hadamard Edges
        if (epair.second != EdgeType::hadamard) return;
        auto [neighbor_left, neighbor_right] = epair.first;

        if ((taken.contains(neighbor_left) && neighbor_left->is_hbox()) || (taken.contains(neighbor_right) && neighbor_right->is_hbox())) return;

        if (neighbor_left->is_hbox()) {
            matches.emplace_back(neighbor_left);
            taken.insert(neighbor_left);
            taken.insert(neighbor_right);

            auto nb0 = graph.get_first_neighbor(neighbor_left).first;
            auto nb1 = graph.get_second_neighbor(neighbor_left).first;

            if (neighbor_left != nb0) {
                taken.insert(nb0);
            } else {
                taken.insert(nb1);
            }
        } else if (neighbor_right->is_hbox()) {
            matches.emplace_back(neighbor_right);
            taken.insert(neighbor_left);
            taken.insert(neighbor_right);

            auto nb0 = graph.get_first_neighbor(neighbor_right).first;
            auto nb1 = graph.get_second_neighbor(neighbor_right).first;

            if (nb0 != neighbor_right) {
                taken.insert(nb0);
            } else {
                taken.insert(nb1);
            }
        }
    });

    graph.for_each_edge([&taken, &matches](EdgePair const& epair) {
        if (epair.second == EdgeType::hadamard) return;

        auto [neighbor_left, neighbor_right] = epair.first;

        if (!taken.contains(neighbor_left) && !taken.contains(neighbor_right)) {
            if (neighbor_left->is_hbox() && neighbor_right->is_hbox()) {
                matches.emplace_back(neighbor_left);
                matches.emplace_back(neighbor_right);
                taken.insert(neighbor_left);
                taken.insert(neighbor_right);
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

        for (auto& itr : graph.get_neighbors(matches[i])) {
            ns.emplace_back(itr.first);
            ets.emplace_back(itr.second);
        }

        op.edges_to_add.emplace_back(std::make_pair(ns[0], ns[1]), ets[0] == ets[1] ? EdgeType::hadamard : EdgeType::simple);
        // TODO: Correct for the sqrt(2) difference in H-boxes and H-edges
    }

    ZXRuleTemplate::_update(graph, op);
}
