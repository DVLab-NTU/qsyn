/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Local Complementary Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <gsl/narrow>
#include <unordered_set>
#include <utility>

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = LocalComplementRule::MatchType;

/**
 * @brief Find matchings of the local complementation rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates.
 *        If true, needs to manually check for overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> LocalComplementRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates,
    bool allow_overlapping_candidates  //
) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    for (auto const& v : graph.get_vertices()) {
        if (!candidates->contains(v)) continue;
        if (!v->is_z()) continue;
        if (v->phase().denominator() != 2) continue;

        if (std::ranges::any_of(
                graph.get_neighbors(v),
                [&](auto const& epair) {
                    auto const& [nb, etype] = epair;
                    return etype != EdgeType::hadamard ||
                           !nb->is_z() ||
                           !candidates->contains(nb);
                })) {
            continue;
        }

        for (auto const& [nb, _] : graph.get_neighbors(v)) {
            if (!allow_overlapping_candidates) {
                candidates->erase(nb);
            }
        }

        matches.emplace_back(v->get_id());

        if (!allow_overlapping_candidates) {
            candidates->erase(v);
        }
    }

    return matches;
}
