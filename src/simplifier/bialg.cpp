/****************************************************************************
  FileName     [ bialg.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Bialgebra Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>

#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Finds noninteracting matchings of the bialgebra rule.
 *        (Check PyZX/pyzx/rules.py/match_bialg_parallel for more details)
 *
 * @param g
 */

bool Bialgebra::check_duplicated_vertex(vector<ZXVertex*> vec) {
    vector<int> appeared = {};
    for (size_t i = 0; i < vec.size(); i++) {
        if (find(appeared.begin(), appeared.end(), vec[i]->getId()) == appeared.end()) {
            appeared.push_back(vec[i]->getId());
        } else
            return true;
    }
    return false;
}
void Bialgebra::match(ZXGraph* g) {
    _matchTypeVec.clear();

    unordered_map<size_t, size_t> id2idx;
    for (size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;
    vector<bool> taken(g->getNumVertices(), false);

    for (size_t i = 0; i < g->getNumEdges(); i++) {
        // Verify if the edge is a SIMPLE edge
        if (!(*g->getEdges()[i].second == EdgeType::SIMPLE)) continue;

        ZXVertex* left;
        left = g->getEdges()[i].first.first;
        ZXVertex* right;
        right = g->getEdges()[i].first.second;

        size_t n0 = id2idx[left->getId()], n1 = id2idx[right->getId()];

        // Verify if the vertices are taken
        if (taken[n0] || taken[n1]) continue;

        // Do not consider the phase spider yet
        // todo: consider the phase
        if ((left->getPhase() != 0) || (right->getPhase() != 0)) continue;

        // Verify if the edge is connected by a X and a Z spider.
        if (!((left->getType() == VertexType::X && right->getType() == VertexType::Z) || (left->getType() == VertexType::Z && right->getType() == VertexType::X))) continue;

        // Check if the vertices is_ground (with only one edge).
        if ((left->getNumNeighbors() == 1) || (right->getNumNeighbors() == 1)) continue;

        vector<ZXVertex*> neighbor_of_left = left->getNeighbors(), neighbor_of_right = right->getNeighbors();

        // Check if a vertex has a same neighbor, in other words, two or more edges to another vertex.
        if (check_duplicated_vertex(neighbor_of_left) || check_duplicated_vertex(neighbor_of_right)) continue;

        // Check if all neighbors of z are x without phase and all neighbors of x are z without phase.
        if (!all_of(neighbor_of_left.begin(), neighbor_of_left.end(), [right](ZXVertex* v) { return (v->getPhase() == 0 && v->getType() == right->getType()); })) continue;
        if (!all_of(neighbor_of_right.begin(), neighbor_of_right.end(), [left](ZXVertex* v) { return (v->getPhase() == 0 && v->getType() == left->getType()); })) continue;

        // Check if all the edges are SIMPLE
        // TODO: Make H edge aware too.
        if (!all_of(left->getNeighborMap().begin(), left->getNeighborMap().end(), [](pair<ZXVertex*, EdgeType*> edge_pair) { return *edge_pair.second == EdgeType::SIMPLE; })) continue;
        if (!all_of(right->getNeighborMap().begin(), right->getNeighborMap().end(), [](pair<ZXVertex*, EdgeType*> edge_pair) { return *edge_pair.second == EdgeType::SIMPLE; })) continue;

        _matchTypeVec.push_back(g->getEdges()[i]);

        // set left, right and their neighbors into taken
        for (size_t j = 0; j < neighbor_of_left.size(); j++) {
            taken[id2idx[neighbor_of_left[j]->getId()]] = true;
        }
        for (size_t j = 0; j < neighbor_of_right.size(); j++) {
            taken[id2idx[neighbor_of_right[j]->getId()]] = true;
        }
    }
    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Performs a certain type of bialgebra rewrite based on `_matchTypeVec`
 *        (Check PyZX/pyzx/rules.py/bialg for more details)
 *
 * @param g
 */
void Bialgebra::rewrite(ZXGraph* g) {
    reset();
    // TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD

    for (size_t i = 0; i < _matchTypeVec.size(); i++) {
        ZXVertex* left;
        left = _matchTypeVec[i].first.first;
        ZXVertex* right;
        right = _matchTypeVec[i].first.second;

        vector<ZXVertex*> neighbor_of_left = left->getNeighbors(), neighbor_of_right = right->getNeighbors();

        /* g->removeVertex(left);
        g->removeVertex(right); */
        _removeVertices.push_back(left);
        _removeVertices.push_back(right);

        for (size_t j = 0; j < neighbor_of_left.size(); j++) {
            if (neighbor_of_left[j] == right) continue;
            for (size_t k = 0; k < neighbor_of_right.size(); k++) {
                if (neighbor_of_right[k] == left) continue;
                _edgeTableKeys.push_back(make_pair(neighbor_of_left[j], neighbor_of_right[k]));
                _edgeTableValues.push_back(make_pair(1, 0));
            }
        }
    }
}