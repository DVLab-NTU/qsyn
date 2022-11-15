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
    size_t cnt = 0;
    for(const auto& v: g->getVertices()){
        id2idx[v->getId()] = cnt;
        cnt++;
    }

    vector<bool> taken(g->getNumVertices(), false);

    g -> forEachEdge([&g, &cnt, &id2idx, &taken, this](const EdgePair& epair) {
        // 1: Check EdgeType
        //NOTE - Only Hadamard
        if(epair.second != EdgeType::HADAMARD) return;

        // 2: Get Neighbors
        vector<ZXVertex*> neighbors;
        neighbors.push_back(epair.first.first);
        neighbors.push_back(epair.first.second);

        if(taken[id2idx[neighbors[0]->getId()]] || taken[id2idx[neighbors[1]->getId()]]) return;
        if(neighbors[0]->getType() != VertexType::Z || neighbors[1]->getType() != VertexType::Z) return;

        // 3: Check Neighbors Phase 
        if(neighbors[0]->getPhase() != Phase(1) && neighbors[0]->getPhase() != 0) return;
        if(neighbors[1]->getPhase() != Phase(1) && neighbors[1]->getPhase() != 0) return;

        // 4: Check neighbors of Neighbors
        size_t count_boundary = 0;

        vector<int> mark_v;

        //REVIEW - Squeeze into a for loop
        for(auto& neighbor: neighbors){
            for(auto& [v, et]: neighbor->getNeighbors()){
                mark_v.push_back(v->getId());
                if(v->getType() == VertexType::Z && et == EdgeType::HADAMARD) continue;
                else if(v->getType() == VertexType::BOUNDARY) count_boundary++;
                else return;
            }
        }

        if(count_boundary > 1) return;   // skip when Neighbors are all connected to boundary

        // 5: taken
        for(auto& x: mark_v){
            taken[id2idx[x]] = true;
        }
        taken[id2idx[neighbors[0]->getId()]] = true;
        taken[id2idx[neighbors[1]->getId()]] = true;

        // 6: add Epair into _matchTypeVec
        _matchTypeVec.push_back(epair);

        // 7: clear vector
        neighbors.clear();
        mark_v.clear();
    });
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

    for (auto& epair :  _matchTypeVec){
        // 1 : get m0 m1
        vector<ZXVertex*> neighbors;
        neighbors.push_back(epair.first.first);
        neighbors.push_back(epair.first.second);

        // 2 : boundary find
        for(size_t j=0; j<2; j++){
            bool remove = true;
            for(auto& [v, et] : neighbors[j]->getNeighbors()){
                if(v->getType() == VertexType::BOUNDARY){
                    _edgeTableKeys.push_back(make_pair(neighbors[1-j], v));
                    if( et == EdgeType::SIMPLE) _edgeTableValues.push_back(make_pair(0,1));
                    else _edgeTableValues.push_back(make_pair(1,0));
                    remove = false;
                    break;
                }
            }

            if(remove) _removeVertices.push_back(neighbors[1-j]);
        }

        // 3 4 grouping
        //REVIEW - Big Rewrite
        vector<int> c(g->getNumVertices() ,0);
        vector<ZXVertex*> n0;
        vector<ZXVertex*> n1;
        for(auto& [v, et] : neighbors[0]->getNeighbors()) {
            if (v->getType() == VertexType::BOUNDARY) continue;
            if (v == neighbors[1]) continue;
            n0.push_back(v);
        }
        for(auto& [v, et] : neighbors[1]->getNeighbors()) {
            if (v->getType() == VertexType::BOUNDARY) continue;
            if (v == neighbors[0]) continue;
            n1.push_back(v);
        }
        vector<ZXVertex*> n2;
        set_intersection(n0.begin(), n0.end(), n1.begin(), n1.end(), back_inserter(n2));

        vector<ZXVertex*> n3, n4;
        set_difference(n0.begin(), n0.end(), n2.begin(), n2.end(), back_inserter(n3));
        set_difference(n1.begin(), n1.end(), n2.begin(), n2.end(), back_inserter(n4));
        n0 = n3; n1 = n4;
        n3.clear();n4.clear();

        // 5: scalar (skip)
        
        // 6:add phase
        for(auto& x: n2)    x->setPhase(x->getPhase() + Phase(1) + neighbors[0]->getPhase() + neighbors[1]->getPhase());
        for(auto& x: n1)    x->setPhase(x->getPhase() + neighbors[0]->getPhase());
        for(auto& x: n0)    x->setPhase(x->getPhase() + neighbors[1]->getPhase());

        // 7:connect n0 n1 n2
        for(auto& itr : n0){
            if(itr->getType() == VertexType::BOUNDARY) continue;
            for(auto& a: n1){
                if(a->getType() == VertexType::BOUNDARY) continue;
                _edgeTableKeys.push_back(make_pair(itr, a));
                _edgeTableValues.push_back(make_pair(0,1));
            }
            for(auto& b: n2){
                if(b->getType() == VertexType::BOUNDARY) continue;
                _edgeTableKeys.push_back(make_pair(itr, b));
                _edgeTableValues.push_back(make_pair(0,1));
            }
        }

        for(auto& itr : n1){
            if(itr->getType() == VertexType::BOUNDARY) continue;
            for(auto& a: n2){
                if(a->getType() == VertexType::BOUNDARY) continue;
                _edgeTableKeys.push_back(make_pair(itr, a));
                _edgeTableValues.push_back(make_pair(0,1));
            }
        }

        // 8: clear vector
        neighbors.clear();
        c.clear();
        n0.clear();
        n1.clear();
        n2.clear();
    }
}