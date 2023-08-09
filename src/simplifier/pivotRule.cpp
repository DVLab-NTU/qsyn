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
 * @brief Preprocess the matches so that it conforms with the rewrite functions
 *
 * @param graph The graph to be preprocessed
 */
void PivotRule::preprocess(ZXGraph& graph) const {
    for (auto& v : _boundaries) {
        auto& [nb, etype] = v->getFirstNeighbor();
        graph.addBuffer(v, nb, etype);
    }
}

/**
 * @brief Finds matchings of the pivot rule.
 *
 * @param grapg The graph to find matches
 */
std::vector<MatchType> PivotRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;
    _boundaries.clear();

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

        std::vector<ZXVertex*> boundaries;
        for (auto& v : {vs, vt}) {
            for (auto& [nb, et] : v->getNeighbors()) {
                if (nb->isZ() && et == EdgeType::HADAMARD) continue;
                if (nb->isBoundary()) {
                    boundaries.emplace_back(nb);
                } else {
                    taken.insert(nb);
                    return;
                }
            }
        }
        if (boundaries.size() > 1) return;

        // 5: taken
        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->getNeighbors()) taken.insert(v);
        for (auto& [v, _] : vt->getNeighbors()) taken.insert(v);

        // 6: add Epair into _matchTypeVec
        matches.push_back({vs, vt});  // NOTE: cannot emplace_back -- std::array does not have a constructor!;
        this->_boundaries.insert(this->_boundaries.end(), boundaries.begin(), boundaries.end());
    });

    return matches;
}
