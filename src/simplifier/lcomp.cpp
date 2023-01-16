/****************************************************************************
  FileName     [ lcomp.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Local Complementary Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "zxGraph.h"
#include "zxRules.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Find noninteracting matchings of the local complementation rule.
 *
 * @param g
 */
void LComp::match(ZXGraph* g) {
    _matchTypeVec.clear();

    // Find all Z vertices that connect to all neighb ors with H edge.
    unordered_set<ZXVertex*> taken;

    for (const auto& v : g->getVertices()) {
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
                vector<ZXVertex*> neighbors;
                for (const auto& [nb, _] : v->getNeighbors()) {
                    if (v == nb) continue;
                    neighbors.push_back(nb);
                    taken.insert(nb);
                }
                taken.insert(v);
                _matchTypeVec.push_back(make_pair(v, neighbors));
            }
        }
    }
    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Remove `first` vertex in _matchType, and connect each two vertices of `second` in _matchType
 *
 * @param g
 */
void LComp::rewrite(ZXGraph* g) {
    reset();

    for (const auto& [v, neighbors] : _matchTypeVec) {
        _removeVertices.push_back(v);
        size_t hEdgeCount = 0;
        for (auto& [nb, etype] : v->getNeighbors()) {
            if (nb == v && etype == EdgeType::HADAMARD) {
                hEdgeCount++;
            }
        }
        Phase p = v->getPhase() + Phase(hEdgeCount / 2);
        // TODO - global scalar ignored
        for (size_t n = 0; n < neighbors.size(); n++) {
            neighbors[n]->setPhase(neighbors[n]->getPhase() - p);
            for (size_t j = n + 1; j < neighbors.size(); j++) {
                _edgeTableKeys.push_back(make_pair(neighbors[n], neighbors[j]));
                _edgeTableValues.push_back(make_pair(0, 1));
            }
        }
    }
}
