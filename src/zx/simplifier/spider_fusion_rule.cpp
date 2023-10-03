/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Spider Fusion Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = SpiderFusionRule::MatchType;

/**
 * @brief Find non-interacting matchings of the spider fusion rule.
 *
 * @param graph The graph to find matches.
 */
std::vector<MatchType> SpiderFusionRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> match_type_vec;

    std::unordered_set<ZXVertex*> taken;

    graph.for_each_edge([&taken, &match_type_vec](EdgePair const& epair) {
        if (epair.second != EdgeType::simple) return;
        ZXVertex* v0 = epair.first.first;
        ZXVertex* v1 = epair.first.second;  // to be merged to v0

        if (taken.contains(v0) || taken.contains(v1)) return;

        if ((v0->get_type() == v1->get_type()) && (v0->is_x() || v0->is_z())) {
            taken.insert(v0);
            taken.insert(v1);
            const Neighbors& v1n = v1->get_neighbors();
            // NOTE: Cannot choose the vertex connected to the vertices that will be merged
            for (auto& [nb, etype] : v1n) {
                taken.insert(nb);
            }
            match_type_vec.emplace_back(v0, v1);
        }
    });

    return match_type_vec;
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *
 * @param g
 */
void SpiderFusionRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto [v0, v1] : matches) {
        v0->set_phase(v0->get_phase() + v1->get_phase());
        Neighbors v1_neighbors = v1->get_neighbors();

        for (auto& [neighbor, edgeType] : v1_neighbors) {
            // NOTE: Will become selfloop after merged, only considered hadamard
            if (neighbor == v0) {
                if (edgeType == EdgeType::hadamard) {
                    v0->set_phase(v0->get_phase() + Phase(1));
                }
                // NOTE: No need to remove edges since v1 will be removed
            } else {
                op.edges_to_add.emplace_back(std::make_pair(v0, neighbor), edgeType);
            }
        }
        op.vertices_to_remove.emplace_back(v1);
    }

    _update(graph, op);
}
