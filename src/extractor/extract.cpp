// /****************************************************************************
//   FileName     [ extract.cpp ]
//   PackageName  [ extractor ]
//   Synopsis     [ graph extractor ]
//   Author       [ Chin-Yi Cheng ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include "extract.h"
#include <algorithm>
#include <iterator>
extern size_t verbose;

/**
 * @brief Initialize the extractor. Set ZX-graph to QCir qubit map.
 * 
 */
void Extractor::initialize(){
    if(verbose >=5 ) cout << "Initialize" << endl;
    size_t cnt = 0;
    for(auto o: _graph -> getOutputs()){
        if(o->getFirstNeighbor().first->getType() != VertexType::BOUNDARY){
            o->getFirstNeighbor().first->setQubit(o->getQubit());
            _frontier.emplace(o->getFirstNeighbor().first);
        }
        _qubitMap[o->getQubit()] = cnt;
        _circuit->addQubit(1);
        cnt+=1; 
    }

    //NOTE - get zx to qc qubit mapping
    _frontier.sort([](const ZXVertex* a, const ZXVertex* b) {
        return a->getQubit() < b->getQubit();
    });

    updateNeighbors();
    for(auto v: _graph -> getVertices()){
        if(_graph->isGadget(v)){
            _axels.emplace(v->getFirstNeighbor().first);
        }
    }
    if(verbose >=8){
        printFroniter();
        printNeighbors();
        _graph -> printQubits({});
        _circuit -> printQubits();
    }
}

QCir* Extractor::extract(){
    while(true){
        cleanFrontier();
        updateNeighbors();
        if(_frontier.size() == 0){
            break;
        }
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
        
        if(!containSingleNeighbor()){
            if(verbose >=5) cout << "Perform Gaussian Elimination." << endl;
            gaussianElimination();
            updateGraphByMatrix();
            extractCXs();
        }
        else{
            if(verbose >=5) cout << "Construct an easy matrix." << endl;
            _biAdjacency.fromZXVertices(_frontier, _neighbors);
        }
        if(extractHsFromM2() == 0){
            cerr << "Error: No Candidate Found!!" << endl; 
            _biAdjacency.printMatrix();
            return nullptr;
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
    cout << "Finish extracting!" << endl;
    if(verbose >=8) {
        _circuit->printQubits();
        _graph->printQubits({});
    }
    permuteQubit();
    if(verbose >=8) {
        _circuit->printQubits();
        _graph->printQubits({});
    }
    
    return _circuit;
}

/**
 * @brief Clean frontier. which contains extract singles and CZs. Used in extract.
 * 
 */
void Extractor::cleanFrontier(){
    if(verbose>=3) cout << "Clean Frontier" << endl;
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
    if(verbose>=3) cout << "Extract Singles" << endl;
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
    if(verbose>=3) cout << "Extract CZs" << endl;
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
    if(verbose>=3) cout << "Extract CXs" << endl;
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
        if(verbose >= 5) cout << "Add CX: " << ctrl << " " << targ << endl;
        _circuit -> addGate("cx", {ctrl,targ}, Phase(0), false);
    }
}

/**
 * @brief Extract Hadamard if singly connected frontier is found
 * 
 * @return size_t 
 */
size_t Extractor::extractHsFromM2(){
    if(verbose>=3) cout << "Extract Hs" << endl;
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
                    frontNeighPairs.emplace_back(frontId2Vertex[row_cnt], neighId2Vertex[col]);
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
    if(verbose>=3) cout << "Remove Gadget" << endl;
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
               
                pivotBoundaryRule->clearBoundary();
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
 * @brief 
 * 
 */
void Extractor::columnOptimalSwap(){
    //NOTE - Swap columns of matrix and order of neighbors
    
    _rowInfo.clear();
    _colInfo.clear();
    
    size_t rowCnt = _biAdjacency.numRows();
    
    size_t colCnt = _biAdjacency.numCols();
    set<size_t> emptySet;
    
    for(size_t i=0; i<rowCnt; i++){
        _rowInfo[i] = emptySet;
    }
    for(size_t i=0; i<colCnt; i++){
        _colInfo[i] = emptySet;
    }

    for(size_t i=0; i<rowCnt; i++){
       
        for(size_t j=0; j<colCnt; j++){
            
            if(_biAdjacency.getMatrix()[i].getRow()[j] == 1){
                _rowInfo[i].emplace(j);
                _colInfo[j].emplace(i);
            }
        }
        
    }
    
    Target target;
    target = findColumnSwap(target);

    set<size_t> colSet, left, right, targKey, targVal;
    for(size_t i=0; i<colCnt; i++) colSet.emplace(i);
    for(auto& [k,v]: target){
        targKey.emplace(k);
        targVal.emplace(v);
    }

    set_difference(colSet.begin(), colSet.end(), targVal.begin(), targVal.end(), inserter(left, left.end()));
    set_difference(colSet.begin(), colSet.end(), targKey.begin(), targKey.end(), inserter(right, right.end()));
    vector<size_t> lvec(left.begin(), left.end());
    vector<size_t> rvec(right.begin(), right.end());
    for(size_t i=0; i<lvec.size(); i++){
        target[rvec[i]] = lvec[i];
    }
    Target perm;
    for(auto& [k, v]: target){
        perm[v] = k;
    }
    vector<ZXVertex*> nebVec(_neighbors.begin(), _neighbors.end());
    vector<ZXVertex*> newNebVec = nebVec;
    for(size_t i=0; i<nebVec.size(); i++){
        newNebVec[i] = nebVec[perm[i]];
    }
    _neighbors.clear();
    for(auto& v: newNebVec) _neighbors.emplace(v);
}

Target Extractor::findColumnSwap(Target target){
    //REVIEW - No need to copy in cpp
    size_t rowCnt = _rowInfo.size();
    size_t colCnt = _colInfo.size();

    set<size_t> claimedCols;
    set<size_t> claimedRows;
    for(auto& [key,value]: target){
        claimedCols.emplace(key);
        claimedRows.emplace(value);
    }

    while(true){
        int min_index = -1;
        set<size_t> min_options;
        for(size_t i=0; i<1000; i++) min_options.emplace(i);
        bool foundCol = false;
        for(size_t i=0; i<rowCnt; i++){
            if(claimedRows.contains(i)) continue;
            set<size_t> freeCols;
            //NOTE - find the free columns
            set_difference(_rowInfo[i].begin(), _rowInfo[i].end(), claimedCols.begin(), claimedCols.end(), inserter(freeCols, freeCols.end()));
            if(freeCols.size() == 1){
                //NOTE - pop the only element
                size_t j = *(freeCols.begin());
                freeCols.erase(j);

                target[j] = i;
                claimedCols.emplace(j);
                claimedRows.emplace(i);
                foundCol = true;
                break;
            }
                
            if(freeCols.size() == 0) {
                cout << "No Free Column!!" << endl;
                Target t;
                return t; //NOTE - Contradiction
            }

            for(auto& j: freeCols){
                set<size_t> freeRows;
                set_difference(_colInfo[j].begin(), _colInfo[j].end(), claimedRows.begin(), claimedRows.end(), inserter(freeRows, freeRows.end()));
                if(freeRows.size() == 1){
                    target[j] = i; //NOTE - j can only be connected to i
                    claimedCols.emplace(j);
                    claimedRows.emplace(i);
                    foundCol = true;
                    break;
                }  
            }     
            if(foundCol) break;
            if(freeCols.size() < min_options.size()){
                min_index = i;
                min_options = freeCols;
            }
        }

        if(!foundCol){
            bool done = true;
            for(auto& [r, _]: _rowInfo){
                if(!claimedRows.contains(r)){
                    done = false;
                    break;
                }
            }
            if(done){
                return target;
            }
            if(min_index == -1){
                cerr << "Error: this shouldn't happen ever" << endl;
                assert(false);
            }
            //NOTE -  depth-first search
            //REVIEW - no need to copy in cpp -> pass by value
            Target copiedTarget = target;
            // tgt = target.copy()
            if(verbose >=8) cout << "Backtracking on " << min_index << endl;

            for(auto& idx: min_options){
                if(verbose >=8) cout << "> trying option" << idx << endl;
                copiedTarget[idx] = min_index;
                Target newTarget = findColumnSwap(copiedTarget);
                if (newTarget.size()>0)
                    return newTarget;
            }
            if(verbose >=8) cout << "Unsuccessful" << endl;
            return target;
        }
    }
}


/**
 * @brief Perform Gaussian Elimination
 * 
 */
void Extractor::gaussianElimination(){
    _biAdjacency.fromZXVertices(_frontier, _neighbors);
    //_biAdjacency.printMatrix();
    //cout << "----- columnOptimalSwap -----" << endl;
    columnOptimalSwap();
    _biAdjacency.fromZXVertices(_frontier, _neighbors);
    //_biAdjacency.printMatrix();
    // _biAdjacency.gaussianElimPyZX();
    _biAdjacency.gaussianElim(true);
    _cnots = _biAdjacency.getOpers();
}

/**
 * @brief Permute qubit if input and output are not match
 * 
 */
void Extractor::permuteQubit(){
    if(verbose>=3) cout << "Permute Qubit" << endl;
    unordered_map<size_t, size_t> swapMap; // o to i
    unordered_map<size_t, size_t> swapInvMap; // i to o
    bool unmatched = false;
    for(auto& o: _graph -> getOutputs()){
        ZXVertex* i = o -> getFirstNeighbor().first;
        assert(_graph -> getInputs().contains(i));
        if(i->getQubit() != o->getQubit()){
            unmatched = true;
        }
        swapMap[o->getQubit()] = i->getQubit();
    }
    if(unmatched){
        for(auto& [o,i]: swapMap){
            swapInvMap[i] = o;
        }
        for(auto& [o,i]: swapMap){
            if(o == i) continue;
            size_t t1 = i;
            size_t t2 = swapInvMap[o];
            //NOTE - SWAP
            _circuit -> addGate("cx",{_qubitMap[o], _qubitMap[t2]},Phase(0),false);
            _circuit -> addGate("cx",{_qubitMap[t2], _qubitMap[o]},Phase(0),false);
            _circuit -> addGate("cx",{_qubitMap[o], _qubitMap[t2]},Phase(0),false);
            //swaps.append((o,t2))
            swapMap[t2] = t1;
            swapInvMap[t1] = t2;
        }
    }
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
        _graph->addEdge(v->getFirstNeighbor().first, v->getSecondNeighbor().first, EdgeType::SIMPLE);
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
    if(verbose>=3) cout << "Update Graph by Matrix" << endl;
    for(auto rowItr = _frontier.begin(); rowItr!= _frontier.end(); rowItr++){
        size_t colcnt = 0;
        for(auto colItr = _neighbors.begin(); colItr!= _neighbors.end(); colItr++){
            if(_biAdjacency.getMatrix()[rowcnt].getRow()[colcnt] == 1){
                //NOTE - Should Connect
                if(!(*rowItr)->isNeighbor(*colItr)){
                    //NOTE - But Not Connect
                    _graph->addEdge(*rowItr, *colItr, et);
                }
            }
            else{
                //NOTE - Should Not Connect
                if((*rowItr)->isNeighbor(*colItr)){
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