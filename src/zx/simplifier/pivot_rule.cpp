/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = PivotRule::MatchType;

/**
 * @brief Finds matchings of the pivot rule.
 *
 * @param grapg The graph to find matches
 */
std::vector<MatchType> PivotRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;
    graph.for_each_edge([&graph, &taken, &matches](EdgePair const& epair) {
        if (epair.second != EdgeType::hadamard) return;

        // 2: Get Neighbors
        auto [vs, vt] = epair.first;

        if (taken.contains(vs) || taken.contains(vt)) return;
        if (!vs->is_z() || !vt->is_z()) return;

        // 3: Check Neighbors Phase
        bool vs_is_n_pi = vs->has_n_pi_phase();
        bool vt_is_n_pi = vt->has_n_pi_phase();

        if (!vs_is_n_pi || !vt_is_n_pi) return;

        // 4: Check neighbors of Neighbors

        bool found_one = false;
        for (auto& v : {vs, vt}) {
            for (auto& [nb, et] : graph.get_neighbors(v)) {
                if (nb->is_z() && et == EdgeType::hadamard) continue;
                if (nb->is_boundary()) {
                    if (found_one) return;
                    found_one = true;
                } else {
                    taken.insert(nb);
                    return;
                }
            }
        }

        // 5: taken
        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : graph.get_neighbors(vs)) taken.insert(v);
        for (auto& [v, _] : graph.get_neighbors(vt)) taken.insert(v);

        // 6: add Epair into _matchTypeVec
        matches.emplace_back(vs, vt);
    });

    return matches;
}

void PivotRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    for (auto const& [vs, vt] : matches) {
        for (auto& v : {vs, vt}) {
            for (auto& [nb, et] : graph.get_neighbors(v)) {
                if (nb->is_z() && et == EdgeType::hadamard) continue;
                if (nb->is_boundary()) {
                    graph.add_buffer(nb, v, et);
                    goto next_pair;
                }
            }
        }
    next_pair:
        continue;
    }

    PivotRuleInterface::apply(graph, matches);
}
