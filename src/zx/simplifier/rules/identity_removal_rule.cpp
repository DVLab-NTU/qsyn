/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = IdentityRemovalRule::MatchType;

/**
 * @brief Find matchings of the identity removal rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
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

        if (v->get_phase() != Phase(0)) continue;
        if (v->get_type() != VertexType::z && v->get_type() != VertexType::x) continue;
        if (graph.get_num_neighbors(v) != 2) continue;

        auto [n0, etype0] = graph.get_first_neighbor(v);
        auto [n1, etype1] = graph.get_second_neighbor(v);

        matches.emplace_back(v, n0, n1, zx::concat_edge(etype0, etype1));

        if (allow_overlapping_candidates) continue;

        candidates->erase(v);
        candidates->erase(n0);
        candidates->erase(n1);
    }

    return matches;
}

/**
 * @brief Apply the identity removal rule to the graph.
 *
 * @param graph The graph to be simplified.
 * @param matches The matches of the identity removal rule.
 */
void IdentityRemovalRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto const& [v, n0, n1, edge_type] : matches) {
        op.vertices_to_remove.emplace_back(v);
        if (n0 == n1) {
            n0->set_phase(n0->get_phase() + Phase(1));
            continue;
        }
        op.edges_to_add.emplace_back(std::make_pair(n0, n1), edge_type);
    }

    _update(graph, op);
}
