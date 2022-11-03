/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxGraph.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <vector>
#include <unordered_set>

#include "util.h"
#include "textFormat.h"
#include <ranges>
#include <chrono>

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

EdgeType str2EdgeType(const string& str) {
    if      (str == "SIMPLE"  ) return EdgeType::SIMPLE;
    else if (str == "HADAMARD") return EdgeType::HADAMARD;
    return EdgeType::ERRORTYPE;
}

string EdgeType2Str(const EdgeType& et) {
    if (et == EdgeType::SIMPLE)   return "-";
    if (et == EdgeType::HADAMARD) return TF::BOLD(TF::BLUE("H"));
    return "";
}

/**************************************/
/*   class ZXVertex member functions   */
/**************************************/

// ZXVertex* ZXVertex::getNeighbor(size_t idx) const {
//     if (idx > getNeighbors().size() - 1) return nullptr;
//     return getNeighbors()[idx];
// }

// vector<ZXVertex*> ZXVertex::getNeighbors() const {
//     vector<ZXVertex*> neighbors;
//     for (auto& itr : getNeighborMap()) neighbors.push_back(itr.first);
//     return neighbors;
// }

// Print functions

/**
 * @brief Print summary of ZXVertex
 *
 */
void ZXVertex::printVertex() const {
    cout << "ID:\t" << _id << "\t";
    cout << "VertexType:\t" << VertexType2Str(_type) << "\t";
    cout << "Qubit:\t" << _qubit << "\t";
    cout << "Phase:\t" << _phase << "\t";
    cout << "#Neighbors:\t" << _neighbors.size() << "\t";
    printNeighbors();
}

/**
 * @brief Print each element in _neighborMap
 *
 */
void ZXVertex::printNeighbors() const {
    vector<NeighborPair> nbVec;
    for (const auto& [nb, etype] : _neighbors) nbVec.push_back(make_pair(nb, etype));

    ranges::sort(nbVec, [] (const NeighborPair& a, const NeighborPair& b) -> bool { 
        return a.first->getId() < b.first->getId();
    });

    for (const auto& [nb, etype]: nbVec) {
        cout << "(" << nb->getId() << ", " << EdgeType2Str(etype) << ") ";
    }
    cout << endl;
}

// Action

/**
 * @brief Remove all the connection between `this` and `v`. (Overhauled)
 *
 * @param v
 * @param checked
 */
void ZXVertex::disconnect(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!isNeighbor(v)) {
            cerr << "Error: Vertex " << v->getId() << " is not a neighbor of " << _id << endl;
            return;
        }
    }
    _neighbors.erase(make_pair(v, EdgeType::SIMPLE));
    _neighbors.erase(make_pair(v, EdgeType::HADAMARD));
}

// Test

/**
 * @brief Check if `v` is one of the neighbors in ZXVertex (Overhauled)
 *
 * @param v
 * @return bool
 */
bool ZXVertex::isNeighbor(ZXVertex* v) const {
    return _neighbors.contains(make_pair(v, EdgeType::SIMPLE)) || _neighbors.contains(make_pair(v, EdgeType::HADAMARD));
}

/**
 * @brief Check if ZXGraph is graph-like, report first error
 *
 * @param
 * @return bool
 */
// bool ZXGraph::isGraphLike() const {
    
//     // 2. all Hedge or Bedge
//     for(size_t i=0; i < _edges.size(); i++){
//         if((*_edges[i].second)== EdgeType::HADAMARD) continue;
//         else{
//             if(_edges[i].first.first->getType()== VertexType::BOUNDARY || _edges[i].first.second->getType()== VertexType::BOUNDARY) continue;
//             else{
//                 cout << "False: Type (" << *_edges[i].second << ") of edge " << _edges[i].first.first->getId() << "--" << _edges[i].first.second->getId() << " is invalid!!" << endl;
//                 return false;
//             }
//         }
//     }
//     // 4. B-Z-B and B has only an edge
//     for(size_t i=0; i<_inputs.size(); i++){
//         if(_inputs[i] -> getNumNeighbors() != 1){
//             cout << "False: Boundary vertex " << _inputs[i]->getId() << " has invalid number of neighbors!!" << endl;
//             return false;
//         }
//         if(_inputs[i] -> getNeighbor(0) -> getType() == VertexType::BOUNDARY){
//             cout << "False: Boundary vertex " << _inputs[i]->getId() << " has a boundary neighbor!!" << _inputs[i] -> getNeighbor(0) -> getId() << " !!" << endl;
//             return false;
//         }
//     }
//     // 1. all Z or B  3. no parallel, no selfloop (vertex neighbor)
//     for(size_t i=0; i < _vertices.size(); i++){
//         if(_vertices[i]->getType()!=VertexType::BOUNDARY && _vertices[i]->getType()!=VertexType::Z){
//             cout << "False: Type (" << _vertices[i]->getType() << ") of vertex " << _vertices[i]->getId() << " is invalid!!" << endl;
//             return false;
//         }
//         vector<ZXVertex* > neighbors = _vertices[i]->getNeighbors();
//         vector<ZXVertex* > found;
//         for(size_t j=0; j<neighbors.size(); j++){
//             if(neighbors[j] == _vertices[i]){
//                 cout << "False: Vertex "<< _vertices[i]->getId() << " has selfloop(s)!!" << endl;
//                 return false;
//             }
//             else{
//                 if(find(found.begin(), found.end(), neighbors[j]) != found.end()){
//                     cout << "False: Vertices " << _vertices[i]->getId() << " and " << neighbors[j]->getId() << " have parallel edges!!" << endl;
//                     return false;
//                 }
//                 found.push_back(neighbors[j]);
//             }
//         }
//     }
//     cout << TF::BOLD(TF::GREEN("True: The graph is graph-like")) << endl;
//     return true;
// }

/**************************************/
/*   class ZXGraph member functions   */
/**************************************/

// Getter and setter

// size_t ZXGraph::getNumIncidentEdges(ZXVertex* v) const {
//     // cout << "Find incident of " << v->getId() << endl; 
//     size_t count = 0;
//     for(const auto& edge : _edges){
//         if(edge.first.first == v || edge.first.second == v) count++;
//     }
//     return count;
// }

// EdgePair ZXGraph::getFirstIncidentEdge(ZXVertex* v) const {
//     for(const auto& edge : _edges){
//         if(edge.first.first == v || edge.first.second == v) return edge;
//     }
//     return make_pair(make_pair(nullptr, nullptr), nullptr);
// }

// vector<EdgePair> ZXGraph::getIncidentEdges(ZXVertex* v) const {
//     // cout << "Find incident of " << v->getId() << endl; 
//     vector<EdgePair> incidentEdges;
//     for(size_t e = 0; e < _edges.size(); e++){
//         if(_edges[e].first.first == v || _edges[e].first.second == v){
//             // cout << _edges[e].first.first->getId() << " " << _edges[e].first.second->getId() << endl;
//             incidentEdges.push_back(_edges[e]);
//         }
//     }
//     return incidentEdges;
// }

size_t ZXGraph::getNumEdges() const {
    auto sizes = views::transform(_vertices, [](ZXVertex* const& v) { 
        return v->getNumNeighbors(); 
    });
    return accumulate(sizes.begin(), sizes.end(), 0);
}


vector<ZXVertex*> ZXGraph::getSortedListFromSet(const ZXVertexList& set) const {
    vector<ZXVertex*> result;
    for(const auto& item: set)
        result.push_back(item);
    sort(result.begin(), result.end(), [](ZXVertex* a, ZXVertex* b){ return a->getId() < b->getId();});
    return result;
}
// For testing
// void ZXGraph::generateCNOT() {
//     cout << "Generate a 2-qubit CNOT graph for testing" << endl;
//     // Generate Inputs
//     vector<ZXVertex*> inputs, outputs, vertices;
//     for (size_t i = 0; i < 2; i++) {
//         addInput(findNextId(), i);
//     }

//     // Generate CNOT
//     addVertex(findNextId(), 0, VertexType::Z);
//     addVertex(findNextId(), 1, VertexType::X);

//     // Generate Outputs
//     for (size_t i = 0; i < 2; i++) {
//         addOutput(findNextId(), i);
//     }

//     // Generate edges [(0,2), (1,3), (2,3), (2,4), (3,5)]
//     addEdgeById(3, 5, new EdgeType(EdgeType::SIMPLE));
//     addEdgeById(0, 2, new EdgeType(EdgeType::SIMPLE));
//     addEdgeById(1, 3, new EdgeType(EdgeType::SIMPLE));
//     addEdgeById(2, 3, new EdgeType(EdgeType::SIMPLE));
//     addEdgeById(2, 4, new EdgeType(EdgeType::SIMPLE));
// }

// bool ZXGraph::isEmpty() const {
//     if (_inputs.empty() && _outputs.empty() && _vertices.empty() && _edges.empty()) return true;
//     return false;
// }

// bool ZXGraph::isValid() const {
//     for (auto v: _inputs) {
//         if (v->getNumNeighbors() != 1) return false;
//     }
//     for (auto v: _outputs) {
//         if (v->getNumNeighbors() != 1) return false;
//     }
//     for (size_t i = 0; i < _edges.size(); i++) {
//         if (!_edges[i].first.first->isNeighbor(_edges[i].first.second) ||
//             !_edges[i].first.second->isNeighbor(_edges[i].first.first)) return false;
//     }
//     return true;
// }

// bool ZXGraph::isConnected(ZXVertex* v1, ZXVertex* v2) const {
//     if (v1->isNeighbor(v2) && v2->isNeighbor(v1)) return true;
//     return false;
// }

bool ZXGraph::isId(size_t id) const {
    for (auto v : _vertices) {
        if (v->getId() == id) return true;
    }
    return false;
}

// bool ZXGraph::isInputQubit(int qubit) const {
//     return (_inputList.contains(qubit));
// }

// bool ZXGraph::isOutputQubit(int qubit) const {
//     return (_outputList.contains(qubit));
// }

// Add and Remove

/// @brief Add input to ZXVertexList (Overhauled)
/// @param qubit 
/// @param checked 
/// @return 
ZXVertex* ZXGraph::addInput(int qubit, bool checked) {
    if (!checked) {
        if (isInputQubit(qubit)) {
            cerr << "Error: This qubit's input already exists!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = addVertex(qubit, VertexType::BOUNDARY, Phase(), true);
    _inputs.emplace(v);
    setInputHash(qubit, v);
    return v;
}

/// @brief Add output to ZXVertexList (Overhauled)
/// @param qubit 
/// @param checked 
/// @return 
ZXVertex* ZXGraph::addOutput(int qubit, bool checked) {
    if (!checked) {
        if (isOutputQubit(qubit)) {
            cerr << "Error: This qubit's output already exists!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = addVertex(qubit, VertexType::BOUNDARY, Phase(), true);
    _outputs.emplace(v);
    setOutputHash(qubit, v);
    return v;
}

/// @brief Add vertex to ZXVertexList (Overhauled)
/// @param qubit 
/// @param vt 
/// @param phase 
/// @param checked 
/// @return 
ZXVertex* ZXGraph::addVertex(int qubit, VertexType vt, Phase phase, bool checked) {
    if (!checked) {
        if (vt == VertexType::BOUNDARY) {
            cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = new ZXVertex(_currentVertexId, qubit, vt, phase);
    _vertices.emplace(v);
    if (verbose >= 5) cout << "Add vertex (" << VertexType2Str(vt) << ")" << _currentVertexId << endl;
    _currentVertexId++;
    return v;
}

/// @brief Add a set of inputs (Overhauled)
/// @param inputs 
void ZXGraph::addInputs(const ZXVertexList&  inputs) {
    _inputs.insert(inputs.begin(), inputs.end());
}

/// @brief Add a set of outputs (Overhauled)
/// @param outputs 
void ZXGraph::addOutputs(const ZXVertexList&  outputs) {
    _outputs.insert(outputs.begin(), outputs.end());
}

/// @brief Add a set of vertices (Overhauled)
/// @param vertices
void ZXGraph::addVertices(const ZXVertexList& vertices) {
    _vertices.insert(vertices.begin(), vertices.end());
}

/**
 * @brief Add edge (<<vs, vt>, et>)
 *
 * @param vs
 * @param vt
 * @param et
 * @return EdgePair
 */
EdgePair ZXGraph::addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et) {
    if (vs == vt) {
        Phase phase = (et == EdgeType::HADAMARD) ? Phase(1) : Phase(0);
        cout << "Note: converting this self-loop to phase " << phase << " on vertex " << vs->getId() <<"..." << endl;
        vs->setPhase(vs->getPhase() + phase);
        return makeEdgePairDummy();
    }

    if (vs->isNeighbor(make_pair(vt, et))) {
        if ( 
            (vs->isZ() && vt->isX() && et == EdgeType::HADAMARD) ||
            (vs->isX() && vt->isZ() && et == EdgeType::HADAMARD) ||
            (vs->isZ() && vt->isZ() && et == EdgeType::SIMPLE)   ||
            (vs->isX() && vt->isX() && et == EdgeType::SIMPLE)   
        ) {
            cout << "Note: Redundant edge; merging into existing edge..." << endl;
        } else if (
            (vs->isZ() && vt->isX() && et == EdgeType::SIMPLE  ) ||
            (vs->isX() && vt->isZ() && et == EdgeType::SIMPLE  ) ||
            (vs->isZ() && vt->isZ() && et == EdgeType::HADAMARD) ||
            (vs->isX() && vt->isX() && et == EdgeType::HADAMARD)   
        ) {
            cout << "Note: Hopf edge; cancalling out with existing edge..." << endl;
            vs->removeNeighbor(make_pair(vt, et));
            vt->removeNeighbor(make_pair(vs, et));
        }
        //REVIEW - similar conditions for H-Boxes and boundaries?
    } else {
        vs->addNeighbor(make_pair(vt, et));
        vt->addNeighbor(make_pair(vs, et));
        if (verbose >= 5) cout << "Add edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
    }
    
    return makeEdgePair(vs, vt, et);
}

void ZXGraph::addEdgeById(size_t id_s, size_t id_t, EdgeType et) {
    ZXVertex* vs = findVertexById(id_s);
    ZXVertex* vt = findVertexById(id_t);
    if (!vs)
        cerr << "Error: id_s provided does not exist!" << endl;
    else if (!vt)
        cerr << "Error: id_t provided does not exist!" << endl;
    else {
        addEdge(vs, vt, et);
    }
}

// void ZXGraph::addEdges(vector<EdgePair> edges) {
//     _edges.insert(_edges.end(), edges.begin(), edges.end());
// }

// void ZXGraph::mergeInputList(unordered_map<size_t, ZXVertex*> lst) {
//     _inputList.merge(lst);
// }

// void ZXGraph::mergeOutputList(unordered_map<size_t, ZXVertex*> lst) {
//     _outputList.merge(lst);
// }

/**
 * @brief Remove `v` in ZXGraph and maintain the relationship between each vertex. (Overhauled)
 *
 * @param v
 * @param checked
 */
void ZXGraph::removeVertex(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!_vertices.contains(v)) {
            cerr << "This vertex does not exist!" << endl;
            return;
        }
    }
    
    if (verbose >= 5) cout << "Remove ID: " << v->getId() << endl;

    //! REVIEW Erase neighbors
    Neighbors vNeighbors = v->getNeighbors();
    for(const auto& n: vNeighbors) {
        v -> removeNeighbor(n);
        ZXVertex* nv = n.first;
        EdgeType ne = n.second;
        nv -> removeNeighbor(make_pair(v, ne));
    }
    //! REVIEW remove neighbors
    _vertices.erase(v);


    // Check if also in _inputs or _outputs
    if(_inputs.contains(v)){
        _inputList.erase(v->getQubit());
        _inputs.erase(v);
    }
    if (_outputs.contains(v)) {
        _outputList.erase(v->getQubit());
        _outputs.erase(v);
    }
    // deallocate ZXVertex
    delete v;
}

/**
 * @brief Remove all vertex in vertices by calling `removeVertex(ZXVertex* v, bool checked)`
 *
 * @param vertices
 * @param checked
 */
// void ZXGraph::removeVertices(vector<ZXVertex*> vertices, bool checked) {

//     unordered_set<ZXVertex*> removing;
//     for (const auto& v : vertices) {
//         if (!checked) {
//             if (!isId(v->getId())) {
//                 cerr << "Vertex " << v->getId() << " does not exist!" << endl;
//                 continue;
//             }
//         }
//         if (verbose >= 5) cout << "Remove ID: " << v->getId() << endl;
//         removing.insert(v);
//     }

//     // Check if also in _inputs or _outputs
//     vector<ZXVertex*> newInputs;
//     unordered_map<size_t, ZXVertex*> newInputList;
//     for (const auto& v : _inputs) {
//         if (!removing.contains(v)) {
//             newInputs.push_back(v);
//             newInputList[v->getQubit()] = v;
//         }
//     }
//     setInputs(newInputs);
//     setInputList(newInputList);

//     vector<ZXVertex*> newOutputs;
//     unordered_map<size_t, ZXVertex*> newOutputList;
//     for (const auto& v : _outputs) {
//         if (!removing.contains(v)) {
//             newOutputs.push_back(v);
//             newOutputList[v->getQubit()] = v;
//         }
//     }
//     setOutputs(newOutputs);
//     setOutputList(newOutputList);

//     vector<EdgePair> newEdges;

//     // Check _edges
//     for (const auto& edge : _edges) {
//         if (removing.contains(edge.first.first) || removing.contains(edge.first.second)) {
//             edge.first.first->disconnect(edge.first.second, true);
//         } else {
//             newEdges.push_back(edge);
//         }
//     }
//     setEdges(newEdges);
//     // Check _vertices
//     vector<ZXVertex*> newVertices;
//     for (const auto& v : _vertices) {
//         if (!removing.contains(v)) {
//             newVertices.push_back(v);
//         }
//     }
//     setVertices(newVertices);

//     // deallocate ZXVertex
//     for (const auto& v : removing) {
//         delete v;
//     }
// }

/**
 * @brief Remove all vertices with no neighbor.
 *
 */
// void ZXGraph::removeIsolatedVertices() {
//     vector<ZXVertex*> removing;
//     for (const auto& v : _vertices) {
//         if (v->getNeighborMap().empty()) {
//             removing.push_back(v);
//         }
//     }
//     removeVertices(removing);
// }

/**
 * @brief Remove all edges between `vs` and `vt` by pointer.
 *
 * @param vs
 * @param vt
 * @param checked
 */
void ZXGraph::removeAllEdgeBetween(ZXVertex* vs, ZXVertex* vt, bool checked) {
    if (!checked) {
        if (!vs->isNeighbor(vt) || !vt->isNeighbor(vs)) {
            cerr << "Error: Vertex " << vs->getId() << " and " << vt->getId() << " are not connected!" << endl;
            return;
        }
    }

    vs->disconnect(vt, true);

    if (verbose >= 5) cout << "Remove edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
}

/**
 * @brief Remove an edge exactly equal to `ep`.
 *
 * @param ep
 */
void ZXGraph::removeEdge(const EdgePair& ep) {
    ZXVertex* vs = ep.first.first;
    ZXVertex* vt = ep.first.second;
    EdgeType et = ep.second;
    vs->removeNeighbor(make_pair(vt, et));
    vt->removeNeighbor(make_pair(vs, et));

    if (verbose >= 5) cout << "Remove edge ( " << vs->getId() << ", " << vt->getId() << " ), type: " << EdgeType2Str(et) << endl;
}

// void ZXGraph::removeEdgesByEdgePairs(const vector<EdgePair>& eps) {
//     unordered_set<EdgePair> removing;
//     for (const auto& ep : eps) {
//         removing.insert(ep);
//         if (verbose >= 5) {
//             cout << "Remove (" << ep.first.first->getId() << ", " << ep.first.second->getId() << " )" << endl;
//         }

//         NeighborMap nbm = ep.first.first->getNeighborMap();
//         auto result = nbm.equal_range(ep.first.second);
//         for (auto& itr = result.first; itr != result.second; ++itr) {
//             if (itr->second == ep.second) {
//                 nbm.erase(itr);
//                 ep.first.first->setNeighborMap(nbm);
//                 break;
//             }
//         }

//         nbm = ep.first.second->getNeighborMap();
//         result = nbm.equal_range(ep.first.first);
//         for (auto& itr = result.first; itr != result.second; ++itr) {
//             if (itr->second == ep.second) {
//                 nbm.erase(itr);
//                 ep.first.second->setNeighborMap(nbm);
//                 break;
//             }
//         }
//     }

//     vector<EdgePair> newEdges;

//     for (const auto& edge : _edges) {
//         if (!removing.contains(edge) && !removing.contains(
//             make_pair(make_pair(edge.first.second, edge.first.first), edge.second)
//         )) {
//             newEdges.push_back(edge);
//         }
//     } 
//     setEdges(newEdges);
// }

/**
 * @brief adjoint the zxgraph
 * 
 */
// void ZXGraph::adjoint() {
//     swap(_inputs, _outputs);
//     swap(_inputList, _outputList);
//     for (auto& v : _vertices) v->setPhase(-v->getPhase());
// }

/**
 * @brief Assign rotation/value to the specified boundary 
 *
 * @param qubit
 * @param isInput
 * @param ty
 * @param phase
 */
// void ZXGraph::assignBoundary(size_t qubit, bool isInput, VertexType ty, Phase phase){
//     ZXVertex* v = addVertex(findNextId(), qubit, ty, phase);
//     ZXVertex* boundary = isInput ? _inputList[qubit] : _outputList[qubit];
//     EdgeType e = *(boundary -> getNeighborMap().begin()->second);
//     ZXVertex* nebBound = boundary->getNeighbor(0);
//     removeVertex(boundary);
//     // removeEdge(boundary, nebBound);
//     addEdge(v, nebBound, new EdgeType(e));
// }


// Find functions

/**
 * @brief Find Vertex by vertex's id.
 *
 * @param id
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::findVertexById(const size_t& id) const {
    for(const auto& ver: _vertices){
        if(ver->getId() == id) return ver;
    }
    return nullptr;
}

/**
 * @brief Find the next id that is never been used.
 *
 * @return size_t
 */
// size_t ZXGraph::findNextId() const {
//     size_t nextId = 0;
//     for (size_t i = 0; i < _vertices.size(); i++) {
//         if (_vertices[i]->getId() >= nextId) nextId = _vertices[i]->getId() + 1;
//     }
//     return nextId;
// }

// Action
// void ZXGraph::reset() {
//     // for(size_t i = 0; i < _vertices.size(); i++) delete _vertices[i];
//     // for(size_t i = 0; i < _topoOrder.size(); i++) delete _topoOrder[i];
//     _inputs.clear();
//     _outputs.clear();
//     _vertices.clear();
//     _edges.clear();
//     _inputList.clear();
//     _outputList.clear();
//     _topoOrder.clear();
//     _globalDFScounter = 1;
// }

// ZXGraph* ZXGraph::copy() const {
//     //! Check if EdgeType change simultaneously
    
//     ZXGraph* newGraph = new ZXGraph(0);
//     unordered_map<size_t, ZXVertex*> id2vertex;
//     newGraph->setId(getId());

//     newGraph->_inputs.reserve(this->getNumInputs());
//     newGraph->_inputList.reserve(this->getNumInputs());
//     newGraph->_outputs.reserve(this->getNumOutputs());
//     newGraph->_outputList.reserve(this->getNumOutputs());
//     newGraph->_vertices.reserve(this->getNumVertices());
//     id2vertex.reserve(this->getNumVertices());
//     newGraph->_edges.reserve(this->getNumEdges());
//     // new Inputs
//     for (const auto& v : this->getInputs()) {
//         id2vertex[v->getId()] = newGraph->addInput(v->getId(), v->getQubit(), true);
        
//     }

//     // new Outputs
//     for (const auto& v : this->getOutputs()) {
//         id2vertex[v->getId()] = newGraph->addOutput(v->getId(), v->getQubit(), true);
//     }

//     // new Vertices (without I/O)
//     for (const auto& v : this->getVertices()) {
//         if (v->getType() != VertexType::BOUNDARY) {
//             id2vertex[v->getId()] = newGraph->addVertex(v->getId(), v->getQubit(), v->getType(), v->getPhase(), true);
//         }
//     }

//     for (const auto& [vpair, etype]: this->getEdges()) {
//         newGraph->addEdge(id2vertex[vpair.first->getId()], id2vertex[vpair.second->getId()], new EdgeType(*etype));
//     }
    
//     return newGraph;
// }

// void ZXGraph::sortIOByQubit() {
//     sort(_inputs.begin(), _inputs.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
//     sort(_outputs.begin(), _outputs.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
// }

// void ZXGraph::sortVerticeById() {
//     sort(_vertices.begin(), _vertices.end(), [](ZXVertex* a, ZXVertex* b) { return a->getId() < b->getId(); });
// }

// void ZXGraph::liftQubit(const size_t& n) {
//     for_each(_vertices.begin(), _vertices.end(), [&n](ZXVertex* v) { v->setQubit(v->getQubit() + n); });

//     unordered_map<size_t, ZXVertex*> newInputList, newOutputList;

//     for_each(_inputList.begin(), _inputList.end(),
//              [&n, &newInputList](pair<size_t, ZXVertex*> itr) {
//                  newInputList[itr.first + n] = itr.second;
//              });
//     for_each(_outputList.begin(), _outputList.end(),
//              [&n, &newOutputList](pair<size_t, ZXVertex*> itr) {
//                  newOutputList[itr.first + n] = itr.second;
//              });

//     setInputList(newInputList);
//     setOutputList(newOutputList);
// }

// // Print functions
void ZXGraph::printGraph() const {
    cout << "Graph " << _id << endl;
    cout << setw(15) << left << "Inputs: " << getNumInputs() << endl;
    cout << setw(15) << left << "Outputs: " << getNumOutputs() << endl;
    cout << setw(15) << left << "Vertices: " << getNumVertices() << endl;
    cout << setw(15) << left << "Edges: " << getNumEdges() << endl;
}

void ZXGraph::printInputs() const {
    vector<ZXVertex*> vs;
    for (const auto& v : _inputs) vs.push_back(v);

    ranges::sort(vs, ZXVertex::idLessThan);
    for (size_t i = 0; i < vs.size(); i++) {
        cout << "Input " << i + 1 << setw(8) << left << ":" << vs[i]->getId() << endl;
    }
    cout << "Total #Inputs: " << getNumInputs() << endl;
}

void ZXGraph::printOutputs() const {
    vector<ZXVertex*> vs;
    for (const auto& v : _inputs) vs.push_back(v);

    ranges::sort(vs, ZXVertex::idLessThan);
    for (size_t i = 0; i < vs.size(); i++) {
        cout << "Output " << i + 1 << setw(7) << left << ":" << vs[i]->getId() << endl;
    }
    cout << "Total #Outputs: " << getNumOutputs() << endl;
}

void ZXGraph::printVertices() const {
    cout << "\n";
    vector<ZXVertex*> vs;
    //! REVIEW print efficiency?
    for (const auto& v : _vertices) vs.push_back(v);
    
    ranges::sort(vs, ZXVertex::idLessThan);
    for (const auto& v : vs) {
        v->printVertex();
    }
    cout << "Total #Vertices: " << getNumVertices() << endl;
    cout << "\n";
}

void ZXGraph::printEdges() const {
    forEachEdge([](const EdgePair& epair) {
        cout << "( " << epair.first.first->getId() << ", " << epair.first.second->getId() << " )\tType:\t" << EdgeType2Str(epair.second) << endl;
    });
    cout << "Total #Edges: " << getNumEdges() << endl;
}

