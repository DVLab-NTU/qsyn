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
 * @brief Find matchings of the spider rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> SpiderFusionRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates,
    bool allow_overlapping_candidates  //
) const {
    std::vector<MatchType> match_type_vec;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    graph.for_each_edge([&](EdgePair const& epair) {
        if (epair.second != EdgeType::simple) return;
        ZXVertex* v0 = epair.first.first;
        ZXVertex* v1 = epair.first.second;  // to be merged to v0

        if (!candidates->contains(v0) || !candidates->contains(v1)) return;

        if ((v0->get_type() == v1->get_type()) && (v0->is_x() || v0->is_z())) {
            match_type_vec.emplace_back(v0, v1);
            if (allow_overlapping_candidates) return;
            candidates->erase(v0);
            candidates->erase(v1);
            // NOTE: Cannot choose the vertex connected to the vertices that will be merged
            for (auto& [nb, _] : graph.get_neighbors(v1)) {
                candidates->erase(nb);
            }
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

        for (auto& [neighbor, edgeType] : graph.get_neighbors(v1)) {
            if (neighbor == v0) continue;
            op.edges_to_add.emplace_back(std::make_pair(v0, neighbor), edgeType);
        }
        op.vertices_to_remove.emplace_back(v1);
    }

    _update(graph, op);
}
