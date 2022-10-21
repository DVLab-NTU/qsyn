/****************************************************************************
  FileName     [ hrule.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Hadamard Rule Definition ]
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
    if(verbose >= 7) g->printVertices();
    
    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    // Find all H-boxes
    vector<bool> taken(g->getNumVertices(), false);
    vector<bool> inMatches(g->getNumVertices(), false);
    for(size_t i = 0; i < g->getNumVertices(); i++){
        if(g->getVertices()[i]->getType() == VertexType::H_BOX && g->getVertices()[i]->getNumNeighbors() == 2){
            vector<ZXVertex*> neighbors = g->getVertices()[i]->getNeighbors();
            size_t n0 = id2idx[neighbors[0]->getId()], n1 = id2idx[neighbors[1]->getId()];
            if(taken[n0] || taken[n1]) continue;
            if(!inMatches[n0] && !inMatches[n1]){
                _matchTypeVec.push_back(g->getVertices()[i]);
                inMatches[id2idx[g->getVertices()[i]->getId()]] = true;
                taken[n0] = true;
                taken[n1] = true;
            }
        }
    }
    if(verbose >= 3) cout << "Find match of hadamard-rule: " << _matchTypeVec.size() << endl;
    setMatchTypeVecNum(_matchTypeVec.size());
}


void LComp::rewrite(ZXGraph* g){
    reset();
    setRemoveVertices(_matchTypeVec);
    vector<ZXVertex*> ns; vector<EdgeType*> ets;

    for(size_t i = 0; i < _matchTypeVec.size(); i++){
        // Only two neighbors which is ensured
        for(auto& itr : _matchTypeVec[i]->getNeighborMap()){
            ns.push_back(itr.first);
            ets.push_back(itr.second);
        }
        _edgeTableKeys.push_back(make_pair(ns[0], ns[1]));
        if(*ets[0] == *ets[1]) _edgeTableValues.push_back(make_pair(0,1));
        else _edgeTableValues.push_back(make_pair(1,0));

        //! TODO Correct for the sqrt(2) difference in H-boxes and H-edges
    }
}

