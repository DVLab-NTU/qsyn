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

VertexType str2VertexType(string str){
    if(str == "BOUNDARY") return VertexType::BOUNDARY;
    else if (str == "Z") return VertexType::Z;
    else if (str == "X") return VertexType::X;
    else if (str == "H_BOX") return VertexType::H_BOX;
    return VertexType::ERRORTYPE;
}

EdgeType str2EdgeType(string str){
    if(str == "SIMPLE") return EdgeType::SIMPLE;
    else if(str == "HADAMARD") return EdgeType::HADAMARD;
    return EdgeType::ERRORTYPE;
}

/**************************************/
/*   class ZXVertex member functions   */
/**************************************/

// ZXVertex::ZXVertex(const ZXVertex& zxVertex){
//     ZXVertex copy = ZXVertex(0, 0, VertexType::ERRORTYPE);
//     copy._qubit = zxVertex._qubit;
//     copy._id = zxVertex._id;
//     copy._type = zxVertex._type;
//     copy._phase = zxVertex._phase;
//     for(size_t i = 0; i < zxVertex._neighbors.size(); i++){
//         copy._neighbors.push_back(zxVertex._neighbors[i]);
//     }
//     return &copy;
// }

// Getter and Setter
NeighborPair ZXVertex::getNeighborById(size_t id) const{
   if(!isNeighborById(id)) cerr << "Error: Vertex " << id << " is not a neighbor of " << _id << endl;
   else{
       for(size_t i = 0; i < _neighbors.size(); i++){
           if(_neighbors[i].first->getId() == id) return _neighbors[i];
       }
   }
}


// Add and Remove

void ZXVertex::removeNeighbor(NeighborPair neighbor){
    if(find(_neighbors.begin(), _neighbors.end(), neighbor) != _neighbors.end()){
        cout << "  * remove " << neighbor.first->getId() << " from " << _id << endl;
        _neighbors.erase(find(_neighbors.begin(), _neighbors.end(), neighbor));
    } 
    else cerr << "Vertex " << neighbor.first->getId() << " is not a neighbor of " << _id << endl;
}

void ZXVertex::removeNeighborById(size_t id){
    if(!isNeighborById(id)) cerr << "Error: Vertex " << id << " is not a neighbor of " << _id << endl;
    else{
        for(size_t i = 0; i < _neighbors.size();){
            if(_neighbors[i].first->getId() == id) {
                _neighbors.erase(_neighbors.begin()+i);
                cout << "  * remove " << id << " from " << _id << endl;
            }
            else i++;
        }
    }
}


// Print functions

void ZXVertex::printVertex() const{
    cout << "ID:\t" << _id << "\t";
    cout << "VertexType:\t" << _type << "\t";
    cout << "Qubit:\t" << _qubit << "\t";
    cout << "#Neighbors:\t" << _neighbors.size() << "\t";
    printNeighbors();
}

void ZXVertex::printNeighbors() const{
    for(size_t i = 0; i < _neighbors.size(); i++){
        cout << "( " << _neighbors[i].first->getId() << ", " << _neighbors[i].second << " )  " ;
    }
    cout << endl;
}


// Test
bool ZXVertex::isNeighbor(ZXVertex* v) const{
    for(size_t i = 0; i < _neighbors.size(); i++){
        if(_neighbors[i].first == v) return true;
    }
    return false;
}

bool ZXVertex::isNeighborById(size_t id) const{
    for(size_t i = 0; i < _neighbors.size(); i++){
        if(_neighbors[i].first->getId() == id) return true;
    }
    return false;
}


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
    edgeList.push_back(make_pair(0,2));
    edgeList.push_back(make_pair(1,3));
    edgeList.push_back(make_pair(2,3));
    edgeList.push_back(make_pair(2,4));
    edgeList.push_back(make_pair(3,5));

    vector<EdgePair > edges;

    for(size_t i = 0; i < edgeList.size(); i++){
        if(findVertexById(edgeList[i].first) != nullptr && findVertexById(edgeList[i].second) != nullptr){
            ZXVertex* s = findVertexById(edgeList[i].first); ZXVertex* t = findVertexById(edgeList[i].second);
            s->addNeighbor(make_pair(t, EdgeType::SIMPLE));
            t->addNeighbor(make_pair(s, EdgeType::SIMPLE));
            edges.push_back(make_pair(make_pair(s, t), EdgeType::SIMPLE));
        }
    }
    setEdges(edges);
    setQubitCount(_inputs.size());
}

bool ZXGraph::isEmpty() const{
    if(_inputs.empty() && _outputs.empty() && _vertices.empty() && _edges.empty() && _nqubit == 0) return true;
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


// Add and Remove
void ZXGraph::addInput(size_t id, int qubit){
    if(isId(id)) cerr << "Error: This vertex id is already exist!!" << endl;
    else{
        ZXVertex* v = new ZXVertex(id, qubit, VertexType::BOUNDARY);
        _vertices.push_back(v);
        _inputs.push_back(v);
    }
}

void ZXGraph::addOutput(size_t id, int qubit){
    if(isId(id)) cerr << "Error: This vertex id is already exist!!" << endl;
    else{
        ZXVertex* v = new ZXVertex(id, qubit, VertexType::BOUNDARY);
        _vertices.push_back(v);
        _outputs.push_back(v);
    }
}

void ZXGraph::addVertex(size_t id, int qubit, VertexType vt){
    if(isId(id)) cerr << "Error: This vertex id is already exist!!" << endl;
    else if(vt == VertexType::BOUNDARY) cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
    else{
        ZXVertex* v = new ZXVertex(id, qubit, vt);
        _vertices.push_back(v);
    }
}

void ZXGraph::addEdgeById(size_t id_s, size_t id_t, EdgeType et){
    if(!isId(id_s)) cerr << "Error: id_s provided is not exist!" << endl;
    else if(!isId(id_t)) cerr << "Error: id_t provided is not exist!" << endl;
    else{
        cout << "Add edge ( " << id_s << ", " << id_t << " )" << endl;
        ZXVertex* vs = findVertexById(id_s); ZXVertex* vt = findVertexById(id_t);
        vs->addNeighbor(make_pair(vt, et)); vt->addNeighbor(make_pair(vs, et));
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

void ZXGraph::removeVertex(ZXVertex* v){
    if(!isId(v->getId())) cerr << "This vertex is not exist!" << endl;
    else{
        cout << "Remove ID: " << v->getId() << endl;
        // Remove all neighbors' records
        for(size_t i = 0; i < v->getNeighbors().size(); i++){
            v->getNeighbors()[i].first->removeNeighbor(make_pair(v, v->getNeighbors()[i].second));
        }
        // Check if also in _inputs or _outputs
        if(find(_inputs.begin(), _inputs.end(), v) != _inputs.end()) _inputs.erase(find(_inputs.begin(), _inputs.end(), v));
        if(find(_outputs.begin(), _outputs.end(), v) != _outputs.end()) _outputs.erase(find(_outputs.begin(), _outputs.end(), v));

        // Check _edges
        for(size_t i = 0; i < _edges.size();){
            if(_edges[i].first.first == v || _edges[i].first.second == v) _edges.erase(_edges.begin()+i);
            else i++;
        }

        // Check _vertices
        _vertices.erase(find(_vertices.begin(), _vertices.end(), v));

        // deallocate ZXVertex
        delete v;
    }
}

void ZXGraph::removeVertexById(size_t id){
    if(findVertexById(id) != nullptr) removeVertex(findVertexById(id));
    else cerr << "Error: This vertex id is not exist!!" << endl;
}

void ZXGraph::removeIsolatedVertices(){
    for(size_t i = 0; i < _vertices.size(); ){
        if(_vertices[i]->getNeighbors().empty()){
            removeVertex(_vertices[i]);
        }
        else i++;
    }
}

void ZXGraph::removeEdgeById(size_t id_s, size_t id_t){
    if(!isId(id_s)) cerr << "Error: id_s provided is not exist!" << endl;
    else if(!isId(id_t)) cerr << "Error: id_t provided is not exist!" << endl;
    else if(!isConnected(findVertexById(id_s), findVertexById(id_t))) cerr << "Error: id_s and id_t are not connected!" << endl;
    else{
        cout << "Remove edge ( " << id_s << ", " << id_t << " )" << endl;
        ZXVertex* vs = findVertexById(id_s); ZXVertex* vt = findVertexById(id_t);
        vs->removeNeighborById(id_t); vt->removeNeighborById(id_s);
        for(size_t i = 0; i < _edges.size();){
            if((_edges[i].first.first == vs && _edges[i].first.second == vt) || (_edges[i].first.first == vt && _edges[i].first.second == vs)){
                _edges.erase(_edges.begin()+i);
            } 
            else i++;
        }
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

ZXGraph* ZXGraph::copy() const{
    ZXGraph* newGraph = new ZXGraph(0);

    newGraph->setId(getId());
    newGraph->setQubitCount(getQubitCount());

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
        EdgePair newPair = make_pair(make_pair(newGraph->findVertexById(oriPair.first.first->getId()), newGraph->findVertexById(oriPair.first.second->getId())), oriPair.second);
        edges.push_back(newPair);
    }
    newGraph->setEdges(edges);
    return newGraph;
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

