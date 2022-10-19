/****************************************************************************
  FileName     [ pivot.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include <numbers>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Matches Hadamard-edges that are connected to H-boxes or two neighboring H-boxes
 *        (Check PyZX/pyzx/hrules.py/match_connected_hboxes for more details)
 * 
 * @param g 
 */
//size_t i = 1;

void Pivot::match(ZXGraph* g){
    _matchTypeVec.clear(); //should be an edgeType

    //TODO: rewrite _matchTypeVec
    if(verbose >= 7) g->printVertices();

    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    vector<bool> taken(g->getNumVertices(), false);

    // Main match code
    for(size_t i = 0; i < g->getNumEdges(); i++){
        //// 0
        //// 1
        if(* g->getEdges()[i].second != EdgeType::HADAMARD) continue;

        //// 2
        vector<ZXVertex*> neighbors;
        neighbors.push_back(g->getEdges()[i].first.first);
        neighbors.push_back(g->getEdges()[i].first.second);

        if(taken[id2idx[neighbors[0]->getId()]] || taken[id2idx[neighbors[1]->getId()]]) continue;
        if(neighbors[0]->getType() != VertexType::Z || neighbors[1]->getType() != VertexType::Z) continue;

        //// 3
        if(neighbors[0]->getPhase() != Phase(1) || neighbors[1]->getPhase() != Phase(1)) continue;

        //// 4
        bool invalid_edge = false;

        size_t count_b = 0;

        vector<int> mark_v;

        // v0
        for(auto& x: neighbors[0]->getNeighborMap()){
            for(auto& itr : x.first->getNeighborMap()){
                mark_v.push_back(itr.first->getId());
            }

            if(x.first->getType() == VertexType::Z && * x.second == EdgeType::HADAMARD) continue;
            else if(x.first->getType() == VertexType::BOUNDARY) count_b++;
            else {
                invalid_edge = true;
                break;
            }

        }

        if(invalid_edge) continue;

        // v1
        for(auto& x: neighbors[1]->getNeighborMap()){
            for(auto& itr : x.first->getNeighborMap()){
                mark_v.push_back(itr.first->getId());
            }

            if(x.first->getType() == VertexType::Z && * x.second == EdgeType::HADAMARD) continue;
            else if(x.first->getType() == VertexType::BOUNDARY) count_b++;
            else {
                invalid_edge = true;
                break;
            }

        }

        if(invalid_edge) continue;
        if(count_b > 1) continue;

        //// 5
        for(auto& x: mark_v){
            taken[id2idx[x]] = true;
        }

        //// 6
        taken[i] = true;
        _matchTypeVec.push_back(i); // id

    }

    if(verbose >= 3) cout << "Find match of pivot-rule: " << _matchTypeVec.size() << endl;

    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        (Check PyZX/pyzx/hrules.py/fuse_hboxes for more details)
 * 
 * @param g 
 */
void Pivot::rewrite(ZXGraph* g){
    reset();
    //TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD

    //_removeEdges = _matchTypeVec;

    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;
    vector<bool> isBoundary(g->getNumVertices(), false);

    for (auto& i :  _matchTypeVec){
        // 1
        vector<ZXVertex*> neighbors;
        neighbors.push_back(g->getEdges()[i].first.first);
        neighbors.push_back(g->getEdges()[i].first.second);

        // 2 - 1 : boundary find
        for(size_t j=0; j<2; j++){
            bool remove = true;

            for(auto& itr : neighbors[j]->getNeighborMap()){
                if(itr.first->getType() == VertexType::BOUNDARY){
                    _edgeTableKeys.push_back(make_pair(neighbors[1-j], itr.first));
                    if( * itr.second == EdgeType::SIMPLE)_edgeTableValues.push_back(make_pair(0,1));
                    else _edgeTableValues.push_back(make_pair(1,0));
                    remove = false;
                    isBoundary[id2idx[itr.first->getId()]] = true;
                    break;
                }
            }

            if(remove) _removeVertices.push_back(neighbors[1-j]);
        }

        // 2 - 2
        vector<ZXVertex*> n0;
        vector<ZXVertex*> n1;
        vector<ZXVertex*> n2;
        for(auto& x : neighbors[0]->getNeighbors()) {
            if (isBoundary[id2idx[x->getId()]]) continue;
            if (x == neighbors[1]) continue;
            n0.push_back(x);
        }
        for(auto& x : neighbors[1]->getNeighbors()) {
            if (isBoundary[id2idx[x->getId()]]) continue;
            if (x == neighbors[0]) continue;
            n1.push_back(x);
        }

        // 3 4  Find n2
        for (size_t j=0; j<n0.size(); j++){
            for(size_t k=0; k<n1.size(); k++) {
                if (n0[j] == n1[k]){
                    n2.push_back(n0[j]);
                    n0.erase(n0.begin() + j);
                    n1.erase(n1.begin() + k);
                    break;
                }
            }
        }

        //// 5: scalar
        

        //// 6:add phase
        for(auto& x: n2){
            x->setPhase(x->getPhase() + Phase(1) + neighbors[0]->getPhase() + neighbors[1]->getPhase());
        }

        for(auto& x: n1){
            x->setPhase(x->getPhase() + neighbors[0]->getPhase());
        }

        for(auto& x: n0){
            x->setPhase(x->getPhase() + neighbors[1]->getPhase());
        }

        //// 7 
        for(auto& itr : n0){
            if(isBoundary[itr->getId()]) continue;
            for(auto& a: n1){
                if(isBoundary[id2idx[a->getId()]]) continue;
                _edgeTableKeys.push_back(make_pair(itr, a));
                _edgeTableValues.push_back(make_pair(0,1));
            }
            for(auto& b: n2){
                if(isBoundary[id2idx[b->getId()]]) continue;
                _edgeTableKeys.push_back(make_pair(itr, b));
                _edgeTableValues.push_back(make_pair(0,1));
            }
        }

        for(auto& itr : n1){
            if(isBoundary[id2idx[itr->getId()]]) continue;
            for(auto& a: n2){
                if(isBoundary[id2idx[a->getId()]]) continue;
                _edgeTableKeys.push_back(make_pair(itr, a));
                _edgeTableValues.push_back(make_pair(0,1));
            }
        }

    }
    
}