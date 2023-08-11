/****************************************************************************
  FileName     [ localComplementRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Local Complementary Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <unordered_set>
#include <utility>

#include "./zxRulesTemplate.hpp"

extern size_t verbose;

using MatchType = LocalComplementRule::MatchType;

/**
 * @brief Find noninteracting matchings of the local complementation rule.
 *
 * @param graph The graph to find matches in.
 */
std::vector<MatchType> LocalComplementRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;

    for (const auto& v : graph.getVertices()) {
        if (v->getType() == VertexType::Z && (v->getPhase() == Phase(1, 2) || v->getPhase() == Phase(3, 2))) {
            bool matchCondition = true;
            if (taken.contains(v)) continue;

            for (const auto& [nb, etype] : v->getNeighbors()) {
                if (etype != EdgeType::HADAMARD || nb->getType() != VertexType::Z || taken.contains(nb)) {
                    matchCondition = false;
                    break;
                }
            }
            if (matchCondition) {
                std::vector<ZXVertex*> neighbors;
                for (const auto& [nb, _] : v->getNeighbors()) {
                    if (v == nb) continue;
                    neighbors.emplace_back(nb);
                    taken.insert(nb);
                }
                taken.insert(v);
                matches.emplace_back(make_pair(v, neighbors));
            }
        }
    }

    return matches;
}

void LocalComplementRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    for (const auto& [v, neighbors] : matches) {
        op.verticesToRemove.emplace_back(v);
        size_t hEdgeCount = 0;
        for (auto& [nb, etype] : v->getNeighbors()) {
            if (nb == v && etype == EdgeType::HADAMARD) {
                hEdgeCount++;
            }
        }
        Phase p = v->getPhase() + Phase(hEdgeCount / 2);
        // TODO: global scalar ignored
        for (size_t n = 0; n < neighbors.size(); n++) {
            neighbors[n]->setPhase(neighbors[n]->getPhase() - p);
            for (size_t j = n + 1; j < neighbors.size(); j++) {
                op.edgesToAdd.emplace_back(std::make_pair(neighbors[n], neighbors[j]), EdgeType::HADAMARD);
            }
        }
    }

    update(graph, op);
}
