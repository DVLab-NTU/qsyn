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
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
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
        if (v->get_phase() != Phase(1, 2) && v->get_phase() != Phase(3, 2)) continue;

        if (std::ranges::any_of(
                graph.get_neighbors(v),
                [&](auto const& epair) {
                    auto const& [nb, etype] = epair;
                    return etype != EdgeType::hadamard ||
                           nb->get_type() != VertexType::z ||
                           !candidates->contains(nb);
                })) {
            continue;
        }

        std::vector<ZXVertex*> neighbors;
        for (auto const& [nb, _] : graph.get_neighbors(v)) {
            if (v == nb) continue;
            neighbors.emplace_back(nb);
            if (!allow_overlapping_candidates) {
                candidates->erase(nb);
            }
        }

        matches.emplace_back(make_pair(v, neighbors));

        if (!allow_overlapping_candidates) {
            candidates->erase(v);
        }
    }

    return matches;
}

void LocalComplementRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto const& [v, neighbors] : matches) {
        op.vertices_to_remove.emplace_back(v);
        size_t h_edge_count = 0;
        for (auto& [nb, etype] : graph.get_neighbors(v)) {
            if (nb == v && etype == EdgeType::hadamard) {
                h_edge_count++;
            }
        }
        auto const p = v->get_phase() + Phase(gsl::narrow<int>(h_edge_count / 2));
        // TODO: global scalar ignored
        for (size_t n = 0; n < neighbors.size(); n++) {
            neighbors[n]->set_phase(neighbors[n]->get_phase() - p);
            for (size_t j = n + 1; j < neighbors.size(); j++) {
                op.edges_to_add.emplace_back(std::make_pair(neighbors[n], neighbors[j]), EdgeType::hadamard);
            }
        }
    }

    _update(graph, op);
}
