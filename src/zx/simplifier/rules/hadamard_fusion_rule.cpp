/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Cancellation Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = HadamardFusionRule::MatchType;

/**
 * @brief Find matchings of the Hadamard fusion rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @return std::vector<MatchType>
 */
std::vector<MatchType> HadamardFusionRule::find_matches(
    ZXGraph const& graph, std::optional<ZXVertexList> candidates) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    auto const match_hadamard_edge = [&](EdgePair const& epair) {
        assert(epair.second == EdgeType::hadamard);

        auto const [neighbor_left, neighbor_right] = epair.first;

        if ((!candidates->contains(neighbor_left) && neighbor_left->is_hbox()) || (!candidates->contains(neighbor_right) && neighbor_right->is_hbox())) return;

        if (neighbor_left->is_hbox()) {
            matches.emplace_back(neighbor_left);

            if (_allow_overlapping_candidates) return;

            candidates->erase(neighbor_left);
            candidates->erase(neighbor_right);

            auto nb0 = graph.get_first_neighbor(neighbor_left).first;
            auto nb1 = graph.get_second_neighbor(neighbor_left).first;

            if (neighbor_left != nb0) {
                candidates->erase(nb0);
            } else {
                candidates->erase(nb1);
            }
        } else if (neighbor_right->is_hbox()) {
            matches.emplace_back(neighbor_right);

            if (_allow_overlapping_candidates) return;

            candidates->erase(neighbor_left);
            candidates->erase(neighbor_right);

            auto nb0 = graph.get_first_neighbor(neighbor_right).first;
            auto nb1 = graph.get_second_neighbor(neighbor_right).first;

            if (nb0 != neighbor_right) {
                candidates->erase(nb0);
            } else {
                candidates->erase(nb1);
            }
        }
    };

    auto const match_simple_edge = [&](EdgePair const& epair) {
        assert(epair.second == EdgeType::simple);

        auto const [neighbor_left, neighbor_right] = epair.first;

        if (!candidates->contains(neighbor_left) || !candidates->contains(neighbor_right)) return;

        if (neighbor_left->is_hbox() && neighbor_right->is_hbox()) {
            matches.emplace_back(neighbor_left);
            matches.emplace_back(neighbor_right);

            if (_allow_overlapping_candidates) return;

            candidates->erase(neighbor_left);
            candidates->erase(neighbor_right);
        }
    };

    graph.for_each_edge([&](EdgePair const& epair) {
        switch (epair.second) {
            case EdgeType::hadamard:
                match_hadamard_edge(epair);
                break;
            case EdgeType::simple:
                match_simple_edge(epair);
                break;
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
