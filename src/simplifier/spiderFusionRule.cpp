/****************************************************************************
  FileName     [ sfusion.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Spider Fusion Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRulesTemplate.hpp"

using MatchType = SpiderFusionRule::MatchType;

extern size_t verbose;

/**
 * @brief Find non-interacting matchings of the spider fusion rule.
 *
 * @param graph The graph to find matches.
 */
std::vector<MatchType> SpiderFusionRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> _matchTypeVec;

    std::unordered_set<ZXVertex*> taken;

    graph.forEachEdge([&taken, &_matchTypeVec](const EdgePair& epair) {
        if (epair.second != EdgeType::SIMPLE) return;
        ZXVertex* v0 = epair.first.first;
        ZXVertex* v1 = epair.first.second;  // to be merged to v0

        if (taken.contains(v0) || taken.contains(v1)) return;

        if ((v0->getType() == v1->getType()) && (v0->isX() || v0->isZ())) {
            taken.insert(v0);
            taken.insert(v1);
            const Neighbors& v1n = v1->getNeighbors();
            // NOTE: Cannot choose the vertex connected to the vertices that will be merged
            for (auto& [nb, etype] : v1n) {
                taken.insert(nb);
            }
            _matchTypeVec.emplace_back(v0, v1);
        }
    });

    return _matchTypeVec;
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *
 * @param g
 */
void SpiderFusionRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    for (auto [v0, v1] : matches) {
        v0->setPhase(v0->getPhase() + v1->getPhase());
        Neighbors v1_neighbors = v1->getNeighbors();

        for (auto& [neighbor, edgeType] : v1_neighbors) {
            // NOTE: Will become selfloop after merged, only considered hadamard
            if (neighbor == v0) {
                if (edgeType == EdgeType::HADAMARD) {
                    v0->setPhase(v0->getPhase() + Phase(1));
                }
                // NOTE: No need to remove edges since v1 will be removed
            } else {
                op.edgesToAdd.emplace_back(std::make_pair(v0, neighbor), edgeType);
            }
        }
        op.verticesToRemove.emplace_back(v1);
    }

    update(graph, op);
}
