// /****************************************************************************
//   FileName     [ extract.cpp ]
//   PackageName  [ extractor ]
//   Synopsis     [ graph extractor ]
//   Author       [ Chin-Yi Cheng ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include "extract.h"

bool Extractor::removeGadget(){
    PivotBoundary* pivotBoundaryRule = new PivotBoundary();
    vector<ZXVertex*> match;
    vector<vector<ZXVertex*>> matches;

    bool gadgetRemoved = false;
    for(auto n: _neighbors){
        if(! _graph -> isGadget(n)) continue;
        for(auto [candidate, _]: n->getNeighbors()){
            if(_frontiers.contains(candidate)){
                //FIXME - Ask
                match.push_back(n->getFirstNeighbor().first);
                match.push_back(candidate);
                matches.push_back(match);
                pivotBoundaryRule->setMatchTypeVec(matches);
                pivotBoundaryRule->rewrite(_graph);
                
                _frontiers.erase(candidate);
                _frontiers.emplace(n->getFirstNeighbor().first);
                //REVIEW - qubit_map
                gadgetRemoved = true;
                break;
            }
        }
    }
    return gadgetRemoved;
}

void Extractor::gaussianElimination(){
    M2 biAdjacency;
    biAdjacency.fromZXVertices(_frontiers, _neighbors);
    biAdjacency.gaussianElim(true);
    vector<Oper> cnots = biAdjacency.getOpers();
}