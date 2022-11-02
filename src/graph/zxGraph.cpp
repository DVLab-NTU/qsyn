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

/**************************************/
/*   class ZXVertex member functions   */
/**************************************/

ZXVertex* ZXVertex::getNeighbor_depr(size_t idx) const {
    if (idx > getNeighbors_depr().size() - 1) return nullptr;
    return getNeighbors_depr()[idx];
}

vector<ZXVertex*> ZXVertex::getNeighbors_depr() const {
    vector<ZXVertex*> neighbors;
    for (auto& itr : getNeighborMap()) neighbors.push_back(itr.first);
    return neighbors;
}

// Print functions

/**
 * @brief Print summary of ZXVertex
 *
 */
void ZXVertex::printVertex_depr() const {
    cout << "ID:\t" << _id << "\t";
    cout << "VertexType:\t" << VertexType2Str(_type) << "\t";
    cout << "Qubit:\t" << _qubit << "\t";
    cout << "Phase:\t" << _phase << "\t";
    cout << "#Neighbors:\t" << _neighborMap_depr.size() << "\t";
    printNeighborMap_depr();
}

/**
 * @brief Print each element in _neighborMap
 *
 */
void ZXVertex::printNeighborMap_depr() const {
    vector<pair<ZXVertex*, EdgeType*> > neighborList;
    for (auto& itr : _neighborMap_depr) neighborList.push_back(make_pair(itr.first, itr.second));

    sort(neighborList.begin(), neighborList.end(), [](pair<ZXVertex*, EdgeType*> a, pair<ZXVertex*, EdgeType*> b) { return a.first->getId() < b.first->getId(); });

    for (size_t i = 0; i < neighborList.size(); i++) {
        cout << "(" << neighborList[i].first->getId() << ", " << EdgeType2Str_depr(neighborList[i].second) << ") ";
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
void ZXVertex::disconnect_depr(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!isNeighbor_depr(v)) {
            cerr << "Error: Vertex " << v->getId() << " is not a neighbor of " << _id << endl;
            return;
        }
    }

    _neighborMap_depr.erase(v);
    NeighborMap_depr nMap = v->getNeighborMap();
    nMap.erase(this);
    v->setNeighborMap(nMap);
}

// Test

/**
 * @brief Check if `v` is one of the neighbors in ZXVertex
 *
 * @param v
 * @return bool
 */
bool ZXVertex::isNeighbor_depr(ZXVertex* v) const {
    return _neighborMap_depr.contains(v);
}

/**
 * @brief Check if ZXGraph is graph-like, report first error
 *
 * @param
 * @return bool
 */
bool ZXGraph::isGraphLike() const {
    
    // 2. all Hedge or Bedge
    for(size_t i=0; i < _edges_depr.size(); i++){
        if((*_edges_depr[i].second)== EdgeType::HADAMARD) continue;
        else{
            if(_edges_depr[i].first.first->getType()== VertexType::BOUNDARY || _edges_depr[i].first.second->getType()== VertexType::BOUNDARY) continue;
            else{
                cout << "False: Type (" << *_edges_depr[i].second << ") of edge " << _edges_depr[i].first.first->getId() << "--" << _edges_depr[i].first.second->getId() << " is invalid!!" << endl;
                return false;
            }
        }
    }
    // 4. B-Z-B and B has only an edge
    for(size_t i=0; i<_inputs_depr.size(); i++){
        if(_inputs_depr[i] -> getNumNeighbors_depr() != 1){
            cout << "False: Boundary vertex " << _inputs_depr[i]->getId() << " has invalid number of neighbors!!" << endl;
            return false;
        }
        if(_inputs_depr[i] -> getNeighbor_depr(0) -> getType() == VertexType::BOUNDARY){
            cout << "False: Boundary vertex " << _inputs_depr[i]->getId() << " has a boundary neighbor!!" << _inputs_depr[i] -> getNeighbor_depr(0) -> getId() << " !!" << endl;
            return false;
        }
    }
    // 1. all Z or B  3. no parallel, no selfloop (vertex neighbor)
    for(size_t i=0; i < _vertices_depr.size(); i++){
        if(_vertices_depr[i]->getType()!=VertexType::BOUNDARY && _vertices_depr[i]->getType()!=VertexType::Z){
            cout << "False: Type (" << _vertices_depr[i]->getType() << ") of vertex " << _vertices_depr[i]->getId() << " is invalid!!" << endl;
            return false;
        }
        vector<ZXVertex* > neighbors = _vertices_depr[i]->getNeighbors_depr();
        vector<ZXVertex* > found;
        for(size_t j=0; j<neighbors.size(); j++){
            if(neighbors[j] == _vertices_depr[i]){
                cout << "False: Vertex "<< _vertices_depr[i]->getId() << " has selfloop(s)!!" << endl;
                return false;
            }
            else{
                if(find(found.begin(), found.end(), neighbors[j]) != found.end()){
                    cout << "False: Vertices " << _vertices_depr[i]->getId() << " and " << neighbors[j]->getId() << " have parallel edges!!" << endl;
                    return false;
                }
                found.push_back(neighbors[j]);
            }
        }
    }
    cout << TF::BOLD(TF::GREEN("True: The graph is graph-like")) << endl;
    return true;
}

/**************************************/
/*   class ZXGraph member functions   */
/**************************************/

// Getter and setter

size_t ZXGraph::getNumIncidentEdges_depr(ZXVertex* v) const {
    // cout << "Find incident of " << v->getId() << endl; 
    size_t count = 0;
    for(const auto& edge : _edges_depr){
        if(edge.first.first == v || edge.first.second == v) count++;
    }
    return count;
}

EdgePair_depr ZXGraph::getFirstIncidentEdge_depr(ZXVertex* v) const {
    for(const auto& edge : _edges_depr){
        if(edge.first.first == v || edge.first.second == v) return edge;
    }
    return make_pair(make_pair(nullptr, nullptr), nullptr);
}

vector<EdgePair_depr> ZXGraph::getIncidentEdges_depr(ZXVertex* v) const {
    // cout << "Find incident of " << v->getId() << endl; 
    vector<EdgePair_depr> incidentEdges;
    for(size_t e = 0; e < _edges_depr.size(); e++){
        if(_edges_depr[e].first.first == v || _edges_depr[e].first.second == v){
            // cout << _edges[e].first.first->getId() << " " << _edges[e].first.second->getId() << endl;
            incidentEdges.push_back(_edges_depr[e]);
        }
    }
    return incidentEdges;
}

// For testing
void ZXGraph::generateCNOT() {
    cout << "Generate a 2-qubit CNOT graph for testing" << endl;
    // Generate Inputs
    vector<ZXVertex*> inputs, outputs, vertices;
    for (size_t i = 0; i < 2; i++) {
        addInput_depr(findNextId(), i);
    }

    // Generate CNOT
    addVertex_depr(findNextId(), 0, VertexType::Z);
    addVertex_depr(findNextId(), 1, VertexType::X);

    // Generate Outputs
    for (size_t i = 0; i < 2; i++) {
        addOutput_depr(findNextId(), i);
    }

    // Generate edges [(0,2), (1,3), (2,3), (2,4), (3,5)]
    addEdgeById_depr(3, 5, new EdgeType(EdgeType::SIMPLE));
    addEdgeById_depr(0, 2, new EdgeType(EdgeType::SIMPLE));
    addEdgeById_depr(1, 3, new EdgeType(EdgeType::SIMPLE));
    addEdgeById_depr(2, 3, new EdgeType(EdgeType::SIMPLE));
    addEdgeById_depr(2, 4, new EdgeType(EdgeType::SIMPLE));
}

bool ZXGraph::isEmpty() const {
    if (_inputs_depr.empty() && _outputs_depr.empty() && _vertices_depr.empty() && _edges_depr.empty()) return true;
    return false;
}

bool ZXGraph::isValid() const {
    for (auto v: _inputs_depr) {
        if (v->getNumNeighbors_depr() != 1) return false;
    }
    for (auto v: _outputs_depr) {
        if (v->getNumNeighbors_depr() != 1) return false;
    }
    for (size_t i = 0; i < _edges_depr.size(); i++) {
        if (!_edges_depr[i].first.first->isNeighbor_depr(_edges_depr[i].first.second) ||
            !_edges_depr[i].first.second->isNeighbor_depr(_edges_depr[i].first.first)) return false;
    }
    return true;
}

bool ZXGraph::isConnected(ZXVertex* v1, ZXVertex* v2) const {
    if (v1->isNeighbor_depr(v2) && v2->isNeighbor_depr(v1)) return true;
    return false;
}

bool ZXGraph::isId_depr(size_t id) const {
    for (size_t i = 0; i < _vertices_depr.size(); i++) {
        if (_vertices_depr[i]->getId() == id) return true;
    }
    return false;
}

bool ZXGraph::isInputQubit(int qubit) const {
    return (_inputList.contains(qubit));
}

bool ZXGraph::isOutputQubit(int qubit) const {
    return (_outputList.contains(qubit));
}

// Add and Remove
ZXVertex* ZXGraph::addInput_depr(size_t id, int qubit, bool checked) {
    if (!checked) {
        if (isId_depr(id)) {
            cerr << "Error: This vertex id already exists!!" << endl;
            return nullptr;
        } else if (isInputQubit(qubit)) {
            cerr << "Error: This qubit's input already exists!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = addVertex_depr(id, qubit, VertexType::BOUNDARY, Phase(), true);

    _inputs_depr.push_back(v);
    setInputHash(qubit, v);
    if (verbose >= 5) cout << "Add input " << id << endl;
    return v;
}

ZXVertex* ZXGraph::addOutput_depr(size_t id, int qubit, bool checked) {
    if (!checked) {
        if (isId_depr(id)) {
            cerr << "Error: This vertex id already exists!!" << endl;
            return nullptr;
        } else if (isOutputQubit(qubit)) {
            cerr << "Error: This qubit's output already exists!!" << endl;
            return nullptr;
        } 
    }
    ZXVertex* v = addVertex_depr(id, qubit, VertexType::BOUNDARY, Phase(), true);
    
    _outputs_depr.push_back(v);
    setOutputHash(qubit, v);
    if (verbose >= 5) cout << "Add output " << id << endl;
    return v;
    
}

ZXVertex* ZXGraph::addVertex_depr(size_t id, int qubit, VertexType vt, Phase phase, bool checked) {
    if (!checked) {
        if (isId_depr(id)) {
            cerr << "Error: This vertex id is already exist!!" << endl;
            return nullptr;
        } else if (vt == VertexType::BOUNDARY) {
            cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = new ZXVertex(id, qubit, vt, phase);
    _vertices_depr.push_back(v);
    // ZXNeighborMap nbm{{v, EdgeType::SIMPLE}};
    _vertices.emplace(v);
    if (verbose >= 5) cout << "Add vertex " << id << endl;
    return v;
}
/**
 * @brief Add edge (<<vs, vt>, et>)
 *
 * @param vs
 * @param vt
 * @param et
 * @return EdgePair
 */
EdgePair_depr ZXGraph::addEdge_depr(ZXVertex* vs, ZXVertex* vt, EdgeType* et, bool allowSelfloop) {
    vs->addNeighbor_depr(make_pair(vt, et));
    vt->addNeighbor_depr(make_pair(vs, et));
    _edges_depr.emplace_back(make_pair(vs, vt), et);
    if (verbose >= 5) cout << "Add edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
    return _edges_depr.back();

    // return make_pair(make_pair(nullptr, nullptr), nullptr);
}

void ZXGraph::addEdgeById_depr(size_t id_s, size_t id_t, EdgeType* et) {
    if (!isId_depr(id_s))
        cerr << "Error: id_s provided does not exist!" << endl;
    else if (!isId_depr(id_t))
        cerr << "Error: id_t provided does not exist!" << endl;
    else {
        ZXVertex* vs = findVertexById_depr(id_s);
        ZXVertex* vt = findVertexById_depr(id_t);
        addEdge_depr(vs, vt, et);
    }
}

void ZXGraph::addInputs_depr(vector<ZXVertex*> inputs) {
    _inputs_depr.insert(_inputs_depr.end(), inputs.begin(), inputs.end());
}

void ZXGraph::addOutputs_depr(vector<ZXVertex*> outputs) {
    _outputs_depr.insert(_outputs_depr.end(), outputs.begin(), outputs.end());
}

void ZXGraph::addVertices_depr(vector<ZXVertex*> vertices) {
    _vertices_depr.insert(_vertices_depr.end(), vertices.begin(), vertices.end());
}

void ZXGraph::addEdges(vector<EdgePair_depr> edges) {
    _edges_depr.insert(_edges_depr.end(), edges.begin(), edges.end());
}

void ZXGraph::mergeInputList(unordered_map<size_t, ZXVertex*> lst) {
    _inputList.merge(lst);
}

void ZXGraph::mergeOutputList(unordered_map<size_t, ZXVertex*> lst) {
    _outputList.merge(lst);
}

/**
 * @brief Remove `v` in ZXGraph and maintain the relationship between each vertex.
 *
 * @param v
 * @param checked
 */
void ZXGraph::removeVertex_depr(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!isId_depr(v->getId())) {
            cerr << "This vertex does not exist!" << endl;
            return;
        }
    }
    
    if (verbose >= 5) cout << "Remove ID: " << v->getId() << endl;

    //! REVIEW remove neighbors
    _vertices.erase(v);


    // Check if also in _inputs or _outputs
    if (auto itr = find(_inputs_depr.begin(), _inputs_depr.end(), v); itr != _inputs_depr.end()) {
        _inputList.erase(v->getQubit());
        _inputs_depr.erase(itr);
    }
    if (auto itr = find(_outputs_depr.begin(), _outputs_depr.end(), v); itr != _outputs_depr.end()) {
        _outputList.erase(v->getQubit());
        _outputs_depr.erase(itr);
    }

    // Check _edges
    vector<EdgePair_depr> newEdges;
    for (size_t i = 0; i < _edges_depr.size(); i++) {
        if (_edges_depr[i].first.first == v || _edges_depr[i].first.second == v)
            _edges_depr[i].first.first->disconnect_depr(_edges_depr[i].first.second, true);
        else {
            newEdges.push_back(_edges_depr[i]);
        }
    }
    setEdges(newEdges);

    // Check _vertices
    _vertices_depr.erase(find(_vertices_depr.begin(), _vertices_depr.end(), v));

    // deallocate ZXVertex
    delete v;
}

/**
 * @brief Remove all vertex in vertices by calling `removeVertex(ZXVertex* v, bool checked)`
 *
 * @param vertices
 * @param checked
 */
void ZXGraph::removeVertices(vector<ZXVertex*> vertices, bool checked) {

    unordered_set<ZXVertex*> removing;
    for (const auto& v : vertices) {
        if (!checked) {
            if (!isId_depr(v->getId())) {
                cerr << "Vertex " << v->getId() << " does not exist!" << endl;
                continue;
            }
        }
        if (verbose >= 5) cout << "Remove ID: " << v->getId() << endl;
        removing.insert(v);
    }

    // Check if also in _inputs or _outputs
    vector<ZXVertex*> newInputs;
    unordered_map<size_t, ZXVertex*> newInputList;
    for (const auto& v : _inputs_depr) {
        if (!removing.contains(v)) {
            newInputs.push_back(v);
            newInputList[v->getQubit()] = v;
        }
    }
    setInputs_depr(newInputs);
    setInputList(newInputList);

    vector<ZXVertex*> newOutputs;
    unordered_map<size_t, ZXVertex*> newOutputList;
    for (const auto& v : _outputs_depr) {
        if (!removing.contains(v)) {
            newOutputs.push_back(v);
            newOutputList[v->getQubit()] = v;
        }
    }
    setOutputs(newOutputs);
    setOutputList(newOutputList);

    vector<EdgePair_depr> newEdges;

    // Check _edges
    for (const auto& edge : _edges_depr) {
        if (removing.contains(edge.first.first) || removing.contains(edge.first.second)) {
            edge.first.first->disconnect_depr(edge.first.second, true);
        } else {
            newEdges.push_back(edge);
        }
    }
    setEdges(newEdges);
    // Check _vertices
    vector<ZXVertex*> newVertices;
    for (const auto& v : _vertices_depr) {
        if (!removing.contains(v)) {
            newVertices.push_back(v);
        }
    }
    setVertices(newVertices);

    // deallocate ZXVertex
    for (const auto& v : removing) {
        delete v;
    }
}

/**
 * @brief Remove vertex by vertex's id. (Used in ZXCmd.cpp)
 *
 * @param id
 */
void ZXGraph::removeVertexById(const size_t& id) {
    ZXVertex* v = findVertexById(id);
    if (v != nullptr)
        removeVertex(v, true);
    else
        cerr << "Error: This vertex id does not exist!!" << endl;
}

/**
 * @brief Remove all vertices with no neighbor.
 *
 */
void ZXGraph::removeIsolatedVertices_depr() {
    vector<ZXVertex*> removing;
    for (const auto& v : _vertices_depr) {
        if (v->getNeighborMap().empty()) {
            removing.push_back(v);
        }
    }
    removeVertices(removing);
}

/**
 * @brief Remove all edges between `vs` and `vt` by pointer.
 *
 * @param vs
 * @param vt
 * @param checked
 */
void ZXGraph::removeEdge_depr(ZXVertex* vs, ZXVertex* vt, bool checked) {
    if (!checked) {
        if (!vs->isNeighbor_depr(vt) || !vt->isNeighbor_depr(vs)) {
            cerr << "Error: Vertex " << vs->getId() << " and " << vt->getId() << " are not connected!" << endl;
            return;
        }
    }
    for (size_t i = 0; i < _edges_depr.size();) {
        if ((_edges_depr[i].first.first == vs && _edges_depr[i].first.second == vt) || (_edges_depr[i].first.first == vt && _edges_depr[i].first.second == vs)) {
            _edges_depr.erase(_edges_depr.begin() + i);
        } else
            i++;
    }
    vs->disconnect_depr(vt, true);

    if (verbose >= 5) cout << "Remove edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
}

/**
 * @brief Remove an edge exactly equal to `ep`.
 *
 * @param ep
 */
void ZXGraph::removeEdgeByEdgePair_depr(const EdgePair_depr& ep) {
    for (size_t i = 0; i < _edges_depr.size(); i++) {
        if ((ep.first.first == _edges_depr[i].first.first && ep.first.second == _edges_depr[i].first.second && ep.second == _edges_depr[i].second) || 
             (ep.first.first == _edges_depr[i].first.second && ep.first.second == _edges_depr[i].first.first && ep.second == _edges_depr[i].second)) {
            if (verbose >= 5) cout << "Remove (" << ep.first.first->getId() << ", " << ep.first.second->getId() << " )" << endl;
            NeighborMap_depr nb = ep.first.first->getNeighborMap();
            auto neighborItr = nb.equal_range(ep.first.second);
            for (auto itr = neighborItr.first; itr != neighborItr.second; ++itr) {
                if (itr->second == ep.second) {
                    nb.erase(itr);
                    ep.first.first->setNeighborMap(nb);
                    break;
                }
            }
            nb = ep.first.second->getNeighborMap();
            neighborItr = nb.equal_range(ep.first.first);
            for (auto itr = neighborItr.first; itr != neighborItr.second; ++itr) {
                if (itr->second == ep.second) {
                    nb.erase(itr);
                    ep.first.second->setNeighborMap(nb);
                    break;
                }
            }
            delete ep.second;
            _edges_depr.erase(_edges_depr.begin() + i);

            return;
        }
    }
}

void ZXGraph::removeEdgesByEdgePairs_depr(const vector<EdgePair_depr>& eps) {
    unordered_set<EdgePair_depr> removing;
    for (const auto& ep : eps) {
        removing.insert(ep);
        if (verbose >= 5) {
            cout << "Remove (" << ep.first.first->getId() << ", " << ep.first.second->getId() << " )" << endl;
        }

        NeighborMap_depr nbm = ep.first.first->getNeighborMap();
        auto result = nbm.equal_range(ep.first.second);
        for (auto& itr = result.first; itr != result.second; ++itr) {
            if (itr->second == ep.second) {
                nbm.erase(itr);
                ep.first.first->setNeighborMap(nbm);
                break;
            }
        }

        nbm = ep.first.second->getNeighborMap();
        result = nbm.equal_range(ep.first.first);
        for (auto& itr = result.first; itr != result.second; ++itr) {
            if (itr->second == ep.second) {
                nbm.erase(itr);
                ep.first.second->setNeighborMap(nbm);
                break;
            }
        }
    }

    vector<EdgePair_depr> newEdges;

    for (const auto& edge : _edges_depr) {
        if (!removing.contains(edge) && !removing.contains(
            make_pair(make_pair(edge.first.second, edge.first.first), edge.second)
        )) {
            newEdges.push_back(edge);
        }
    } 
    setEdges(newEdges);
}

/**
 * @brief Remove all edges between `vs` and `vt` by vertex's id.
 *
 * @param id_s
 * @param id_t
 * @param etype if given ERRORTYPE, remove all edges between two vertices
 */
void ZXGraph::removeEdgeById(const size_t& id_s, const size_t& id_t, EdgeType etype) {
    ZXVertex* vs = findVertexById(id_s);
    ZXVertex* vt = findVertexById(id_t);
    if (!vs) {
        cerr << "Error: id_s provided does not exist!" << endl;
        return;
    }
    if (!vt) {
        cerr << "Error: id_t provided does not exist!" << endl;
        return;
    }
    
    if (etype == EdgeType::ERRORTYPE) {
        removeAllEdgeBetween(vs, vt);
    } else {
        removeEdge(makeEdgePair(vs, vt, etype));
    }
}

/**
 * @brief adjoint the zxgraph
 * 
 */
void ZXGraph::adjoint() {
    swap(_inputs_depr, _outputs_depr);
    swap(_inputList, _outputList);
    for (auto& v : _vertices_depr) v->setPhase(-v->getPhase());
}

/**
 * @brief Assign rotation/value to the specified boundary 
 *
 * @param qubit
 * @param isInput
 * @param ty
 * @param phase
 */
void ZXGraph::assignBoundary(size_t qubit, bool isInput, VertexType ty, Phase phase){
    ZXVertex* v = addVertex_depr(findNextId(), qubit, ty, phase);
    ZXVertex* boundary = isInput ? _inputList[qubit] : _outputList[qubit];
    EdgeType e = *(boundary -> getNeighborMap().begin()->second);
    ZXVertex* nebBound = boundary->getNeighbor_depr(0);
    removeVertex_depr(boundary);
    // removeEdge(boundary, nebBound);
    addEdge_depr(v, nebBound, new EdgeType(e));
}


// Find functions

/**
 * @brief Find Vertex by vertex's id.
 *
 * @param id
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::findVertexById_depr(const size_t& id) const {
    for (size_t i = 0; i < _vertices_depr.size(); i++) {
        if (_vertices_depr[i]->getId() == id) return _vertices_depr[i];
    }
    return nullptr;
}

/**
 * @brief Find the next id that is never been used.
 *
 * @return size_t
 */
size_t ZXGraph::findNextId() const {
    size_t nextId = 0;
    for (size_t i = 0; i < _vertices_depr.size(); i++) {
        if (_vertices_depr[i]->getId() >= nextId) nextId = _vertices_depr[i]->getId() + 1;
    }
    return nextId;
}

// Action
void ZXGraph::reset() {
    // for(size_t i = 0; i < _vertices.size(); i++) delete _vertices[i];
    // for(size_t i = 0; i < _topoOrder.size(); i++) delete _topoOrder[i];
    _inputs_depr.clear();
    _outputs_depr.clear();
    _vertices_depr.clear();
    _edges_depr.clear();
    _inputList.clear();
    _outputList.clear();
    _topoOrder.clear();
    _globalDFScounter = 1;
}

ZXGraph* ZXGraph::copy() const {
    //! Check if EdgeType change simultaneously
    
    ZXGraph* newGraph = new ZXGraph(0);
    unordered_map<size_t, ZXVertex*> id2vertex;
    newGraph->setId(getId());

    newGraph->_inputs_depr.reserve(this->getNumInputs_depr());
    newGraph->_inputList.reserve(this->getNumInputs_depr());
    newGraph->_outputs_depr.reserve(this->getNumOutputs_depr());
    newGraph->_outputList.reserve(this->getNumOutputs_depr());
    newGraph->_vertices_depr.reserve(this->getNumVertices_depr());
    id2vertex.reserve(this->getNumVertices_depr());
    newGraph->_edges_depr.reserve(this->getNumEdges_depr());
    // new Inputs
    for (const auto& v : this->getInputs_depr()) {
        id2vertex[v->getId()] = newGraph->addInput_depr(v->getId(), v->getQubit(), true);
        
    }

    // new Outputs
    for (const auto& v : this->getOutputs_depr()) {
        id2vertex[v->getId()] = newGraph->addOutput_depr(v->getId(), v->getQubit(), true);
    }

    // new Vertices (without I/O)
    for (const auto& v : this->getVertices_depr()) {
        if (v->getType() != VertexType::BOUNDARY) {
            id2vertex[v->getId()] = newGraph->addVertex_depr(v->getId(), v->getQubit(), v->getType(), v->getPhase(), true);
        }
    }

    for (const auto& [vpair, etype]: this->getEdges()) {
        newGraph->addEdge_depr(id2vertex[vpair.first->getId()], id2vertex[vpair.second->getId()], new EdgeType(*etype));
    }
    
    return newGraph;
}

void ZXGraph::sortIOByQubit() {
    sort(_inputs_depr.begin(), _inputs_depr.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
    sort(_outputs_depr.begin(), _outputs_depr.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
}

void ZXGraph::sortVerticeById() {
    sort(_vertices_depr.begin(), _vertices_depr.end(), [](ZXVertex* a, ZXVertex* b) { return a->getId() < b->getId(); });
}

void ZXGraph::liftQubit(const size_t& n) {
    for_each(_vertices_depr.begin(), _vertices_depr.end(), [&n](ZXVertex* v) { v->setQubit(v->getQubit() + n); });

    unordered_map<size_t, ZXVertex*> newInputList, newOutputList;

    for_each(_inputList.begin(), _inputList.end(),
             [&n, &newInputList](pair<size_t, ZXVertex*> itr) {
                 newInputList[itr.first + n] = itr.second;
             });
    for_each(_outputList.begin(), _outputList.end(),
             [&n, &newOutputList](pair<size_t, ZXVertex*> itr) {
                 newOutputList[itr.first + n] = itr.second;
             });

    setInputList(newInputList);
    setOutputList(newOutputList);
}

// Print functions
void ZXGraph::printGraph_depr() const {
    cout << "Graph " << _id << endl;
    cout << setw(15) << left << "Inputs: " << _inputs_depr.size() << endl;
    cout << setw(15) << left << "Outputs: " << _outputs_depr.size() << endl;
    cout << setw(15) << left << "Vertices: " << _vertices_depr.size() << endl;
    cout << setw(15) << left << "Edges: " << _edges_depr.size() << endl;
}

void ZXGraph::printInputs_depr() const {
    for (size_t i = 0; i < _inputs_depr.size(); i++) {
        cout << "Input " << i + 1 << setw(8) << left << ":" << _inputs_depr[i]->getId() << endl;
    }
    cout << "Total #Inputs: " << _inputs_depr.size() << endl;
}

void ZXGraph::printOutputs_depr() const {
    for (size_t i = 0; i < _outputs_depr.size(); i++) {
        cout << "Output " << i + 1 << setw(7) << left << ":" << _outputs_depr[i]->getId() << endl;
    }
    cout << "Total #Outputs: " << _outputs_depr.size() << endl;
}

void ZXGraph::printVertices_depr() const {
    cout << "\n";
    vector<ZXVertex*> vs;
    //! REVIEW print efficiency?
    for_each(_vertices.begin(), _vertices.end(), [&vs](ZXVertex* const& nb) {
        vs.push_back(nb);
    });
    sort(vs.begin(), vs.end(), [](ZXVertex* const& lhs, ZXVertex* const& rhs){
        return lhs->getId() < rhs->getId();
    });
    for (const auto& v : vs) {
        v->printVertex_depr();
    }
    cout << "Total #Vertices: " << _vertices_depr.size() << endl;
    cout << "\n";
}

void ZXGraph::printEdges_depr() const {
    for (size_t i = 0; i < _edges_depr.size(); i++) {
        cout << "( " << _edges_depr[i].first.first->getId() << ", " << _edges_depr[i].first.second->getId() << " )\tType:\t" << EdgeType2Str_depr(_edges_depr[i].second) << endl;
    }
    cout << "Total #Edges: " << _edges_depr.size() << endl;
}

//REVIEW - unused function
void ZXGraph::printEdge_depr(size_t idx) const{
    if(idx < _edges_depr.size())
        cout << "( " << _edges_depr[idx].first.first->getId() << ", " << _edges_depr[idx].first.second->getId() << " )\tType:\t" << EdgeType2Str_depr(_edges_depr[idx].second) << endl;
}

EdgeType toggleEdge(const EdgeType& et) {
    if (et == EdgeType::SIMPLE)
        return EdgeType::HADAMARD;
    else if (et == EdgeType::HADAMARD)
        return EdgeType::SIMPLE;
    else
        return EdgeType::ERRORTYPE;
}

VertexType str2VertexType(const string& str) {
    if (str == "BOUNDARY")
        return VertexType::BOUNDARY;
    else if (str == "Z")
        return VertexType::Z;
    else if (str == "X")
        return VertexType::X;
    else if (str == "H_BOX")
        return VertexType::H_BOX;
    return VertexType::ERRORTYPE;
}

string VertexType2Str(const VertexType& vt) {
    if (vt == VertexType::X) return TF::BOLD(TF::RED("X"));
    if (vt == VertexType::Z) return TF::BOLD(TF::GREEN("Z"));
    if (vt == VertexType::H_BOX) return TF::BOLD(TF::YELLOW("H"));
    if (vt == VertexType::BOUNDARY) return "●";
    return "";
}

EdgeType* str2EdgeType_depr(const string& str) {
    if (str == "SIMPLE")
        return new EdgeType(EdgeType::SIMPLE);
    else if (str == "HADAMARD")
        return new EdgeType(EdgeType::HADAMARD);
    return new EdgeType(EdgeType::ERRORTYPE);
}

string EdgeType2Str_depr(const EdgeType* et) {
    if (*et == EdgeType::SIMPLE) return "-";
    if (*et == EdgeType::HADAMARD) return TF::BOLD(TF::BLUE("H"));
    return "";
}

EdgePair_depr makeEdgeKey_depr(ZXVertex* v1, ZXVertex* v2, EdgeType* et) {
    return make_pair(
        (v2->getId() < v1->getId()) ? make_pair(v2, v1) : make_pair(v1, v2), et);
}

EdgePair_depr makeEdgeKey_depr(EdgePair_depr epair) {
    return make_pair(
        (epair.first.second->getId() < epair.first.first->getId()) ? make_pair(epair.first.second, epair.first.first) : make_pair(epair.first.first, epair.first.second), epair.second);
}

EdgePair makeEdgePair(ZXVertex* v1, ZXVertex* v2, EdgeType et) {
    return make_pair(
        (v1->getId() < v2->getId()) ? make_pair(v1, v2) : make_pair(v2, v1),
        et);
}

EdgePair makeEdgePair(EdgePair epair) {
    return make_pair(
        (epair.first.first->getId() < epair.first.second->getId()) ? make_pair(epair.first.first, epair.first.second) : make_pair(epair.first.second, epair.first.first),
        epair.second);
}

EdgePair makeEdgePairDummy() {
    return make_pair(make_pair(nullptr, nullptr), EdgeType::ERRORTYPE);
}
