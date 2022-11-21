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
    
    size_t cnt = 0;
    unordered_set<ZXVertex*> taken;

    g -> forEachEdge([&cnt, &taken, this](const EdgePair& epair) {
        if(epair.second != EdgeType::HADAMARD) return;

        // 2: Get Neighbors
        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if(taken.contains(vs) || taken.contains(vt)) return;
        if(!vs->isZ() || !vt->isZ()) return;

        // 3: Check Neighbors Phase 
        bool vsIsNPi = (vs->getPhase().getRational().denominator() == 1);
        bool vtIsNPi = (vt->getPhase().getRational().denominator() == 1);

        if (!vsIsNPi || !vtIsNPi) return;

        // 4: Check neighbors of Neighbors
        size_t count_boundary = 0;

        //REVIEW - Squeeze into a for loop
        for(auto& [v, et]: vs->getNeighbors()){
            if(v->isZ() && et == EdgeType::HADAMARD) continue;
            else if(v->isBoundary()) count_boundary++;
            else return;
        }
        for(auto& [v, et]: vt->getNeighbors()){
            if(v->isZ() && et == EdgeType::HADAMARD) continue;
            else if(v->isBoundary()) count_boundary++;
            else return;
        }

        if(count_boundary > 1) return;   // skip when Neighbors are all connected to boundary

        // 5: taken
        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->getNeighbors()) taken.insert(v);
        for (auto& [v, _] : vt->getNeighbors()) taken.insert(v);

        // 6: add Epair into _matchTypeVec
        _matchTypeVec.push_back({vs, vt});

        // 7: clear vector
    });

    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        
 * @param g 
 */
void Pivot::rewrite(ZXGraph* g){
    reset();

    for (auto& m :  _matchTypeVec){
        // 1 : get m0 m1
        vector<ZXVertex*> neighbors;
        neighbors.push_back(m[0]);
        neighbors.push_back(m[1]);

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
        n0.clear();
        n1.clear();
        n2.clear();
    }
}