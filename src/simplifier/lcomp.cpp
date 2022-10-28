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

    // Find all Z vertices that connect to all neighbors with H edge.
    vector<bool> taken(g->getNumVertices(), false);
    vector<bool> inMatches(g->getNumVertices(), false);
    for(size_t i = 0; i < g->getNumVertices(); i++){
        if(g->getVertices()[i]->getType() == VertexType::Z && (g->getVertices()[i]->getPhase() == Phase(1,2) || g->getVertices()[i]->getPhase() == Phase(3,2)) ){
            bool match_conditions = true;
            size_t vIdx = id2idx[g->getVertices()[i]->getId()];
            if(taken[vIdx]) continue;
            // EdgeType = H ; VertexType = Z ; not in taken or inMatches
            //! TODO: self loop avoidance
            for(auto itr = g->getVertices()[i]->getNeighborMap().begin(); itr != g->getVertices()[i]->getNeighborMap().end(); itr++){
                if(*(itr->second) != EdgeType::HADAMARD || itr->first->getType() != VertexType::Z || taken[id2idx[itr->first->getId()]] || inMatches[id2idx[itr->first->getId()]]){
                    match_conditions = false;
                    break;
                }
            }
            if(match_conditions){
                vector<ZXVertex* > neighbors;
                for(auto itr = g->getVertices()[i]->getNeighborMap().begin(); itr != g->getVertices()[i]->getNeighborMap().end(); itr++){
                    neighbors.push_back(itr->first);
                    taken[id2idx[itr->first->getId()]] = true;
                }
                inMatches[id2idx[g->getVertices()[i]->getId()]] = true;
                _matchTypeVec.push_back(make_pair(g->getVertices()[i], neighbors));
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
    
    for(size_t i = 0; i < _matchTypeVec.size(); i++){
        _removeVertices.push_back(_matchTypeVec[i].first);
        Phase p = _matchTypeVec[i].first->getPhase();
        //! TODO global scalar ignored
        for(size_t n = 0; n < _matchTypeVec[i].second.size(); n++){
            _matchTypeVec[i].second[n]->setPhase(_matchTypeVec[i].second[n]->getPhase()-p);
            for(size_t j = n+1; j < _matchTypeVec[i].second.size(); j++){
                _edgeTableKeys.push_back(make_pair(_matchTypeVec[i].second[n], _matchTypeVec[i].second[j]));
                _edgeTableValues.push_back(make_pair(0, 1));
            }
        }
    }
}

