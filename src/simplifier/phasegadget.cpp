/****************************************************************************
  FileName     [ phasegadget.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include <numbers>
#include <map>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Determines which phase gadgets act on the same vertices, so that they can be fused together.
 *        (Check PyZX/pyzx/rules.py/match_phase_gadgets for more details)
 * 
 * @param g 
 */
void PhaseGadget::match(ZXGraph* g){

    //REVIEW - NO TESTCASES FOR GADGET!! CHECK AFTER ALL RULES ARE FINISHED
    _matchTypeVec.clear(); 
    if(verbose >= 8) g->printVertices();
    
    unordered_map<size_t, size_t> id2idx;
    size_t cnt = 0;
    for(const auto& v: g->getVertices()){
        id2idx[v->getId()] = cnt;
        cnt++;
    }

    vector<bool> taken(g->getNumVertices(), false);

    unordered_map<ZXVertex*, ZXVertex*> gadgets; // (v, the only neighbor)
    unordered_multimap<vector<ZXVertex*>, ZXVertex*> groups;
    unordered_map<vector<ZXVertex*>, bool> done;
    for(const auto& v : g->getVertices()){
        if(v->getPhase() != Phase(0) && v->getPhase() != Phase(1) && v->getNumNeighbors() == 1){
            ZXVertex* neighbor = v->getFirstNeighbor().first;
            if(neighbor->getPhase() != Phase(0) and neighbor->getPhase() != Phase(1)) continue;
            if(neighbor->getType() == VertexType::BOUNDARY) continue;
            if(gadgets.contains(neighbor)) continue;
            // if(taken[id2idx[neighbor->getId()]]) continue;
            vector<ZXVertex*> nebsOfNeighbor;
            for(const auto& nbp : neighbor->getNeighbors()){
                nebsOfNeighbor.push_back(nbp.first);
            }
            nebsOfNeighbor.erase(remove(nebsOfNeighbor.begin(),nebsOfNeighbor.end(),v), nebsOfNeighbor.end());
            
            sort(nebsOfNeighbor.begin(), nebsOfNeighbor.end());
            
            gadgets[neighbor] = v;
            if(nebsOfNeighbor.size()>0){
                done[nebsOfNeighbor] = false;
                groups.insert(make_pair(nebsOfNeighbor,neighbor));
            }
                
            if(verbose >=9){
                for(size_t k=0;k<nebsOfNeighbor.size(); k++){
                    cout << nebsOfNeighbor[k]->getId() << " ";
                }
                cout << " axel added: "<<neighbor->getId() << endl;
            }
        }
    }

    for(auto itr=groups.begin(); itr!=groups.end(); itr++){
        if(done[itr->first]) continue;
        else done[itr->first]= true;
        auto gadgetList = groups.equal_range(itr->first);
        vector<ZXVertex*> axels;
        vector<ZXVertex*> leaves;
        Phase tot = Phase(0);
        for(auto gad = gadgetList.first; gad!= gadgetList.second; gad++){
            if(gad->second->getPhase()==Phase(1)){
                gad->second->setPhase(Phase(0));
                tot = tot + (-1) * gadgets[gad->second]->getPhase();
                gadgets[gad->second] -> setPhase((-1) * gadgets[gad->second]->getPhase());
            }
            else{
                tot = tot + gadgets[gad->second]->getPhase();
            }
            axels.push_back(gad->second);
            leaves.push_back(gadgets[gad->second]);
        }
        if(leaves.size()>1){
            _matchTypeVec.emplace_back(tot, axels, leaves);
        }
            
    }
    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        
 * @param g 
 */
void PhaseGadget::rewrite(ZXGraph* g){
    reset();
    
    for(size_t i=0; i<_matchTypeVec.size(); i++){
        ZXVertex* leaf = get<2>(_matchTypeVec[i])[0];
        leaf -> setPhase(get<0>(_matchTypeVec[i]));
        vector<ZXVertex*> rm_axels = get<1>(_matchTypeVec[i]);
        vector<ZXVertex*> rm_leaves = get<2>(_matchTypeVec[i]);
        for(size_t j=1; j<rm_axels.size(); j++){
            _removeVertices.push_back(rm_axels[j]);
        }
        for(size_t j=1; j<rm_leaves.size(); j++){
            _removeVertices.push_back(rm_leaves[j]);
        }
    }
}