/****************************************************************************
  FileName     [ hfusion.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Hadamard Cancellation Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <iostream>
#include <vector>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Matches Hadamard-edges that are connected to H-boxes or two eighboring H-boxes
 *        (Check PyZX/pyzx/hrules.py/match_connected_hboxes for more details)
 * 
 * @param g 
 */
void HboxFusion::match(ZXGraph* g){
    _matchTypeVec.clear();

    //TODO: rewrite _matchTypeVec
    if(verbose >= 7) g->printVertices();
  
    
    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    vector<bool> taken(g->getNumVertices(), false);
    // vector<bool> inMatches(g->getNumVertices(), false);
    // typedef pair<pair<ZXVertex*, ZXVertex*>, EdgeType*> EdgePair;
    for(size_t i = 0; i < g->getNumEdges(); i++){
        if(* g->getEdges()[i].second != EdgeType::HADAMARD) {
            //cout << "Not H-edge." << endl;
            continue;
        }
        else {
            //cout << "Is H." << endl;
        }

        ZXVertex* neighbor_left;
        neighbor_left = g->getEdges()[i].first.first;
        ZXVertex* neighbor_right;
        neighbor_right = g->getEdges()[i].first.second;

        size_t n0 = id2idx[neighbor_left->getId()], n1 = id2idx[neighbor_right->getId()];

        if((taken[n0] && neighbor_left->getType() == VertexType::H_BOX) || (taken[n1] && neighbor_right->getType() == VertexType::H_BOX)) {
            continue;
        }

        if(neighbor_left->getType() == VertexType::H_BOX){
            _matchTypeVec.push_back(neighbor_left);
            taken[n0] = true;
            taken[n1] = true;
            // missing : 判斷edge == H & next_neighbor == H
            vector<ZXVertex*> neighbors = neighbor_left->getNeighbors();
            size_t n2 = id2idx[neighbors[0]->getId()], n3 = id2idx[neighbors[1]->getId()];
            
            if (n2 != n0){
                taken[n2]=true;
            }
            else taken[n3]=true;

        }
        else if(neighbor_right->getType() == VertexType::H_BOX){
            _matchTypeVec.push_back(neighbor_right);
            taken[n0] = true;
            taken[n1] = true;
            vector<ZXVertex*> neighbors = neighbor_right->getNeighbors();
            size_t n2 = id2idx[neighbors[0]->getId()], n3 = id2idx[neighbors[1]->getId()];
            
            if (n2 != n0){
                taken[n2]=true;
            }
            else taken[n3]=true;
        }
        else if(neighbor_left->getType() != VertexType::H_BOX || neighbor_right->getType() != VertexType::H_BOX) {
            //cout << "But No H-box." << endl;
            continue;
        }
        // if(neighbor_left[0]->_phase != 1 && neighbor_right[0]->_phase != 1) continue;
    }

    

    if(verbose >= 3) cout << "Find match of hfuse-rule: " << _matchTypeVec.size() << endl;
    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        (Check PyZX/pyzx/hrules.py/fuse_hboxes for more details)
 * 
 * @param g 
 */
void HboxFusion::rewrite(ZXGraph* g){
    reset();
    //TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD

    // _removeVertices
    _removeVertices = _matchTypeVec;

    for(size_t i = 0; i < _matchTypeVec.size(); i++){

        vector<ZXVertex*> ns; 
        vector<EdgeType*> ets;

        // Phase check???
        // add power
        // Only two neighbors which is ensured
        for(auto& itr : _matchTypeVec[i]->getNeighborMap()){
            ns.push_back(itr.first);
            ets.push_back(itr.second);
        }

        // _removeEdges : Create Remove two edges from NeighborMap
        //EdgePair e0 = make_pair(make_pair(ns[0], _matchTypeVec[i]), ets[0]);
        //EdgePair e1 = make_pair(make_pair(ns[1], _matchTypeVec[i]), ets[1]);
        //_removeEdges.push_back(e0);
        //_removeEdges.push_back(e1);

        // _edgeTableKeys : add edge keys
        _edgeTableKeys.push_back(make_pair(ns[0], ns[1]));

        // _edgeTableValues : add edge type
        if(*ets[0] == *ets[1]) _edgeTableValues.push_back(make_pair(0,1));
        else _edgeTableValues.push_back(make_pair(1,0));

        ns.clear();
        ets.clear();
    }
}