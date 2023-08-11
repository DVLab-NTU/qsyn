/****************************************************************************
  FileName     [ idRemovalRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zxRulesTemplate.hpp"
#include "zx/zxGraph.hpp"

using MatchType = IdRemovalRule::MatchType;

/**
 * @brief Find all the matches of the identity removal rule.
 *
 * @param g The graph to be simplified.
 */
std::vector<MatchType> IdRemovalRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;

    for (const auto& v : graph.getVertices()) {
        if (taken.contains(v)) continue;

        const Neighbors& neighbors = v->getNeighbors();
        if (v->getPhase() != Phase(0)) continue;
        if (v->getType() != VertexType::Z && v->getType() != VertexType::X) continue;
        if (neighbors.size() != 2) continue;

        auto [n0, edgeType0] = *(neighbors.begin());
        auto [n1, edgeType1] = *next(neighbors.begin());

        EdgeType edgeType = (edgeType0 == edgeType1) ? EdgeType::SIMPLE : EdgeType::HADAMARD;

        matches.emplace_back(v, n0, n1, edgeType);
        taken.insert(v);
        taken.insert(n0);
        taken.insert(n1);
    }

    return matches;
}

/**
 * @brief Apply the identity removal rule to the graph.
 *
 * @param graph The graph to be simplified.
 * @param matches The matches of the identity removal rule.
 */
void IdRemovalRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    for (const auto& match : matches) {
        ZXVertex* v = std::get<0>(match);
        ZXVertex* n0 = std::get<1>(match);
        ZXVertex* n1 = std::get<2>(match);
        EdgeType edgeType = std::get<3>(match);

        op.verticesToRemove.emplace_back(v);
        if (n0 == n1) {
            n0->setPhase(n0->getPhase() + Phase(1));
            continue;
        }
        op.edgesToAdd.emplace_back(std::make_pair(n0, n1), edgeType);
    }

    update(graph, op);
}
