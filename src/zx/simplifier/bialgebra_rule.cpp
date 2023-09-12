/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <utility>

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = BialgebraRule::MatchType;

/**
 * @brief Check if the vexter of vertices has duplicate
 *
 * @param vec
 * @return true if found
 */
bool BialgebraRule::_has_dupicate(std::vector<ZXVertex*> vec) const {
    std::vector<int> appeared = {};
    for (size_t i = 0; i < vec.size(); i++) {
        if (find(appeared.begin(), appeared.end(), vec[i]->get_id()) == appeared.end()) {
            appeared.emplace_back(vec[i]->get_id());
        } else {
            return true;
        }
    }
    return false;
}

/**
 * @brief Find noninteracting matchings of the bialgebra rule.
 *        (Check PyZX/pyzx/rules.py/match_bialg_parallel for more details)
 *
 * @param g
 */
std::vector<MatchType> BialgebraRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<size_t, size_t> id2idx;
    size_t cnt = 0;
    for (auto const& v : graph.get_vertices()) {
        id2idx[v->get_id()] = cnt;
        cnt++;
    }

    // FIXME - performance: replace unordered_map<size_t, size_t> id2idx; and vector<bool> taken(graph.getNumVertices(), false); with a single
    //  unordered_set<ZXVertex*> taken; Not fixing at the moment as bialg is not used in full reduce
    std::vector<bool> taken(graph.get_num_vertices(), false);
    graph.for_each_edge([&id2idx, &taken, &matches, this](EdgePair const& epair) {
        if (epair.second != EdgeType::simple) return;
        auto [left, right] = std::get<0>(epair);

        size_t n0 = id2idx[left->get_id()], n1 = id2idx[right->get_id()];

        // Verify if the vertices are taken
        if (taken[n0] || taken[n1]) return;

        // Do not consider the phase spider yet
        // todo: consider the phase
        if ((left->get_phase() != Phase(0)) || (right->get_phase() != Phase(0))) return;

        // Verify if the edge is connected by a X and a Z spider.
        if (!((left->get_type() == VertexType::x && right->get_type() == VertexType::z) || (left->get_type() == VertexType::z && right->get_type() == VertexType::x))) return;

        // Check if the vertices is_ground (with only one edge).
        if ((left->get_num_neighbors() == 1) || (right->get_num_neighbors() == 1)) return;

        std::vector<ZXVertex*> neighbor_of_left = left->get_copied_neighbors(), neighbor_of_right = right->get_copied_neighbors();

        // Check if a vertex has a same neighbor, in other words, two or more edges to another vertex.
        if (_has_dupicate(neighbor_of_left) || _has_dupicate(neighbor_of_right)) return;

        // Check if all neighbors of z are x without phase and all neighbors of x are z without phase.
        if (!all_of(neighbor_of_left.begin(), neighbor_of_left.end(), [right = right](ZXVertex* v) { return (v->get_phase() == Phase(0) && v->get_type() == right->get_type()); })) {
            return;
        }
        if (!all_of(neighbor_of_right.begin(), neighbor_of_right.end(), [left = left](ZXVertex* v) { return (v->get_phase() == Phase(0) && v->get_type() == left->get_type()); })) {
            return;
        }

        // Check if all the edges are SIMPLE
        // TODO: Make H edge aware too.
        if (!all_of(left->get_neighbors().begin(), left->get_neighbors().end(), [](std::pair<ZXVertex*, EdgeType> edge_pair) { return edge_pair.second == EdgeType::simple; })) {
            return;
        }
        if (!all_of(right->get_neighbors().begin(), right->get_neighbors().end(), [](std::pair<ZXVertex*, EdgeType> edge_pair) { return edge_pair.second == EdgeType::simple; })) {
            return;
        }

        matches.emplace_back(epair);

        // set left, right and their neighbors into taken
        for (size_t j = 0; j < neighbor_of_left.size(); j++) {
            taken[id2idx[neighbor_of_left[j]->get_id()]] = true;
        }
        for (size_t j = 0; j < neighbor_of_right.size(); j++) {
            taken[id2idx[neighbor_of_right[j]->get_id()]] = true;
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

        std::vector<ZXVertex*> neighbor_of_left = left->get_copied_neighbors();
        std::vector<ZXVertex*> neighbor_of_right = right->get_copied_neighbors();

        op.vertices_to_remove.emplace_back(left);
        op.vertices_to_remove.emplace_back(right);

        for (auto const& neighbor_left : neighbor_of_left) {
            if (neighbor_left == right) continue;
            for (auto const& neighbor_right : neighbor_of_right) {
                if (neighbor_right == left) continue;
                op.edgesToAdd.emplace_back(std::make_pair(neighbor_left, neighbor_right), EdgeType::simple);
            }
        }
    }

    _update(graph, op);
}
