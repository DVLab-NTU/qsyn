/****************************************************************************
  FileName     [ pivot.cpp ]
  PackageName  [ simplifier ]
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
 * @brief Finds matchings of the pivot rule.
 *        (Check PyZX/pyzx/rules.py/match_pivot_parallel for more details)
 * 
 * @param g 
 */
void Pivot::match(ZXGraph* g){
    _matchTypeVec.clear(); 
    if(verbose >= 8) g->printVertices();

    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    vector<bool> taken(g->getNumVertices(), false);

    // traverse edge
    for(size_t i = 0; i < g->getNumEdges(); i++){
        //// 1: Check EdgeType
        if(* g->getEdges()[i].second != EdgeType::HADAMARD) continue;

        //// 2: Get Neighbors
        vector<ZXVertex*> neighbors;
        neighbors.push_back(g->getEdges()[i].first.first);
        neighbors.push_back(g->getEdges()[i].first.second);

        if(taken[id2idx[neighbors[0]->getId()]] || taken[id2idx[neighbors[1]->getId()]]) continue;
        if(neighbors[0]->getType() != VertexType::Z || neighbors[1]->getType() != VertexType::Z) continue;

        //// 3: Check Neighbors Phase 
        if(neighbors[0]->getPhase() != Phase(1) && neighbors[0]->getPhase() != 0) continue;
        if(neighbors[1]->getPhase() != Phase(1) && neighbors[1]->getPhase() != 0) continue;

        //// 4: Check neighbors of Neighbors
        bool invalid_edge = false;

        size_t count_b = 0;

        vector<int> mark_v;

        // v0
        for(auto& x: neighbors[0]->getNeighborMap()){
            mark_v.push_back(x.first->getId());

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
            mark_v.push_back(x.first->getId());

            if(x.first->getType() == VertexType::Z && * x.second == EdgeType::HADAMARD) continue;
            else if(x.first->getType() == VertexType::BOUNDARY) count_b++;
            else {
                invalid_edge = true;
                break;
            }

        }

        if(invalid_edge) continue;
        if(count_b > 1) continue;   // skip when Neighbors are all connected to boundary

        //// 5: taken
        for(auto& x: mark_v){
            taken[id2idx[x]] = true;
        }
        taken[id2idx[neighbors[0]->getId()]] = true;
        taken[id2idx[neighbors[1]->getId()]] = true;

        //// 6: add edge id into _matchTypeVec
        _matchTypeVec.push_back(i); // id

        //// 7: clear vector
        neighbors.clear();
        mark_v.clear();

    }
    taken.clear();

    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
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


    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;
    vector<bool> isBoundary(g->getNumVertices(), false);

    for (auto& i :  _matchTypeVec){
        // 1 : get m0 m1
        vector<ZXVertex*> neighbors;
        neighbors.push_back(g->getEdges()[i].first.first);
        neighbors.push_back(g->getEdges()[i].first.second);

        // 2 : boundary find
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

        // 3 table of c
        vector<int> c(g->getNumVertices() ,0);
        vector<ZXVertex*> n0;
        vector<ZXVertex*> n1;
        vector<ZXVertex*> n2;
        for(auto& x : neighbors[0]->getNeighbors()) {
            if (isBoundary[id2idx[x->getId()]]) continue;
            if (x == neighbors[1]) continue;
            c[id2idx[x->getId()]]++;
        }
        for(auto& x : neighbors[1]->getNeighbors()) {
            if (isBoundary[id2idx[x->getId()]]) continue;
            if (x == neighbors[0]) continue;
            c[id2idx[x->getId()]] += 2;
        }

        // 4  Find n0 n1 n2
        for (size_t a=0; a<g->getNumVertices(); a++){
            if(c[a] == 1) n0.push_back(g->getVertices()[a]);
            else if (c[a] == 2) n1.push_back(g->getVertices()[a]);
            else if (c[a] == 3) n2.push_back(g->getVertices()[a]);
            else continue;
        }
        
        //// 5: scalar (skip)
        

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

        //// 7: connect n0 n1 n2
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

        //// 8: clear vector
        neighbors.clear();
        c.clear();
        n0.clear();
        n1.clear();
        n2.clear();

    }

    isBoundary.clear();
    
}