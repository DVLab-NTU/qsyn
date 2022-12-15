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
    size_t cnt = 0;
    for(auto o: _graph -> getOutputs()){
        o->getFirstNeighbor().first->setQubit(o->getQubit());
        _frontier.emplace(o->getFirstNeighbor().first);
    }
    _frontier.sort([](const ZXVertex* a, const ZXVertex* b) {
        return a->getQubit() < b->getQubit();
    });
    for(auto o: _frontier){
        _qubitMap[o->getQubit()] = cnt;
        _circuit->addQubit(1);
        cnt+=1;
    }
    updateNeighbors();

    for(auto v: _graph -> getVertices()){
        if(_graph->isGadget(v)){
            _axels.emplace(v->getFirstNeighbor().first);
        }
    }
    if(verbose >=8){
        printFroniters();
        printNeighbors();
        _circuit -> printQubits();
    }
}

void Extractor::cleanFrontier(){
    //NOTE - Edge and Phase
    extractSingles();
    //NOTE - CZs
    extractCZs();
}

void Extractor::extractSingles(){
    vector<pair<ZXVertex*, ZXVertex*>> toggleList;
    for(ZXVertex* o: _graph->getOutputs()){
        if(o->getFirstNeighbor().second == EdgeType::HADAMARD){
            _circuit -> addGate("H", {_qubitMap[o->getQubit()]}, Phase(0), false);
            toggleList.emplace_back(o, o->getFirstNeighbor().first);
        }
        Phase ph = o->getFirstNeighbor().first -> getPhase();
        if(ph != Phase(0)){
            _circuit -> addSingleRZ(_qubitMap[o->getQubit()], ph, false);
            o->getFirstNeighbor().first -> setPhase(Phase(0));
        }
    }
    for(auto [s,t]: toggleList){
        _graph -> addEdge(s, t, EdgeType::SIMPLE);
        _graph -> removeEdge(s, t, EdgeType::HADAMARD);
    }

    if(verbose>=8){
        _circuit -> printQubits();
        _graph -> printQubits({});
    }
}

void Extractor::extractCZs(size_t strategy){
    vector<pair<ZXVertex*, ZXVertex*>> removeList;
    _frontier.sort([](const ZXVertex* a, const ZXVertex* b) {
        return a->getQubit() < b->getQubit();
    });
    for(auto itr = _frontier.begin(); itr != _frontier.end(); itr++){
        for(auto jtr = next(itr); jtr != _frontier.end(); jtr++){
            if((*itr)->isNeighbor((*jtr))){
                removeList.emplace_back((*itr),(*jtr));
            }
        }
    }
    for(const auto [s,t]: removeList){
        _graph -> removeEdge(s, t, EdgeType::HADAMARD);
        _circuit -> addGate("cz",{_qubitMap[s->getQubit()], _qubitMap[t->getQubit()]}, Phase(1), false);
    }

    //REVIEW - Other Strategies
    if(verbose>=8){
        _circuit -> printQubits();
        _graph -> printQubits({});
    }
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
            if(_frontier.contains(candidate)){
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
                _frontier.erase(candidate);

                pivotBoundaryRule->rewrite(_graph);
                simp.amend();
               
                
                // n->setQubit(candidate->getQubit());
                if(targetBoundary != nullptr)
                    _frontier.emplace(targetBoundary->getFirstNeighbor().first);
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
    _biAdjacency.fromZXVertices(_frontier, _neighbors);
    _biAdjacency.gaussianElim(true);
}

void Extractor::updateNeighbors(){
    _neighbors.clear();
    for(auto f: _frontier){
        for(auto [n, _]: f->getNeighbors()){
            if(n->getType() == VertexType::BOUNDARY) continue;
            if((! _neighbors.contains(n)) && (! _frontier.contains(n))) 
                _neighbors.emplace(n);
        }
    }
}

void Extractor::printFroniters(){
    cout << "Frontiers:" << endl;
    for(auto f: _frontier)
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