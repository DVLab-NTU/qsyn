/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;

using MatchType = PivotRule::MatchType;

/**
 * @brief Find matchings of the pivot rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> PivotRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates,
    bool allow_overlapping_candidates  //
) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    graph.for_each_edge([&](EdgePair const& epair) {
        if (epair.second != EdgeType::hadamard) return;

        // 2: Get Neighbors
        auto [vs, vt] = epair.first;

        if (!candidates->contains(vs) || !candidates->contains(vt)) return;
        if (!vs->is_z() || !vt->is_z()) return;

        // 3: Check Neighbors Phase
        if (!vs->has_n_pi_phase() || !vt->has_n_pi_phase()) return;

        // 4: Check neighbors of Neighbors

        bool found_one = false;
        for (auto& v : {vs, vt}) {
            for (auto& [nb, et] : graph.get_neighbors(v)) {
                if (nb->is_z() && et == EdgeType::hadamard) continue;
                if (nb->is_boundary()) {
                    if (found_one) return;
                    found_one = true;
                } else {
                    return;
                }
            }
        }
        matches.emplace_back(vs->get_id(), vt->get_id());

        if (allow_overlapping_candidates) return;

        candidates->erase(vs);
        candidates->erase(vt);

        for (auto& [v, _] : graph.get_neighbors(vs)) candidates->erase(v);
        for (auto& [v, _] : graph.get_neighbors(vt)) candidates->erase(v);
    });

    return matches;
}
