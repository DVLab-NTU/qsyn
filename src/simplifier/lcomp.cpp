/****************************************************************************
  FileName     [ lcomp.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Local Complementary Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <iostream>
#include <vector>
#include "zxRules.h"
using namespace std;

extern size_t verbose;


/**
 * @brief Finds noninteracting matchings of the local complementation rule.
 * 
 * @param g 
 */
void LComp::match(ZXGraph* g){
    _matchTypeVec.clear();
    if(verbose >= 8) g->printVertices();
    
    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    // Find all Z vertices that connect to all neighb ors with H edge.
    vector<bool> taken(g->getNumVertices(), false);
    vector<bool> inMatches(g->getNumVertices(), false);
    for(const auto& v : g->getVertices()){
        if(v->getType() == VertexType::Z && (v->getPhase() == Phase(1,2) || v->getPhase() == Phase(3,2)) ){
            bool matchCondition = true;
            size_t vIdx = id2idx[v->getId()];
            if(taken[vIdx]) continue;
            // EdgeType = H ; VertexType = Z ; not in taken or inMatches
            //! TODO: self loop avoidance
            for(const auto& [nb, etype] : v->getNeighborMap()){
                if(*(etype) != EdgeType::HADAMARD || nb->getType() != VertexType::Z || taken[id2idx[nb->getId()]] || inMatches[id2idx[nb->getId()]]){
                    matchCondition = false;
                    break;
                }
            }
            if (matchCondition) {
                vector<ZXVertex* > neighbors;
                for(const auto& [nb, _] : v->getNeighborMap()){
                    if (v == nb) continue;
                    neighbors.push_back(nb);
                    taken[id2idx[nb->getId()]] = true;
                }
                inMatches[id2idx[v->getId()]] = true;
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
void LComp::rewrite(ZXGraph* g){
    reset();
    
    for(const auto& [v, neighbors] : _matchTypeVec){
        _removeVertices.push_back(v);
        size_t hEdgeCount = 0;
        for (auto& [nb, etype] : v->getNeighborMap()) {
            if (nb == v && *etype == EdgeType::HADAMARD) {
                hEdgeCount++;
            }
        }
        Phase p = v->getPhase() + Phase(hEdgeCount/2);
        //! TODO global scalar ignored
        for(size_t n = 0; n < neighbors.size(); n++){
            neighbors[n]->setPhase(neighbors[n]->getPhase()-p);
            for(size_t j = n+1; j < neighbors.size(); j++){
                
                _edgeTableKeys.push_back(make_pair(neighbors[n], neighbors[j]));
                _edgeTableValues.push_back(make_pair(0, 1));
            }
        }
    }
}

