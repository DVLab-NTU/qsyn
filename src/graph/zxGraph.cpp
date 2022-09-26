/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include "zxGraph.h"
#include "util.h"

using namespace std;
extern size_t verbose;

EdgeType toggleEdge(EdgeType et){
    if(et == EdgeType::SIMPLE) return EdgeType::HADAMARD;
    else if(et == EdgeType::HADAMARD) return EdgeType::SIMPLE;
    else return EdgeType::ERRORTYPE;
}

VertexType str2VertexType(string str){
    if(str == "BOUNDARY") return VertexType::BOUNDARY;
    else if (str == "Z") return VertexType::Z;
    else if (str == "X") return VertexType::X;
    else if (str == "H_BOX") return VertexType::H_BOX;
    return VertexType::ERRORTYPE;
}

string VertexType2Str(VertexType vt){
    if(vt == VertexType::X) return "\033[1;31mX\033[0m";
    if(vt == VertexType::Z) return "\033[1;32mZ\033[0m";
    if(vt == VertexType::H_BOX) return "\033[1;33mH\033[0m";
    if(vt == VertexType::BOUNDARY) return "●";
    return "";
}

EdgeType* str2EdgeType(string str){
    if(str == "SIMPLE") return new EdgeType(EdgeType::SIMPLE);
    else if(str == "HADAMARD") return new EdgeType(EdgeType::HADAMARD);
    return new EdgeType(EdgeType::ERRORTYPE);
}

string EdgeType2Str(EdgeType* et){
    if(*et == EdgeType::SIMPLE) return "-";
    if(*et == EdgeType::HADAMARD) return "\033[1;34mH\033[0m";
    return "";
}

/**************************************/
/*   class ZXVertex member functions   */
/**************************************/


// Getter and Setter
// vector<NeighborPair > ZXVertex::getNeighborById(size_t id) const{
//     vector<NeighborPair > nList;
//     if(!isNeighborById(id)) cerr << "Error: Vertex " << id << " is not a neighbor of " << _id << endl;
//     else{
//          for(size_t i = 0; i < _neighbors.size(); i++){
//              if(_neighbors[i].first->getId() == id) nList.push_back(_neighbors[i]);
//          }
//     }
//     return nList;
// }

vector<NeighborPair > ZXVertex::getNeighborByPointer(ZXVertex* v) const{
    vector<NeighborPair > nList;
    if(!isNeighbor(v)) cerr << "Error: Vertex " << v->getId() << " is not a neighbor of " << _id << endl;
    else{
        //! TODO
        //  for(size_t i = 0; i < _neighbors.size(); i++){
        //      if(_neighbors[i].first == v) nList.push_back(_neighbors[i]);
        //  }
    }
    return nList;
}

// Add and Remove

// void ZXVertex::removeNeighbor(NeighborPair neighbor){
//     //! Only remove the first find neighbor pair
//     auto findNeighbor = find(_neighbors.begin(), _neighbors.end(), neighbor);
//     if(findNeighbor != _neighbors.end()){
//         if(verbose >= 7) cout << "  * remove " << neighbor.first->getId() << " from " << _id << endl;
//         _neighbors.erase(findNeighbor);
//     } 
//     else cerr << "Error: Vertex " << neighbor.first->getId() << " is not a neighbor of " << _id << endl;
// }

// void ZXVertex::removeNeighborById(size_t id){
//     //! Remove all neighbor pair if key == id
//     if(!isNeighborById(id)) cerr << "Error: Vertex " << id << " is not a neighbor of " << _id << endl;
//     else{
//         for(size_t i = 0; i < _neighbors.size();){
//             if(_neighbors[i].first->getId() == id) {
//                 _neighbors.erase(_neighbors.begin()+i);
//                 if(verbose >= 7) cout << "  * remove " << id << " from " << _id << endl;
//             }
//             else i++;
//         }
//     }
// }


// Print functions

void ZXVertex::printVertex() const{
    cout << "ID:\t" << _id << "\t";
    cout << "VertexType:\t" << VertexType2Str(_type) << "\t";
    cout << "Qubit:\t" << _qubit << "\t";
    cout << "Phase:\t" << _phase << "\t";
    // cout << "#Neighbors:\t" << _neighbors.size() << "\t";
    cout << "#Neighbors:\t" << _neighborMap.size() << "\t";
    // printNeighbors();
    printNeighborMap();
}

// void ZXVertex::printNeighbors() const{
//     for(size_t i = 0; i < _neighbors.size(); i++){
//         cout << "(" << _neighbors[i].first->getId() << ", " << EdgeType2Str(_neighbors[i].second) << ") " ;
//     }
//     cout << endl;
// }

void ZXVertex::printNeighborMap() const{
    for(auto itr = _neighborMap.begin(); itr != _neighborMap.end(); itr++){
        cout << "(" << itr->first->getId() << ", " << EdgeType2Str(itr->second) << ") " ;
    }
    cout << endl;
}

// Action
/**
 * @brief Remove all the connection between `this` and `v`.
 * 
 * @param v 
 * @param checked 
 */
void ZXVertex::disconnect(ZXVertex* v, bool checked){
    if(!checked){
        if(!isNeighbor(v)){
            cerr << "Error: Vertex " << v->getId() << " is not a neighbor of " << _id << endl;
            return;
        }
    }

    _neighborMap.erase(v);
    NeighborMap nMap = v->getNeighborMap();
    nMap.erase(this);
    v->setNeighborMap(nMap);
}

// void ZXVertex::disconnectById(size_t id){
//     if(!isNeighborById(id)) cerr << "Error: Vertex " << id << " is not a neighbor of " << _id << endl;
//     else{
//         ZXVertex* v = nullptr;
//         for(size_t i = 0; i < _neighbors.size();){
//             if(_neighbors[i].first->getId() == id){
//                 if(v == nullptr) v = _neighbors[i].first;
//                 _neighbors.erase(_neighbors.begin()+i);
//             } 
//             else i++;
//         }
//         vector<NeighborPair> nList = v->getNeighbors();
//         for(size_t i = 0; i < nList.size();){
//             if(nList[i].first == this) nList.erase(nList.begin()+i);
//             else i++;
//         }
//         v->setNeighbors(nList);
//         if(verbose >= 5) cout << "Disconnect " << _id << " and " << id << endl;
//     }
// }

// void ZXVertex::connect(ZXVertex* v, EdgeType* et){
//     addNeighbor(make_pair(v, et));
//     rearrange();
//     v->addNeighbor(make_pair(this, et));
//     v->rearrange();
//     if(verbose >= 7) cout << _id << " and " << v->getId() << " add each other to _neighbor." << endl;
// }

// void ZXVertex::rearrange(){
//     sort(_neighbors.begin(), _neighbors.end());
// }

// Test
bool ZXVertex::isNeighbor(ZXVertex* v) const{
    auto itr = _neighborMap.find(v);
    if(itr != _neighborMap.end()) return true;
    else return false;
}

// bool ZXVertex::isNeighborById(size_t id) const{
//     for(size_t i = 0; i < _neighbors.size(); i++){
//         if(_neighbors[i].first->getId() == id) return true;
//     }
//     return false;
// }


/**************************************/
/*   class ZXGraph member functions   */
/**************************************/

// For testing
void ZXGraph::generateCNOT(){
    cout << "Generate a 2-qubit CNOT graph for testing" << endl;
    // Generate Inputs
    vector<ZXVertex*> inputs, outputs, vertices;
    for(size_t i = 0; i < 2; i++){
        ZXVertex* input = new ZXVertex(vertices.size(), i,  VertexType::BOUNDARY);
        inputs.push_back(input);
        vertices.push_back(input);
    }

    // Generate CNOT
    ZXVertex* z_spider = new ZXVertex(vertices.size(), 0,  VertexType::Z);
    vertices.push_back(z_spider);
    ZXVertex* x_spider = new ZXVertex(vertices.size(), 1,  VertexType::X);
    vertices.push_back(x_spider);

    // Generate Outputs
    for(size_t i = 0; i < 2; i++){
        ZXVertex* output = new ZXVertex(vertices.size(), i,  VertexType::BOUNDARY);
        outputs.push_back(output);
        vertices.push_back(output);
    }
    setInputs(inputs);
    setOutputs(outputs);
    setVertices(vertices);

    // Generate edges [(0,2), (1,3), (2,3), (2,4), (3,5)]
    vector<pair<size_t, size_t> > edgeList;
    edgeList.push_back(make_pair(3,5));
    edgeList.push_back(make_pair(0,2));
    edgeList.push_back(make_pair(1,3));
    edgeList.push_back(make_pair(2,3));
    edgeList.push_back(make_pair(2,4));
    
    vector<EdgePair > edges;

    for(size_t i = 0; i < edgeList.size(); i++){
        if(findVertexById(edgeList[i].first) != nullptr && findVertexById(edgeList[i].second) != nullptr){
            ZXVertex* s = findVertexById(edgeList[i].first); ZXVertex* t = findVertexById(edgeList[i].second);
            EdgeType* et = new EdgeType(EdgeType::SIMPLE);
            

            // NeighborMap
            addEdge(s, t, et);
            // Original
            // s->addNeighbor(make_pair(t, et));
            // t->addNeighbor(make_pair(s, et));
            // edges.push_back(make_pair(make_pair(s, t), et));
        }
    }
    // setEdges(edges);

    //TODO: rearrange
    // for(size_t i = 0; i < _vertices.size(); i++) _vertices[i]->rearrange();
}

bool ZXGraph::isEmpty() const{
    if(_inputs.empty() && _outputs.empty() && _vertices.empty() && _edges.empty()) return true;
    return false;
}

bool ZXGraph::isValid() const{
    for(size_t i = 0; i < _edges.size(); i++){
        if(!_edges[i].first.first->isNeighbor(_edges[i].first.second) || 
           !_edges[i].first.second->isNeighbor(_edges[i].first.first))  return false;
    }
    return true;
}

bool ZXGraph::isConnected(ZXVertex* v1, ZXVertex* v2) const{
    if(v1->isNeighbor(v2) && v2->isNeighbor(v1)) return true;
    return false;
}

bool ZXGraph::isId(size_t id) const{
    for(size_t i = 0; i < _vertices.size(); i++){
        if(_vertices[i]->getId() == id) return true;
    }
    return false;
}

bool ZXGraph::isInputQubit(int qubit) const{
    for(size_t i = 0; i < _inputs.size(); i++){
        if(_inputs[i]->getQubit() == qubit) return true;
    }
    return false;
}

bool ZXGraph::isOutputQubit(int qubit) const{
    for(size_t i = 0; i < _outputs.size(); i++){
        if(_outputs[i]->getQubit() == qubit) return true;
    }
    return false;
}


// Add and Remove
ZXVertex* ZXGraph::addInput(size_t id, int qubit){
    if(isId(id)){
        cerr << "Error: This vertex id is already existed!!" << endl;
        return nullptr;
    } 
    else if(isInputQubit(qubit)){
        cerr << "Error: This qubit's input is already existed!!" << endl;
        return nullptr;
    } 
    else{
        ZXVertex* v = new ZXVertex(id, qubit, VertexType::BOUNDARY);
        _inputs.push_back(v);
        _vertices.push_back(v);
        if(verbose >= 3) cout << "Add input " << id << endl;
        return v;
    }
    
}

ZXVertex* ZXGraph::addOutput(size_t id, int qubit){
    if(isId(id)){
        cerr << "Error: This vertex id is already existed!!" << endl;
        return nullptr;
    } 
    else if(isOutputQubit(qubit)){
        cerr << "Error: This qubit's output is already existed!!" << endl;
        return nullptr;
    } 
    else{
        ZXVertex* v = new ZXVertex(id, qubit, VertexType::BOUNDARY);
        _vertices.push_back(v);
        _outputs.push_back(v);
        if(verbose >= 3) cout << "Add output " << id << endl;
        return v;
    }
}

ZXVertex* ZXGraph::addVertex(size_t id, int qubit, VertexType vt, Phase phase){
    if(isId(id)){
        cerr << "Error: This vertex id is already exist!!" << endl;
        return nullptr;
    } 
    else if(vt == VertexType::BOUNDARY){
        cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
        return nullptr;
    } 
    else{
        ZXVertex* v = new ZXVertex(id, qubit, vt, phase);
        _vertices.push_back(v);
        if(verbose >= 3) cout << "Add vertice " << id << endl;
        return v;
    }
}

EdgePair ZXGraph::addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType* et){
    // NeighborMap mode
    vs->addNeighbor(make_pair(vt, et));
    vt->addNeighbor(make_pair(vs, et));


    // Original
    // vs->connect(vt, et);
    EdgePair e = make_pair(make_pair(vs, vt), et);
    _edges.push_back(e);
    if(verbose >= 3) cout << "Add edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
    return e;
}

void ZXGraph::addEdgeById(size_t id_s, size_t id_t, EdgeType* et){
    if(!isId(id_s)) cerr << "Error: id_s provided is not exist!" << endl;
    else if(!isId(id_t)) cerr << "Error: id_t provided is not exist!" << endl;
    else{
        
        if(verbose >= 3) cout << "Add edge ( " << id_s << ", " << id_t << " )" << endl;
        ZXVertex* vs = findVertexById(id_s); ZXVertex* vt = findVertexById(id_t);
        // NeighborMap mode
        vs->addNeighbor(make_pair(vt, et));
        vt->addNeighbor(make_pair(vs, et));


        // Original
        // vs->addNeighbor(make_pair(vt, et)); vt->addNeighbor(make_pair(vs, et));
        _edges.push_back(make_pair(make_pair(vs, vt), et));
    }
}

void ZXGraph::addInputs(vector<ZXVertex*> inputs){
    _inputs.insert(_inputs.end(), inputs.begin(), inputs.end());
}

void ZXGraph::addOutputs(vector<ZXVertex*> outputs){
    _outputs.insert(_outputs.end(), outputs.begin(), outputs.end());
}

void ZXGraph::addVertices(vector<ZXVertex*> vertices){
    _vertices.insert(_vertices.end(), vertices.begin(), vertices.end());
}

void ZXGraph::addEdges(vector<EdgePair> edges){
     _edges.insert(_edges.end(), edges.begin(), edges.end());
}

void ZXGraph::removeVertex(ZXVertex* v, bool checked){
    if(!checked){
        if(!isId(v->getId())){
            cerr << "This vertex is not existed!" << endl;
            return;
        } 
    } 
    
    if(verbose >= 3) cout << "Remove ID: " << v->getId() << endl;
    // Remove all neighbors' records
    //! TODO

    // Check if also in _inputs or _outputs
    if(find(_inputs.begin(), _inputs.end(), v) != _inputs.end()) _inputs.erase(find(_inputs.begin(), _inputs.end(), v));
    if(find(_outputs.begin(), _outputs.end(), v) != _outputs.end()) _outputs.erase(find(_outputs.begin(), _outputs.end(), v));

    // Check _edges
    for(size_t i = 0; i < _edges.size();){
        if(_edges[i].first.first == v || _edges[i].first.second == v) removeEdge(_edges[i].first.first, _edges[i].first.second, true);
        else i++;
    }

    // Check _vertices
    _vertices.erase(find(_vertices.begin(), _vertices.end(), v));

    // deallocate ZXVertex
    delete v;
}

void ZXGraph::removeVertices(vector<ZXVertex* > vertices){
    for(size_t i = 0; i < vertices.size(); i++){
        removeVertex(vertices[i]);
    }
}

void ZXGraph::removeVertexById(size_t id){
    auto v = findVertexById(id);
    if(v != nullptr) removeVertex(v, true);
    else cerr << "Error: This vertex id is not existed!!" << endl;
}

void ZXGraph::removeIsolatedVertices(){
    for(size_t i = 0; i < _vertices.size(); ){
        if(_vertices[i]->getNeighborMap().empty()){
            removeVertex(_vertices[i]);
        }
        else i++;
    }
}
/**
 * @brief Remove all edges between `vs` and `vt`.
 * 
 * @param vs 
 * @param vt 
 * @param checked 
 */
void ZXGraph::removeEdge(ZXVertex* vs, ZXVertex* vt, bool checked){
    if(!checked){
        if(!vs->isNeighbor(vt) || !vt->isNeighbor(vs)){
            cerr << "Error: Vertex "<< vs->getId() << " and " << vt->getId() << " are not connected!" << endl;
            return;
        } 
    }
    for(size_t i = 0; i < _edges.size();){
        if((_edges[i].first.first == vs && _edges[i].first.second == vt) || (_edges[i].first.first == vt && _edges[i].first.second == vs)){
            _edges.erase(_edges.begin()+i);
        } 
        else i++;
    }
    vs->disconnect(vt, true);
    
    if(verbose >= 5) cout << "Remove edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
}

void ZXGraph::removeEdgeById(size_t id_s, size_t id_t){
    if(!isId(id_s)) cerr << "Error: id_s provided is not existed!" << endl;
    else if(!isId(id_t)) cerr << "Error: id_t provided is not existed!" << endl;
    else{
        ZXVertex* vs = findVertexById(id_s); ZXVertex* vt = findVertexById(id_t);
        removeEdge(vs, vt);
    }
}


// Find functions
ZXVertex* ZXGraph::findVertexById(size_t id) const{
    for(size_t i = 0; i < _vertices.size(); i++){
        if(_vertices[i]->getId() == id) return _vertices[i];
    }
    return nullptr;
}

size_t ZXGraph::findNextId() const{
    size_t nextId = 0;
    for(size_t i = 0; i < _vertices.size(); i++){
        if(_vertices[i]->getId() >= nextId) nextId = _vertices[i]->getId() + 1;
    }
    return nextId;
}


// Action
void ZXGraph::reset(){
    // for(size_t i = 0; i < _vertices.size(); i++) delete _vertices[i];
    // for(size_t i = 0; i < _topoOrder.size(); i++) delete _topoOrder[i];
    _inputs.clear();
    _outputs.clear();
    _vertices.clear();
    _edges.clear();
    _inputList.clear();
    _outputList.clear();
    _topoOrder.clear();
    _globalDFScounter = 1;
}

ZXGraph* ZXGraph::copy() const{
    //! Check if EdgeType change simultaneously
    ZXGraph* newGraph = new ZXGraph(0);

    newGraph->setId(getId());

    vector<ZXVertex*> inputs, outputs, vertices;
    vector<EdgePair > edges;

    // new Inputs
    for(size_t i = 0; i < getInputs().size(); i++){
        ZXVertex* oriVertex = getInputs()[i];
        ZXVertex* newVertex = new ZXVertex(oriVertex->getId(), oriVertex->getQubit(), oriVertex->getType());
        inputs.push_back(newVertex);
        vertices.push_back(newVertex);
    }
    newGraph->setInputs(inputs);

    // new Outputs
    for(size_t i = 0; i < getOutputs().size(); i++){
        ZXVertex* oriVertex = getOutputs()[i];
        ZXVertex* newVertex = new ZXVertex(oriVertex->getId(), oriVertex->getQubit(), oriVertex->getType());
        outputs.push_back(newVertex);
        vertices.push_back(newVertex);
    }
    newGraph->setOutputs(outputs);

    // new Vertices (without I/O)
    for(size_t i = 0; i < getVertices().size(); i++){
        if(getVertices()[i]->getType() != VertexType::BOUNDARY){
            ZXVertex* oriVertex = getVertices()[i];
            ZXVertex* newVertex = new ZXVertex(oriVertex->getId(), oriVertex->getQubit(), oriVertex->getType());
            vertices.push_back(newVertex);
        }
    }
    newGraph->setVertices(vertices);

    for(size_t i = 0; i < getEdges().size(); i++){
        EdgePair oriPair = getEdges()[i];
        ZXVertex* s = newGraph->findVertexById(oriPair.first.first->getId());
        ZXVertex* t = newGraph->findVertexById(oriPair.first.second->getId());
        s->addNeighbor(make_pair(t, oriPair.second));
        t->addNeighbor(make_pair(s, oriPair.second));
        edges.push_back(make_pair(make_pair(s, t), oriPair.second));
    }
    newGraph->setEdges(edges);
    return newGraph;
}

void ZXGraph::sortIOByQubit(){
    sort(_inputs.begin(), _inputs.end(), [](ZXVertex* a, ZXVertex* b){ return a->getQubit() < b->getQubit(); });
    sort(_outputs.begin(), _outputs.end(), [](ZXVertex* a, ZXVertex* b){ return a->getQubit() < b->getQubit(); });
}

void ZXGraph::sortVerticeById(){
    sort(_vertices.begin(), _vertices.end(), [](ZXVertex* a, ZXVertex* b){ return a->getId() < b->getId(); });
}



// Print functions
void ZXGraph::printGraph() const{
    cout << "Graph " << _id << endl;
    cout << setw(15) << left << "Inputs: " << _inputs.size() << endl;
    cout << setw(15) << left << "Outputs: " << _outputs.size() << endl;
    cout << setw(15) << left << "Vertices: " << _vertices.size() << endl;
    cout << setw(15) << left << "Edges: " << _edges.size() << endl;
}

void ZXGraph::printInputs() const{
    for(size_t i = 0; i < _inputs.size(); i++){
        cout << "Input " << i+1 << setw(8) << left << ":" << _inputs[i]->getId() << endl;
    }
    cout << "Total #Inputs: " << _inputs.size() << endl;
}

void ZXGraph::printOutputs() const{
    for(size_t i = 0; i < _outputs.size(); i++){
        cout << "Output " << i+1 << setw(7) << left << ":" << _outputs[i]->getId() << endl;
    }
    cout << "Total #Outputs: " << _outputs.size() << endl;
}

void ZXGraph::printVertices() const{
    for(size_t i = 0; i < _vertices.size(); i++){
        _vertices[i]->printVertex();
    }
    cout << "Total #Vertices: " << _vertices.size() << endl;
}

void ZXGraph::printEdges() const{
    for(size_t i = 0; i < _edges.size(); i++){
        cout << "( " << _edges[i].first.first->getId() << ", " <<  _edges[i].first.second->getId() << " )\tType:\t" << _edges[i].second << endl; 
    }
    cout << "Total #Edges: " << _edges.size() << endl;
}
