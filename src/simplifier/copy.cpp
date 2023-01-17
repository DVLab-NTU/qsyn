/****************************************************************************
  FileName     [ copy.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ State Copy Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "zxGraph.h"
#include "zxRules.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Find spiders with a 0 or pi phase that have a single neighbor, and copies them through. Assumes that all the spiders are green and maximally fused.
 *
 * @param g
 */
void StateCopy::match(ZXGraph* g) {
    // Should be run in graph-like
    _matchTypeVec.clear();
    // if (verbose >= 8) g->printVertices();

    unordered_map<ZXVertex*, size_t> Vertex2idx;

    unordered_map<size_t, size_t> id2idx;
    size_t cnt = 0;
    for (const auto& v : g->getVertices()) {
        Vertex2idx[v] = cnt;
        cnt++;
    }

    vector<bool> validVertex(g->getNumVertices(), true);

    for (const auto& v : g->getVertices()) {
        if (!validVertex[Vertex2idx[v]]) continue;

        if (v->getType() != VertexType::Z) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }
        if (v->getPhase() != Phase(0) && v->getPhase() != Phase(1)) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }
        if (v->getNumNeighbors() != 1) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }

        ZXVertex* PiNeighbor = v->getFirstNeighbor().first;
        if (PiNeighbor->getType() != VertexType::Z) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }
        vector<ZXVertex*> applyNeighbors;
        for (const auto& [nebOfPiNeighbor, _] : PiNeighbor->getNeighbors()) {
            if (nebOfPiNeighbor != v)
                applyNeighbors.push_back(nebOfPiNeighbor);
            validVertex[Vertex2idx[nebOfPiNeighbor]] = false;
        }
        _matchTypeVec.push_back(make_tuple(v, PiNeighbor, applyNeighbors));
    }
    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        (Check PyZX/pyzx/rules.py/apply_copy for more details)
 *
 * @param g
 */
void StateCopy::rewrite(ZXGraph* g) {
    reset();

    // Need to update global scalar and phase
    for (size_t i = 0; i < _matchTypeVec.size(); i++) {
        ZXVertex* npi = get<0>(_matchTypeVec[i]);
        ZXVertex* a = get<1>(_matchTypeVec[i]);
        vector<ZXVertex*> neighbors = get<2>(_matchTypeVec[i]);
        _removeVertices.push_back(npi);
        _removeVertices.push_back(a);
        for (size_t i = 0; i < neighbors.size(); i++) {
            if (neighbors[i]->getType() == VertexType::BOUNDARY) {
                ZXVertex* newV = g->addVertex(neighbors[i]->getQubit(), VertexType::Z, npi->getPhase());
                bool simpleEdge = false;
                if ((neighbors[i]->getFirstNeighbor().second) == EdgeType::SIMPLE)
                    simpleEdge = true;
                _removeEdges.push_back(make_pair(make_pair(a, neighbors[i]), neighbors[i]->getFirstNeighbor().second));

                // new to Boundary
                _edgeTableKeys.push_back(make_pair(newV, neighbors[i]));
                _edgeTableValues.push_back(simpleEdge ? make_pair(0, 1) : make_pair(1, 0));

                // a to new
                _edgeTableKeys.push_back(make_pair(a, newV));
                _edgeTableValues.push_back(make_pair(0, 1));

                // REVIEW - Floating
                newV->setCol((neighbors[i]->getCol() + a->getCol()) / 2);

            } else {
                neighbors[i]->setPhase(npi->getPhase() + neighbors[i]->getPhase());
            }
        }
    }
}