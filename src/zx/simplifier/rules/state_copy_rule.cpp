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
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> StateCopyRule::find_matches(
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

        if (v->get_type() != VertexType::z) {
            continue;
        }
        if (v->get_phase() != Phase(0) && v->get_phase() != Phase(1)) {
            continue;
        }
        if (graph.get_num_neighbors(v) != 1) {
            continue;
        }

        ZXVertex* pi_neighbor = graph.get_first_neighbor(v).first;
        if (pi_neighbor->get_type() != VertexType::z) {
            continue;
        }
        std::vector<ZXVertex*> apply_neighbors;
        for (auto const& [nb, _] : graph.get_neighbors(pi_neighbor)) {
            if (nb != v)
                apply_neighbors.emplace_back(nb);
            if (!allow_overlapping_candidates) {
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
            if (neighbor->get_type() == VertexType::boundary) {
                ZXVertex* const new_v     = graph.add_vertex(VertexType::z, npi->get_phase(), neighbor->get_row(), (neighbor->get_col() + a->get_col()) / 2);
                auto const is_simple_edge = graph.get_first_neighbor(neighbor).second == EdgeType::simple;

                op.edges_to_remove.emplace_back(std::make_pair(a, neighbor), graph.get_first_neighbor(neighbor).second);

                // new to Boundary
                op.edges_to_add.emplace_back(std::make_pair(new_v, neighbor), is_simple_edge ? EdgeType::hadamard : EdgeType::simple);

                // a to new
                op.edges_to_add.emplace_back(std::make_pair(a, new_v), EdgeType::hadamard);

            } else {
                neighbor->set_phase(npi->get_phase() + neighbor->get_phase());
            }
        }
    }

    _update(graph, op);
}
