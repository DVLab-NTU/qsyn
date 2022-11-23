/****************************************************************************
  FileName     [ pivot.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <numbers>
#include <vector>

#include "zxRules.h"
using namespace std;

extern size_t verbose;

bool PivotBoundary::checkBadMatch(const Neighbors& nebs, EdgePair e, bool isVsNeighbor) {
    for (const auto& [v, et] : nebs) {
        if (v->getType() != VertexType::Z)
            return true;
        NeighborPair nbp = v->getFirstNeighbor();
        if (isVsNeighbor && v->getNumNeighbors() == 1 && makeEdgePair(v, nbp.first, nbp.second) != e)
            return true;
    }
    return false;
}

/**
 * @brief Finds matchings of the pivot rule.
 *        (Check PyZX/pyzx/rules.py/match_pivot_parallel for more details)
 *
 * @param g
 */
void PivotBoundary::match(ZXGraph* g) {
    _matchTypeVec.clear();
    // if (verbose >= 8) g->printVertices();
    // if (verbose >= 5) cout << "> match...\n";

    // unordered_map<size_t, size_t> id2idx;
    // size_t cnt = 0;
    // for (const auto& v : g->getVertices()) {
    //     id2idx[v->getId()] = cnt;
    //     cnt++;
    // }

    // vector<bool> taken(g->getNumVertices(), false);
    // for (const auto& v : g->getVertices()) {

    // }

    // unordered_map<size_t, ZXVertex*> table;
    // for (size_t i = 0; i < verticesStorage.size(); i++) {
    //     table[i] = g->addVertex(-2, VertexType::Z, verticesStorage[i]);
    // }
    // for (auto& [idx, vt] : edgesStorage) {
    //     g->addEdge(table[idx], vt, EdgeType(EdgeType::SIMPLE));
    // }
    // for (auto& [vpairs, idx] : matchesStorage) {
    //     vector<ZXVertex*> match{vpairs.first, vpairs.second, table[idx]};
    //     _matchTypeVec.emplace_back(match);
    // }
    // for (auto& e : newEdges) g->addEdge(e.first.first, e.first.second, e.second);

    // newEdges.clear();

    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *
 * @param g
 */
void PivotBoundary::rewrite(ZXGraph* g) {
    reset();

    // for (auto& m : _matchTypeVec) {
    //     if (verbose >= 5) {
    //         cout << "> rewrite...\n";
    //         cout << "vs: " << m[0]->getId() << "\tvt: " << m[1]->getId() << "\tv_gadget: " << m[2]->getId() << endl;
    //     }
    //     vector<ZXVertex*> n0 = m[0]->getCopiedNeighbors();
    //     vector<ZXVertex*> n1 = m[1]->getCopiedNeighbors();
    //     // cout << n0.size() << ' ' << n1.size() << endl;

    //     for (auto itr = n0.begin(); itr != n0.end();) {
    //         if (n0[itr - n0.begin()] == m[1]) {
    //             n0.erase(itr);
    //         } else
    //             itr++;
    //     }

    //     for (auto itr = n1.begin(); itr != n1.end();) {
    //         if (n1[itr - n1.begin()] == m[0] || n1[itr - n1.begin()] == m[2]) {
    //             n1.erase(itr);
    //         } else
    //             itr++;
    //     }

    //     vector<ZXVertex*> n2;
    //     set_intersection(n0.begin(), n0.end(), n1.begin(), n1.end(), back_inserter(n2));

    //     vector<ZXVertex*> n3, n4;
    //     set_difference(n0.begin(), n0.end(), n2.begin(), n2.end(), back_inserter(n3));
    //     set_difference(n1.begin(), n1.end(), n2.begin(), n2.end(), back_inserter(n4));
    //     n0 = n3;
    //     n1 = n4;
    //     n3.clear();
    //     n4.clear();

    //     // Add edge table
    //     for (auto& s : n0) {
    //         for (auto& t : n1) {
    //             if (s->getId() == t->getId()) {
    //                 s->setPhase(s->getPhase() + Phase(1));
    //             } else {
    //                 _edgeTableKeys.push_back(make_pair(s, t));
    //                 _edgeTableValues.push_back(make_pair(0, 1));
    //             }
    //         }
    //         for (auto& t : n2) {
    //             if (s->getId() == t->getId()) {
    //                 s->setPhase(s->getPhase() + Phase(1));
    //             } else {
    //                 _edgeTableKeys.push_back(make_pair(s, t));
    //                 _edgeTableValues.push_back(make_pair(0, 1));
    //             }
    //         }
    //     }
    //     for (auto& s : n1) {
    //         for (auto& t : n2) {
    //             if (s->getId() == t->getId()) {
    //                 s->setPhase(s->getPhase() + Phase(1));
    //             } else {
    //                 _edgeTableKeys.push_back(make_pair(s, t));
    //                 _edgeTableValues.push_back(make_pair(0, 1));
    //             }
    //         }
    //     }

    //     for (auto& v : n2) {
    //         // REVIEW - check if not ground
    //         v->setPhase(v->getPhase() + 1);
    //     }

    //     // REVIEW - scalar add power
    //     for (int i = 0; i < 2; i++) {
    //         Phase a = m[i]->getPhase();
    //         vector<ZXVertex*> target;
    //         target = (i == 0) ? n1 : n0;
    //         if (a != Phase(0)) {
    //             for (auto& t : target) {
    //                 // REVIEW - check if not ground
    //                 t->setPhase(t->getPhase() + a);
    //             }
    //             for (auto& t : n2) {
    //                 // REVIEW - check if not ground
    //                 t->setPhase(t->getPhase() + a);
    //             }
    //         }

    //         if (i == 0) _removeVertices.push_back(m[1]);
    //         if (i == 1) {
    //             // EdgePair e = g->getIncidentEdges(m[2])[0];
    //             NeighborPair ep = m[2]->getFirstNeighbor();
    //             EdgePair e = makeEdgePair(m[2], ep.first, ep.second);
    
    //             _edgeTableKeys.push_back(make_pair(m[0], m[2]));
    //             if (e.second == EdgeType::SIMPLE) {
    //                 _edgeTableValues.push_back(make_pair(0, 1));
    //             } else {
    //                 _edgeTableValues.push_back(make_pair(1, 0));
    //             }
    //             _removeEdges.push_back(e);
    //         }
    //     }
    // }
}