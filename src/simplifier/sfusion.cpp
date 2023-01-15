/****************************************************************************
  FileName     [ sfusion.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Spider Fusion Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "zxGraph.h"
#include "zxRules.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Find non-interacting matchings of the spider fusion rule.
 *
 * @param g
 */
void SpiderFusion::match(ZXGraph* g) {
    _matchTypeVec.clear();

    unordered_set<ZXVertex*> taken;

    g->forEachEdge([&taken, this](const EdgePair& epair) {
        if (epair.second != EdgeType::SIMPLE) return;
        ZXVertex* v0 = epair.first.first;
        ZXVertex* v1 = epair.first.second;  // to be merged to v0

        if (taken.contains(v0) || taken.contains(v1)) return;

        if ((v0->getType() == v1->getType()) && (v0->isX() || v0->isZ())) {
            taken.insert(v0);
            taken.insert(v1);
            const Neighbors& v1n = v1->getNeighbors();
            // NOTE - Cannot choose the vertex connected to the vertices that will be merged
            for (auto& [nb, etype] : v1n) {
                taken.insert(nb);
            }
            _matchTypeVec.push_back(make_pair(v0, v1));
        }
    });

    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *
 * @param g
 */
void SpiderFusion::rewrite(ZXGraph* g) {
    reset();

    for (size_t i = 0; i < _matchTypeVec.size(); i++) {
        ZXVertex* v0 = _matchTypeVec[i].first;
        ZXVertex* v1 = _matchTypeVec[i].second;

        v0->setPhase(v0->getPhase() + v1->getPhase());
        Neighbors v1n = v1->getNeighbors();

        for (auto& nbp : v1n) {
            // NOTE - Will become selfloop after merged, only considered hadamard
            if (nbp.first == v0) {
                if (nbp.second == EdgeType::HADAMARD)
                    v0->setPhase(v0->getPhase() + Phase(1));
                // NOTE - No need to remove edges since v1 will be removed
            } else {
                _edgeTableKeys.push_back(make_pair(v0, nbp.first));
                _edgeTableValues.push_back(nbp.second == EdgeType::SIMPLE ? make_pair(1, 0) : make_pair(0, 1));
            }
        }
        _removeVertices.push_back(v1);
    }
}
