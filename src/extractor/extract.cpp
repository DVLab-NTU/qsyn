// /****************************************************************************
//   FileName     [ extract.cpp ]
//   PackageName  [ extractor ]
//   Synopsis     [ graph extractor ]
//   Author       [ Chin-Yi Cheng ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include "extract.h"
extern size_t verbose;
void Extractor::initialize(){
    vector<ZXVertex*> tmp;
    for(auto o: _graph -> getOutputs()){
        _frontiers.emplace(o->getFirstNeighbor().first);
    }
    _frontiers.sort([](const ZXVertex* a, const ZXVertex* b) {
        return a->getQubit() < b->getQubit();
    });
    updateNeighbors();

    for(auto v: _graph -> getVertices()){
        if(_graph->isGadget(v)){
            _axels.emplace(v->getFirstNeighbor().first);
        }
    }
    // printFroniters();
    // printNeighbors();
}

bool Extractor::removeGadget(){
    PivotBoundary* pivotBoundaryRule = new PivotBoundary();
    Simplifier simp(pivotBoundaryRule, _graph);
    if(verbose >=8) _graph->printVertices();
    if(verbose >=5){
        printFroniters();
        printAxels();
    }
    
    bool gadgetRemoved = false;
    for(auto n: _neighbors){
        if(! _axels.contains(n)) {
            continue;
        }
        for(auto [candidate, _]: n->getNeighbors()){
            if(_frontiers.contains(candidate)){
                vector<ZXVertex*> match;
                vector<vector<ZXVertex*>> matches;
                match.push_back(n);
                match.push_back(candidate);
                matches.push_back(match);
                
                pivotBoundaryRule->setMatchTypeVec(matches);
                
                ZXVertex* targetBoundary = nullptr;
                for(auto [boundary, _]: candidate->getNeighbors()){
                    if(boundary->getType() == VertexType::BOUNDARY){
                        pivotBoundaryRule->addBoudary(boundary);
                        targetBoundary = boundary;
                        break;
                    }
                }
                _axels.erase(n);
                _frontiers.erase(candidate);

                pivotBoundaryRule->rewrite(_graph);
                simp.amend();
               
                
                // n->setQubit(candidate->getQubit());
                if(targetBoundary != nullptr)
                    _frontiers.emplace(targetBoundary->getFirstNeighbor().first);
                //REVIEW - qubit_map
                gadgetRemoved = true;
                break;
            }
        }
    }
    if(verbose >=8) _graph->printVertices();
    if(verbose >=5){
        printFroniters();
        printAxels();
    }
    return gadgetRemoved;
}

void Extractor::gaussianElimination(){
    M2 biAdjacency;
    biAdjacency.fromZXVertices(_frontiers, _neighbors);
    biAdjacency.gaussianElim(true);
    vector<Oper> cnots = biAdjacency.getOpers();
}

void Extractor::updateNeighbors(){
    _neighbors.clear();
    for(auto f: _frontiers){
        for(auto [n, _]: f->getNeighbors()){
            if(n->getType() == VertexType::BOUNDARY) continue;
            if((! _neighbors.contains(n)) && (! _frontiers.contains(n))) 
                _neighbors.emplace(n);
        }
    }
}

void Extractor::printFroniters(){
    cout << "Frontiers:" << endl;
    for(auto f: _frontiers)
        cout << "Qubit:" << f->getQubit() << ": " << f->getId() << endl;
    cout << endl;
}

void Extractor::printNeighbors(){
    cout << "Neighbors:" << endl;
    for(auto n: _neighbors)
        cout << n->getId() << endl;
    cout << endl;
}

void Extractor::printAxels(){
    cout << "Axels:" << endl;
    for(auto n: _axels){
        cout << n->getId() << " (phase gadget: ";
        for(auto [pg, _]: n->getNeighbors()){
            if(_graph->isGadget(pg))
                cout << pg->getId() << ")" << endl;
        }
    }
    cout << endl;
}