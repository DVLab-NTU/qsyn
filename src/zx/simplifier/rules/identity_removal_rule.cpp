/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;

using MatchType = IdentityRemovalRule::MatchType;

/**
 * @brief Find matchings of the identity removal rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates.
 *        If true, return all candidates that match the rule;
 *        otherwise, only returns non-overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> IdentityRemovalRule::find_matches(
    ZXGraph const& graph, std::optional<ZXVertexList> candidates,
    bool allow_overlapping_candidates  //
) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    for (auto const& v : graph.get_vertices()) {
        if (!candidates->contains(v)) continue;

        if (v->phase() != Phase(0)) continue;
        if (!v->is_zx()) continue;
        if (graph.num_neighbors(v) != 2) continue;

        matches.emplace_back(v->get_id());

        if (allow_overlapping_candidates) continue;

        candidates->erase(v);
        candidates->erase(graph.get_first_neighbor(v).first);
        candidates->erase(graph.get_second_neighbor(v).first);
    }

    return matches;
}
