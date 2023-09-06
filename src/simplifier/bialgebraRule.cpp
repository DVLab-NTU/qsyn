/****************************************************************************
  FileName     [ bialgebraRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <utility>

#include "./zxRulesTemplate.hpp"
#include "zx/zxGraph.hpp"

using MatchType = BialgebraRule::MatchType;

/**
 * @brief Check if the vexter of vertices has duplicate
 *
 * @param vec
 * @return true if found
 */
bool BialgebraRule::has_dupicate(std::vector<ZXVertex*> vec) const {
    std::vector<int> appeared = {};
    for (size_t i = 0; i < vec.size(); i++) {
        if (find(appeared.begin(), appeared.end(), vec[i]->getId()) == appeared.end()) {
            appeared.emplace_back(vec[i]->getId());
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
std::vector<MatchType> BialgebraRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<size_t, size_t> id2idx;
    size_t cnt = 0;
    for (const auto& v : graph.getVertices()) {
        id2idx[v->getId()] = cnt;
        cnt++;
    }

    // FIXME - performance: replace unordered_map<size_t, size_t> id2idx; and vector<bool> taken(graph.getNumVertices(), false); with a single
    //  unordered_set<ZXVertex*> taken; Not fixing at the moment as bialg is not used in full reduce
    std::vector<bool> taken(graph.getNumVertices(), false);
    graph.forEachEdge([&id2idx, &taken, &matches, this](const EdgePair& epair) {
        if (epair.second != EdgeType::SIMPLE) return;
        auto [left, right] = std::get<0>(epair);

        size_t n0 = id2idx[left->getId()], n1 = id2idx[right->getId()];

        // Verify if the vertices are taken
        if (taken[n0] || taken[n1]) return;

        // Do not consider the phase spider yet
        // todo: consider the phase
        if ((left->getPhase() != Phase(0)) || (right->getPhase() != Phase(0))) return;

        // Verify if the edge is connected by a X and a Z spider.
        if (!((left->getType() == VertexType::X && right->getType() == VertexType::Z) || (left->getType() == VertexType::Z && right->getType() == VertexType::X))) return;

        // Check if the vertices is_ground (with only one edge).
        if ((left->getNumNeighbors() == 1) || (right->getNumNeighbors() == 1)) return;

        std::vector<ZXVertex*> neighbor_of_left = left->getCopiedNeighbors(), neighbor_of_right = right->getCopiedNeighbors();

        // Check if a vertex has a same neighbor, in other words, two or more edges to another vertex.
        if (has_dupicate(neighbor_of_left) || has_dupicate(neighbor_of_right)) return;

        // Check if all neighbors of z are x without phase and all neighbors of x are z without phase.
        if (!all_of(neighbor_of_left.begin(), neighbor_of_left.end(), [right = right](ZXVertex* v) { return (v->getPhase() == Phase(0) && v->getType() == right->getType()); })) {
            return;
        }
        if (!all_of(neighbor_of_right.begin(), neighbor_of_right.end(), [left = left](ZXVertex* v) { return (v->getPhase() == Phase(0) && v->getType() == left->getType()); })) {
            return;
        }

        // Check if all the edges are SIMPLE
        // TODO: Make H edge aware too.
        if (!all_of(left->getNeighbors().begin(), left->getNeighbors().end(), [](std::pair<ZXVertex*, EdgeType> edge_pair) { return edge_pair.second == EdgeType::SIMPLE; })) {
            return;
        }
        if (!all_of(right->getNeighbors().begin(), right->getNeighbors().end(), [](std::pair<ZXVertex*, EdgeType> edge_pair) { return edge_pair.second == EdgeType::SIMPLE; })) {
            return;
        }

        matches.emplace_back(epair);

        // set left, right and their neighbors into taken
        for (size_t j = 0; j < neighbor_of_left.size(); j++) {
            taken[id2idx[neighbor_of_left[j]->getId()]] = true;
        }
        for (size_t j = 0; j < neighbor_of_right.size(); j++) {
            taken[id2idx[neighbor_of_right[j]->getId()]] = true;
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
void BialgebraRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    for (const auto& match : matches) {
        auto [left, right] = std::get<0>(match);

        std::vector<ZXVertex*> neighbor_of_left = left->getCopiedNeighbors();
        std::vector<ZXVertex*> neighbor_of_right = right->getCopiedNeighbors();

        op.verticesToRemove.emplace_back(left);
        op.verticesToRemove.emplace_back(right);

        for (const auto& neighbor_left : neighbor_of_left) {
            if (neighbor_left == right) continue;
            for (const auto& neighbor_right : neighbor_of_right) {
                if (neighbor_right == left) continue;
                op.edgesToAdd.emplace_back(std::make_pair(neighbor_left, neighbor_right), EdgeType::SIMPLE);
            }
        }
    }

    update(graph, op);
}
