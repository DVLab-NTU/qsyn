// /****************************************************************************
//   FileName     [ extract.cpp ]
//   PackageName  [ extractor ]
//   Synopsis     [ graph extractor ]
//   Author       [ Chin-Yi Cheng ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include "extract.h"
extern size_t verbose;

/**
 * @brief Initialize the extractor. Set ZX-graph to QCir qubit map.
 * 
 */
void Extractor::initialize(){
    for(auto o: _graph -> getOutputs()){
        o->getFirstNeighbor().first->setQubit(o->getQubit());
        _frontier.emplace(o->getFirstNeighbor().first);
    }

    //NOTE - get zx to qc qubit mapping
    _frontier.sort([](const ZXVertex* a, const ZXVertex* b) {
        return a->getQubit() < b->getQubit();
    });

    size_t cnt = 0;
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
        printFroniter();
        printNeighbors();
        _circuit -> printQubits();
    }
}

bool Extractor::extract(){
    while(true){
        cleanFrontier();
        if(removeGadget()){
            if(verbose >=5) cout << "Gadget(s) are removed." << endl;
            if(verbose >=8){
                printFroniter();
                _graph->printQubits({});
                _circuit -> printQubits();
            }
            continue;
        }
        updateNeighbors();
        if(_frontier.size() == 0){
            cout << "Finish extracting!" << endl;
            _circuit->printQubits();
            _graph->printQubits({});
            break;
        }
        if(!containSingleNeighbor()){
            if(verbose >=5) cout << "Perform Gaussian Elimination." << endl;
            gaussianElimination();
            extractCXs();
        }
        else{
            if(verbose >=5) cout << "Construct an easy matrix." << endl;
            _biAdjacency.fromZXVertices(_frontier, _neighbors);
        }
        if(extractHsFromM2() == 0){
            cerr << "Error: No Candidate Found!!" << endl; 
            _circuit->printQubits();
            _graph->printQubits({});
            return false;
        }
        _biAdjacency.reset();
        _cnots.clear();

        if(verbose >=8){
            printFroniter();
            printNeighbors();
            _graph -> printQubits({});
            _circuit -> printQubits();
        }
    }
    return true;
}

/**
 * @brief Clean frontier. which contains extract singles and CZs. Used in extract.
 * 
 */
void Extractor::cleanFrontier(){
    //NOTE - Edge and Phase
    extractSingles();
    //NOTE - CZs
    extractCZs();
}

/**
 * @brief Extract single qubit gates, i.e. rz family and H. Used in clean frontier.
 * 
 */
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

/**
 * @brief Extract CZs from frontier. Used in clean frontier.
 * 
 * @param strategy (0: directly extract) 
 */
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

/**
 * @brief Extract CXs
 * 
 * @param strategy (0: directly by result of Gaussian Elimination) 
 */
void Extractor::extractCXs(size_t strategy){
    unordered_map<size_t, ZXVertex*> frontId2Vertex;
    size_t cnt = 0;
    for(auto& f: _frontier){
        frontId2Vertex[cnt] = f;
        cnt++;
    }
    for(auto& [t,c]: _cnots){
        //NOTE - targ and ctrl are opposite here
        size_t ctrl = _qubitMap[frontId2Vertex[c] -> getQubit()];
        size_t targ = _qubitMap[frontId2Vertex[t] -> getQubit()];
        _circuit -> addGate("cx", {ctrl,targ}, Phase(0), false);
    }
}

/**
 * @brief Extract Hadamard if singly connected frontier is found
 * 
 * @return size_t 
 */
size_t Extractor::extractHsFromM2(){
    unordered_map<size_t, ZXVertex*> frontId2Vertex;
    unordered_map<size_t, ZXVertex*> neighId2Vertex;
    size_t cnt = 0;
    for(auto& f: _frontier){
        frontId2Vertex[cnt] = f;
        cnt++;
    }
    cnt = 0;
    for(auto& n: _neighbors){
        neighId2Vertex[cnt] = n;
        cnt++;
    }

    //NOTE - Store pairs to be modified
    vector<pair<ZXVertex*, ZXVertex*>> frontNeighPairs;
    size_t row_cnt = 0;
    for(auto& row: _biAdjacency.getMatrix()){
        if(row.isOneHot()){
            for(size_t col=0; col<row.size(); col++){
                if(row[col] == 1){
                    frontNeighPairs.emplace_back(frontId2Vertex[row_cnt], neighId2Vertex[row_cnt]);
                    break;
                }
            }
        }
        row_cnt++;
    }

    for(auto& [f, n]: frontNeighPairs){
        //NOTE - Add Hadamard according to the v of frontier (row)
        _circuit -> addGate("h", {_qubitMap[f -> getQubit()]}, Phase(0), false);
        //NOTE - Set #qubit according to the old frontier
        n -> setQubit(f -> getQubit());

        //NOTE - Connect edge between boundary and neighbor
        for(auto& [bound, ep]: f->getNeighbors()){
            if(bound -> getType() != VertexType::BOUNDARY) continue;
            else{
                _graph -> addEdge(bound, n, ep);
                break;
            }
        }  
        //NOTE - Replace frontier by neighbor      
        _frontier.erase(f);
        _frontier.emplace(n);
        _graph -> removeVertex(f);
    }
    return frontNeighPairs.size();
}

/**
 * @brief Remove gadget according to Pivot Boundary Rule 
 * 
 * @return true if gadget(s) are removed 
 * @return false if not
 */
bool Extractor::removeGadget(){
    PivotBoundary* pivotBoundaryRule = new PivotBoundary();
    Simplifier simp(pivotBoundaryRule, _graph);
    if(verbose >=8) _graph->printVertices();
    if(verbose >=5){
        printFroniter();
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
        printFroniter();
        printAxels();
    }
    return gadgetRemoved;
}

/**
 * @brief Perform Gaussian Elimination
 * 
 */
void Extractor::gaussianElimination(){
    _biAdjacency.fromZXVertices(_frontier, _neighbors);
    _biAdjacency.gaussianElim(true);
    _cnots = _biAdjacency.getOpers();
}

void Extractor::updateFrontier(bool sort){
    
}

/**
 * @brief Update _neighbors according to _frontier
 * 
 */
void Extractor::updateNeighbors(){
    _neighbors.clear();
    vector<ZXVertex*> rmVs;
    
    for(auto& f: _frontier){
        size_t hasBound = 0;
        for(auto [n, _]: f->getNeighbors()){
            if(n->getType() == VertexType::BOUNDARY){
                hasBound++;
            }
        }

        if(hasBound == 2){
            if(f->getNumNeighbors() == 2){
                //NOTE - Remove
                
                for(auto [b, ep]: f->getNeighbors()){
                    if(_graph->getInputs().contains(b)){
                        if(ep == EdgeType::HADAMARD)
                            _circuit->addGate("h", {_qubitMap[f->getQubit()]}, Phase(0), false);
                        break;
                    }
                }
                rmVs.push_back(f);
            }
            else{
                for(auto [b, ep]: f->getNeighbors()){
                    if(_graph->getInputs().contains(b)){
                        ZXVertex* nV = _graph->addVertex(b->getQubit(), VertexType::Z, Phase(0));
                        _graph->addEdge(nV, f, EdgeType::HADAMARD);
                        _graph->addEdge(nV, b, toggleEdge(ep));
                        _graph->removeEdge(b, f, ep);
                        break;
                    }
                }
            }
        }
    }
    for(auto& v: rmVs){
        if(verbose>=8) cout << "Remove " << v->getId() << " (q" << v->getQubit() << ") from frontiers." << endl; 
        _frontier.erase(v);
        _graph->removeVertex(v);
    }

    for(auto& f: _frontier){
        for(auto [n, _]: f->getNeighbors()){
            if(n->getType() == VertexType::BOUNDARY) continue;
            if((! _neighbors.contains(n)) && (! _frontier.contains(n))) 
                _neighbors.emplace(n);
        }
    }
}

/**
 * @brief Update _graph according to _biAdjaency
 * 
 * @param et EdgeType, default: EdgeType::HADAMARD
 */
void Extractor::updateGraphByMatrix(EdgeType et){
    size_t rowcnt = 0;
    for(auto rowItr = _frontier.begin(); rowItr!= _frontier.end(); rowItr++){
        size_t colcnt = 0;
        for(auto colItr = _neighbors.begin(); colItr!= _neighbors.end(); colItr++){
            if(_biAdjacency.getMatrix()[rowcnt].getRow()[colcnt] == '1'){
                //NOTE - Should Connect
                if(!(*rowItr)->isNeighbor(*colItr)){
                    //NOTE - But Not Connect
                    _graph->addEdge(*rowItr, *colItr, et);
                }
            }
            else{
                //NOTE - Should Not Connect
                if(!(*rowItr)->isNeighbor(*colItr)){
                    //NOTE - But Connect
                    _graph->removeEdge(*rowItr, *colItr, et);
                }
            }
            colcnt++;
        }
        rowcnt++;
    }
}

/**
 * @brief Frontier contains a vertex only a single neighbor (boundary excluded).
 * 
 * @return true 
 * @return false 
 */
bool Extractor::containSingleNeighbor(){
    for(auto& f: _frontier){
        if(f->getNumNeighbors() == 2) 
            return true;
    }
    return false;
}

void Extractor::printFroniter(){
    cout << "Frontier:" << endl;
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