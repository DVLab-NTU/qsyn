/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ State Copy Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <unordered_set>

#include "./zx_rules_template.hpp"
#include "tl/enumerate.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = StateCopyRule::MatchType;

/**
 * @brief Find matchings of the spider rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @return std::vector<MatchType>
 */
std::vector<MatchType> StateCopyRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    for (auto const& v : graph.get_vertices()) {
        if (!candidates->contains(v)) continue;

        if (!v->is_z()) {
            continue;
        }
        if (v->phase() != Phase(0) && v->phase() != Phase(1)) {
            continue;
        }
        if (graph.num_neighbors(v) != 1) {
            continue;
        }

        ZXVertex* pi_neighbor = graph.get_first_neighbor(v).first;
        if (!pi_neighbor->is_z()) {
            continue;
        }
        std::vector<ZXVertex*> apply_neighbors;
        for (auto const& [nb, _] : graph.get_neighbors(pi_neighbor)) {
            if (nb != v)
                apply_neighbors.emplace_back(nb);
            if (!_allow_overlapping_candidates) {
                candidates->erase(nb);
            }
        }
        matches.emplace_back(v, pi_neighbor, apply_neighbors);
    }

    return matches;
}

void StateCopyRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    // Need to update global scalar and phase
    for (auto const& [npi, a, neighbors] : matches) {
        op.vertices_to_remove.emplace_back(npi);
        op.vertices_to_remove.emplace_back(a);
        for (auto neighbor : neighbors) {
            if (neighbor->is_boundary()) {
                ZXVertex* const new_v     = graph.add_vertex(VertexType::z, npi->phase(), neighbor->get_row(), (neighbor->get_col() + a->get_col()) / 2);
                auto const is_simple_edge = graph.get_first_neighbor(neighbor).second == EdgeType::simple;

                op.edges_to_remove.emplace_back(std::make_pair(a, neighbor), graph.get_first_neighbor(neighbor).second);

                // new to Boundary
                op.edges_to_add.emplace_back(std::make_pair(new_v, neighbor), is_simple_edge ? EdgeType::hadamard : EdgeType::simple);

                // a to new
                op.edges_to_add.emplace_back(std::make_pair(a, new_v), EdgeType::hadamard);

            } else {
                neighbor->phase() += npi->phase();
            }
        }
    }

    _update(graph, op);
}
