/****************************************************************************
  FileName     [ hfusion.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Cancellation Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "zxGraph.h"
#include "zxRules.h"

using namespace std;

extern size_t verbose;

// FIXME - Seems buggy. Not fixing this as it is not in full reduce
/**
 * @brief Match Hadamard-edges that are connected to H-boxes or two neighboring H-boxes
 *
 * @param g
 */
void HboxFusion::match(ZXGraph* g) {
    _matchTypeVec.clear();

    unordered_map<size_t, size_t> id2idx;
    size_t cnt = 0;
    for (const auto& v : g->getVertices()) {
        id2idx[v->getId()] = cnt;
        cnt++;
    }

    // Matches Hadamard-edges that are connected to H-boxes
    vector<bool> taken(g->getNumVertices(), false);

    g->forEachEdge([&id2idx, &taken, this](const EdgePair& epair) {
        // NOTE - Only Hadamard Edges
        if (epair.second != EdgeType::HADAMARD) return;
        ZXVertex* neighbor_left = epair.first.first;
        ZXVertex* neighbor_right = epair.first.second;
        size_t n0 = id2idx[neighbor_left->getId()], n1 = id2idx[neighbor_right->getId()];

        if ((taken[n0] && neighbor_left->getType() == VertexType::H_BOX) || (taken[n1] && neighbor_right->getType() == VertexType::H_BOX)) return;

        if (neighbor_left->getType() == VertexType::H_BOX) {
            _matchTypeVec.push_back(neighbor_left);
            taken[n0] = true;
            taken[n1] = true;

            Neighbors nebs = neighbor_left->getNeighbors();
            NeighborPair nbp0 = *(nebs.begin());
            NeighborPair nbp1 = *next(nebs.begin());
            size_t n2 = id2idx[nbp0.first->getId()], n3 = id2idx[nbp1.first->getId()];

            if (n2 != n0)
                taken[n2] = true;
            else
                taken[n3] = true;
        } else if (neighbor_right->getType() == VertexType::H_BOX) {
            _matchTypeVec.push_back(neighbor_right);
            taken[n0] = true;
            taken[n1] = true;

            Neighbors nebs = neighbor_left->getNeighbors();
            NeighborPair nbp0 = *(nebs.begin());
            NeighborPair nbp1 = *next(nebs.begin());
            size_t n2 = id2idx[nbp0.first->getId()], n3 = id2idx[nbp1.first->getId()];

            if (n2 != n0)
                taken[n2] = true;
            else
                taken[n3] = true;
        } else if (neighbor_left->getType() != VertexType::H_BOX || neighbor_right->getType() != VertexType::H_BOX) {
            return;
        }
    });

    g->forEachEdge([&id2idx, &taken, this](const EdgePair& epair) {
        if (epair.second == EdgeType::HADAMARD) return;

        ZXVertex* neighbor_left = epair.first.first;
        ZXVertex* neighbor_right = epair.first.second;
        size_t n0 = id2idx[neighbor_left->getId()], n1 = id2idx[neighbor_right->getId()];

        if (!taken[n0] && !taken[n1]) {
            if (neighbor_left->getType() == VertexType::H_BOX && neighbor_right->getType() == VertexType::H_BOX) {
                _matchTypeVec.push_back(neighbor_left);
                _matchTypeVec.push_back(neighbor_right);
                taken[n0] = true;
                taken[n1] = true;
            }
        }
    });
    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *
 * @param g
 */
void HboxFusion::rewrite(ZXGraph* g) {
    reset();
    setRemoveVertices(_matchTypeVec);

    for (size_t i = 0; i < _matchTypeVec.size(); i++) {
        // NOTE - Only two neighbors which is ensured
        vector<ZXVertex*> ns;
        vector<EdgeType> ets;

        for (auto& itr : _matchTypeVec[i]->getNeighbors()) {
            ns.push_back(itr.first);
            ets.push_back(itr.second);
        }

        _edgeTableKeys.push_back(make_pair(ns[0], ns[1]));
        if (ets[0] == ets[1])
            _edgeTableValues.push_back(make_pair(0, 1));
        else
            _edgeTableValues.push_back(make_pair(1, 0));
        // TODO - Correct for the sqrt(2) difference in H-boxes and H-edges
    }
}