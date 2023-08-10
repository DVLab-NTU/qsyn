/****************************************************************************
  FileName     [ pivot.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRulesTemplate.hpp"

using MatchType = PivotRule::MatchType;

extern size_t verbose;

/**
 * @brief Finds matchings of the pivot rule.
 *
 * @param grapg The graph to find matches
 */
std::vector<MatchType> PivotRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;
    graph.forEachEdge([&taken, &matches, this](const EdgePair& epair) {
        if (epair.second != EdgeType::HADAMARD) return;

        // 2: Get Neighbors
        auto [vs, vt] = epair.first;

        if (taken.contains(vs) || taken.contains(vt)) return;
        if (!vs->isZ() || !vt->isZ()) return;

        // 3: Check Neighbors Phase
        bool vsIsNPi = vs->hasNPiPhase();
        bool vtIsNPi = vt->hasNPiPhase();

        if (!vsIsNPi || !vtIsNPi) return;

        // 4: Check neighbors of Neighbors

        bool foundOne = false;
        for (auto& v : {vs, vt}) {
            for (auto& [nb, et] : v->getNeighbors()) {
                if (nb->isZ() && et == EdgeType::HADAMARD) continue;
                if (nb->isBoundary()) {
                    if (foundOne) return;
                    foundOne = true;
                } else {
                    taken.insert(nb);
                    return;
                }
            }
        }

        // 5: taken
        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->getNeighbors()) taken.insert(v);
        for (auto& [v, _] : vt->getNeighbors()) taken.insert(v);

        // 6: add Epair into _matchTypeVec
        matches.emplace_back(vs, vt);
    });

    return matches;
}

void PivotRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    for (const auto& [vs, vt] : matches) {
        for (auto& v : {vs, vt}) {
            for (auto& [nb, et] : v->getNeighbors()) {
                if (nb->isZ() && et == EdgeType::HADAMARD) continue;
                if (nb->isBoundary()) {
                    graph.addBuffer(nb, v, et);
                    goto next_pair;
                }
            }
        }
    next_pair:
        continue;
    }

    PivotRuleInterface::apply(graph, matches);
}
