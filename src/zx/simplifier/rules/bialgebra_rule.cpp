/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <unordered_map>
#include <utility>

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = BialgebraRule::MatchType;

/**
 * @brief Find noninteracting matchings of the bialgebra rule.
 *        (Check PyZX/pyzx/rules.py/match_bialg_parallel for more details)
 *
 * @param g
 */
std::vector<MatchType> BialgebraRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;
    graph.for_each_edge([&graph, &taken, &matches](EdgePair const& epair) {
        if (epair.second != EdgeType::simple) return;
        auto [left, right] = std::get<0>(epair);

        // Verify if the vertices are taken
        if (taken.contains(left) || taken.contains(right)) return;

        // Does not consider the phase spider yet
        // TODO: consider the phase
        if ((left->get_phase() != Phase(0)) || (right->get_phase() != Phase(0))) return;

        // Verify if the edge is connected by a X and a Z spider.
        if (!(left->is_x() && right->is_z()) && !(left->is_z() && right->is_x())) return;

        // Check if the vertices is_ground (with only one edge).
        if ((graph.get_num_neighbors(left) == 1) || (graph.get_num_neighbors(right) == 1)) return;

        auto const neighbor_edges_of_left  = graph.get_neighbors(left) | std::views::values;
        auto const neighbor_edges_of_right = graph.get_neighbors(right) | std::views::values;

        // Check if all the edges are SIMPLE
        // TODO: Make H edge aware too.
        if (std::ranges::any_of(neighbor_edges_of_left, [](EdgeType etype) { return etype != EdgeType::simple; })) {
            return;
        }
        if (std::ranges::any_of(neighbor_edges_of_right, [](EdgeType etype) { return etype != EdgeType::simple; })) {
            return;
        }

        // [2023.10.22] Lau, Mu-Te:
        // I've move the edge type check to before the neighbor vertices check.
        // Since we have checked that all edge types are the same, we know that there are no duplicate neighboring vertices
        // Therefore, I have removed the vertex duplication check.

        auto const neighbor_vertices_of_left  = graph.get_neighbors(left) | std::views::keys;
        auto const neighbor_vertices_of_right = graph.get_neighbors(right) | std::views::keys;

        // Check if all neighbors of z are x without phase and all neighbors of x are z without phase.
        if (std::ranges::any_of(neighbor_vertices_of_left, [type = right->get_type()](ZXVertex* v) { return v->get_phase() != Phase(0) || v->get_type() != type; })) {
            return;
        }
        if (std::ranges::any_of(neighbor_vertices_of_right, [type = left->get_type()](ZXVertex* v) { return v->get_phase() != Phase(0) || v->get_type() != type; })) {
            return;
        }

        matches.emplace_back(epair);

        // set left, right and their neighbors into taken
        for (auto const& v : neighbor_vertices_of_left) {
            taken.emplace(v);
        }
        for (auto const& v : neighbor_vertices_of_right) {
            taken.emplace(v);
        }
    });

    return matches;
}

/**
 * @brief Perform a certain type of bialgebra rewrite based on `matches`
 *        (Check PyZX/pyzx/rules.py/bialg for more details)
 *
 * @param g
 */
void BialgebraRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto const& match : matches) {
        auto [left, right] = std::get<0>(match);

        op.vertices_to_remove.emplace_back(left);
        op.vertices_to_remove.emplace_back(right);

        for (auto const& [neighbor_left, _] : graph.get_neighbors(left)) {
            if (neighbor_left == right) continue;
            for (auto const& [neighbor_right, _] : graph.get_neighbors(right)) {
                if (neighbor_right == left) continue;
                op.edges_to_add.emplace_back(std::make_pair(neighbor_left, neighbor_right), EdgeType::simple);
            }
        }
    }

    _update(graph, op);
}
