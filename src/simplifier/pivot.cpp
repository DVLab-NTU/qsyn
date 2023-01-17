/****************************************************************************
  FileName     [ pivot.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "zxGraph.h"
#include "zxRules.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Preprocess the matches so that it conforms with the rewrite functions
 *
 * @param g
 */
void Pivot::preprocess(ZXGraph* g) {
    for (auto& v : this->_boundaries) {
        auto& [nb, etype] = v->getFirstNeighbor();
        g->addBuffer(v, nb, etype);
    }
}
/**
 * @brief Finds matchings of the pivot rule.
 *
 * @param g
 */
void Pivot::match(ZXGraph* g) {
    this->_matchTypeVec.clear();
    this->_boundaries.clear();

    unordered_set<ZXVertex*> taken;
    vector<ZXVertex*> b0, b1;
    g->forEachEdge([&taken, &b0, &b1, this](const EdgePair& epair) {
        b0.clear();
        b1.clear();
        if (epair.second != EdgeType::HADAMARD) return;

        // 2: Get Neighbors
        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if (taken.contains(vs) || taken.contains(vt)) return;
        if (!vs->isZ() || !vt->isZ()) return;

        // 3: Check Neighbors Phase
        bool vsIsNPi = vs->hasNPiPhase();
        bool vtIsNPi = vt->hasNPiPhase();

        if (!vsIsNPi || !vtIsNPi) return;

        // 4: Check neighbors of Neighbors

        // REVIEW - Squeeze into a for loop
        for (auto& [v, et] : vs->getNeighbors()) {
            if (v->isZ() && et == EdgeType::HADAMARD)
                continue;
            else if (v->isBoundary()) {
                b0.push_back(v);
            } else {
                taken.insert(v);
                return;
            }
        }
        for (auto& [v, et] : vt->getNeighbors()) {
            if (v->isZ() && et == EdgeType::HADAMARD)
                continue;
            else if (v->isBoundary()) {
                b1.push_back(v);
            } else {
                taken.insert(v);
                return;
            }
        }

        // if(b0.size() > 0 && b1.size() > 0) return;   // skip when Neighbors are all connected to boundary
        if (b0.size() + b1.size() > 1) return;  // skip when Neighbors are all connected to boundary
        // 5: taken
        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->getNeighbors()) {
            taken.insert(v);
        }
        for (auto& [v, _] : vt->getNeighbors()) {
            taken.insert(v);
        }
        // 6: add Epair into _matchTypeVec
        this->_matchTypeVec.push_back({vs, vt});
        this->_boundaries.insert(this->_boundaries.end(), b0.begin(), b0.end());
        this->_boundaries.insert(this->_boundaries.end(), b1.begin(), b1.end());
    });
    setMatchTypeVecNum(this->_matchTypeVec.size());
}