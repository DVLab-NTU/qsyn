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

#include "util.h"
#include "textFormat.h"

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

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

EdgeType* str2EdgeType(const string& str) {
    if (str == "SIMPLE")
        return new EdgeType(EdgeType::SIMPLE);
    else if (str == "HADAMARD")
        return new EdgeType(EdgeType::HADAMARD);
    return new EdgeType(EdgeType::ERRORTYPE);
}

string EdgeType2Str(const EdgeType* et) {
    if (*et == EdgeType::SIMPLE) return "-";
    if (*et == EdgeType::HADAMARD) return TF::BOLD(TF::BLUE("H"));
    return "";
}

/**************************************/
/*   class ZXVertex member functions   */
/**************************************/

ZXVertex* ZXVertex::getNeighbor(size_t idx) const {
    if (idx > getNeighbors().size() - 1) return nullptr;
    return getNeighbors()[idx];
}

vector<ZXVertex*> ZXVertex::getNeighbors() const {
    vector<ZXVertex*> neighbors;
    for (auto& itr : getNeighborMap()) neighbors.push_back(itr.first);
    return neighbors;
}

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
    cout << "#Neighbors:\t" << _neighborMap.size() << "\t";
    printNeighborMap();
}

/**
 * @brief Print each element in _neighborMap
 *
 */
void ZXVertex::printNeighborMap() const {
    vector<pair<ZXVertex*, EdgeType*> > neighborList;
    for (auto& itr : _neighborMap) neighborList.push_back(make_pair(itr.first, itr.second));

    sort(neighborList.begin(), neighborList.end(), [](pair<ZXVertex*, EdgeType*> a, pair<ZXVertex*, EdgeType*> b) { return a.first->getId() < b.first->getId(); });

    for (size_t i = 0; i < neighborList.size(); i++) {
        cout << "(" << neighborList[i].first->getId() << ", " << EdgeType2Str(neighborList[i].second) << ") ";
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
void ZXVertex::disconnect(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!isNeighbor(v)) {
            cerr << "Error: Vertex " << v->getId() << " is not a neighbor of " << _id << endl;
            return;
        }
    }

    _neighborMap.erase(v);
    NeighborMap nMap = v->getNeighborMap();
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
bool ZXVertex::isNeighbor(ZXVertex* v) const {
    auto itr = _neighborMap.find(v);
    if (itr != _neighborMap.end())
        return true;
    else
        return false;
}

/**************************************/
/*   class ZXGraph member functions   */
/**************************************/

// For testing
void ZXGraph::generateCNOT() {
    cout << "Generate a 2-qubit CNOT graph for testing" << endl;
    // Generate Inputs
    vector<ZXVertex*> inputs, outputs, vertices;
    for (size_t i = 0; i < 2; i++) {
        addInput(findNextId(), i);
    }

    // Generate CNOT
    addVertex(findNextId(), 0, VertexType::Z);
    addVertex(findNextId(), 1, VertexType::X);

    // Generate Outputs
    for (size_t i = 0; i < 2; i++) {
        addOutput(findNextId(), i);
    }

    // Generate edges [(0,2), (1,3), (2,3), (2,4), (3,5)]
    addEdgeById(3, 5, new EdgeType(EdgeType::SIMPLE));
    addEdgeById(0, 2, new EdgeType(EdgeType::SIMPLE));
    addEdgeById(1, 3, new EdgeType(EdgeType::SIMPLE));
    addEdgeById(2, 3, new EdgeType(EdgeType::SIMPLE));
    addEdgeById(2, 4, new EdgeType(EdgeType::SIMPLE));
}

bool ZXGraph::isEmpty() const {
    if (_inputs.empty() && _outputs.empty() && _vertices.empty() && _edges.empty()) return true;
    return false;
}

bool ZXGraph::isValid() const {
    for (auto v: _inputs) {
        if (v->getNumNeighbors() != 1) return false;
    }
    for (auto v: _outputs) {
        if (v->getNumNeighbors() != 1) return false;
    }
    for (size_t i = 0; i < _edges.size(); i++) {
        if (!_edges[i].first.first->isNeighbor(_edges[i].first.second) ||
            !_edges[i].first.second->isNeighbor(_edges[i].first.first)) return false;
    }
    return true;
}

bool ZXGraph::isConnected(ZXVertex* v1, ZXVertex* v2) const {
    if (v1->isNeighbor(v2) && v2->isNeighbor(v1)) return true;
    return false;
}

bool ZXGraph::isId(size_t id) const {
    for (size_t i = 0; i < _vertices.size(); i++) {
        if (_vertices[i]->getId() == id) return true;
    }
    return false;
}

bool ZXGraph::isInputQubit(int qubit) const {
    return (_inputList.find(qubit) != _inputList.end());
}

bool ZXGraph::isOutputQubit(int qubit) const {
    return (_outputList.find(qubit) != _outputList.end());
}

// Add and Remove
ZXVertex* ZXGraph::addInput(size_t id, int qubit) {
    if (isId(id)) {
        cerr << "Error: This vertex id already exists!!" << endl;
        return nullptr;
    } else if (isInputQubit(qubit)) {
        cerr << "Error: This qubit's input already exists!!" << endl;
        return nullptr;
    } else {
        ZXVertex* v = new ZXVertex(id, qubit, VertexType::BOUNDARY);
        _inputs.push_back(v);
        _vertices.push_back(v);
        setInputHash(qubit, v);
        if (verbose >= 3) cout << "Add input " << id << endl;
        return v;
    }
}

ZXVertex* ZXGraph::addOutput(size_t id, int qubit) {
    if (isId(id)) {
        cerr << "Error: This vertex id already exists!!" << endl;
        return nullptr;
    } else if (isOutputQubit(qubit)) {
        cerr << "Error: This qubit's output already exists!!" << endl;
        return nullptr;
    } else {
        ZXVertex* v = new ZXVertex(id, qubit, VertexType::BOUNDARY);
        _vertices.push_back(v);
        _outputs.push_back(v);
        setOutputHash(qubit, v);
        if (verbose >= 3) cout << "Add output " << id << endl;
        return v;
    }
}

ZXVertex* ZXGraph::addVertex(size_t id, int qubit, VertexType vt, Phase phase) {
    if (isId(id)) {
        cerr << "Error: This vertex id is already exist!!" << endl;
        return nullptr;
    } else if (vt == VertexType::BOUNDARY) {
        cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
        return nullptr;
    } else {
        ZXVertex* v = new ZXVertex(id, qubit, vt, phase);
        _vertices.push_back(v);
        if (verbose >= 3) cout << "Add vertex " << id << endl;
        return v;
    }
}
/**
 * @brief Add edge (<<vs, vt>, et>)
 *
 * @param vs
 * @param vt
 * @param et
 * @return EdgePair
 */
EdgePair ZXGraph::addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType* et) {
    // if (vs->getType() == VertexType::BOUNDARY && vs->getNumNeighbors() >= 1) {
    //     cerr << "Boundary vertex " << vs->getId() << " must not have more than one neighbor." << endl;
    // } else if (vt->getType() == VertexType::BOUNDARY && vt->getNumNeighbors() >= 1) {
    //     cerr << "Boundary vertex " << vt->getId() << " must not have more than one neighbor." << endl;
    // } else if (vs == vt && vs->getType() == VertexType::BOUNDARY) {
    //     cerr << "Boundary vertex " << vs->getId() << " must not have self loop" << endl;
    // } else {
        // NeighborMap mode
    vs->addNeighbor(make_pair(vt, et));
    vt->addNeighbor(make_pair(vs, et));
    _edges.emplace_back(make_pair(vs, vt), et);
    if (verbose >= 3) cout << "Add edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
    return _edges.back();
    // }

    // return make_pair(make_pair(nullptr, nullptr), nullptr);
}

void ZXGraph::addEdgeById(size_t id_s, size_t id_t, EdgeType* et) {
    if (!isId(id_s))
        cerr << "Error: id_s provided does not exist!" << endl;
    else if (!isId(id_t))
        cerr << "Error: id_t provided does not exist!" << endl;
    else {
        ZXVertex* vs = findVertexById(id_s);
        ZXVertex* vt = findVertexById(id_t);
        addEdge(vs, vt, et);
    }
}

void ZXGraph::addInputs(vector<ZXVertex*> inputs) {
    _inputs.insert(_inputs.end(), inputs.begin(), inputs.end());
}

void ZXGraph::addOutputs(vector<ZXVertex*> outputs) {
    _outputs.insert(_outputs.end(), outputs.begin(), outputs.end());
}

void ZXGraph::addVertices(vector<ZXVertex*> vertices) {
    _vertices.insert(_vertices.end(), vertices.begin(), vertices.end());
}

void ZXGraph::addEdges(vector<EdgePair> edges) {
    _edges.insert(_edges.end(), edges.begin(), edges.end());
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
void ZXGraph::removeVertex(ZXVertex* v, bool checked) {
    if (!checked) {
        if (!isId(v->getId())) {
            cerr << "This vertex does not exist!" << endl;
            return;
        }
    }

    if (verbose >= 3) cout << "Remove ID: " << v->getId() << endl;

    // Check if also in _inputs or _outputs
    if (auto itr = find(_inputs.begin(), _inputs.end(), v); itr != _inputs.end()) {
        _inputList.erase(v->getQubit());
        _inputs.erase(itr);
    }
    if (auto itr = find(_outputs.begin(), _outputs.end(), v); itr != _outputs.end()) {
        _outputList.erase(v->getQubit());
        _outputs.erase(itr);
    }

    // Check _edges
    vector<EdgePair> newEdges;
    for (size_t i = 0; i < _edges.size(); i++) {
        if (_edges[i].first.first == v || _edges[i].first.second == v)
            _edges[i].first.first->disconnect(_edges[i].first.second, true);
        else {
            newEdges.push_back(_edges[i]);
        }
    }
    setEdges(newEdges);

    // Check _vertices
    _vertices.erase(find(_vertices.begin(), _vertices.end(), v));

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
    for (size_t i = 0; i < vertices.size(); i++) {
        removeVertex(vertices[i], checked);
    }
}

/**
 * @brief Remove vertex by vertex's id. (Used in ZXCmd.cpp)
 *
 * @param id
 */
void ZXGraph::removeVertexById(const size_t& id) {
    auto v = findVertexById(id);
    if (v != nullptr)
        removeVertex(v, true);
    else
        cerr << "Error: This vertex id does not exist!!" << endl;
}

/**
 * @brief Remove all vertices with no neighbor.
 *
 */
void ZXGraph::removeIsolatedVertices() {
    for (size_t i = 0; i < _vertices.size();) {
        if (_vertices[i]->getNeighborMap().empty()) {
            removeVertex(_vertices[i], true);
        } else
            i++;
    }
}

/**
 * @brief Remove all edges between `vs` and `vt` by pointer.
 *
 * @param vs
 * @param vt
 * @param checked
 */
void ZXGraph::removeEdge(ZXVertex* vs, ZXVertex* vt, bool checked) {
    if (!checked) {
        if (!vs->isNeighbor(vt) || !vt->isNeighbor(vs)) {
            cerr << "Error: Vertex " << vs->getId() << " and " << vt->getId() << " are not connected!" << endl;
            return;
        }
    }
    for (size_t i = 0; i < _edges.size();) {
        if ((_edges[i].first.first == vs && _edges[i].first.second == vt) || (_edges[i].first.first == vt && _edges[i].first.second == vs)) {
            _edges.erase(_edges.begin() + i);
        } else
            i++;
    }
    vs->disconnect(vt, true);

    if (verbose >= 5) cout << "Remove edge ( " << vs->getId() << ", " << vt->getId() << " )" << endl;
}

/**
 * @brief Remove an edge exactly equal to `ep`.
 *
 * @param ep
 */
void ZXGraph::removeEdgeByEdgePair(const EdgePair& ep) {
    for (size_t i = 0; i < _edges.size(); i++) {
        if ((ep.first.first == _edges[i].first.first && ep.first.second == _edges[i].first.second && ep.second == _edges[i].second) || 
             (ep.first.first == _edges[i].first.second && ep.first.second == _edges[i].first.first && ep.second == _edges[i].second)) {
            if (verbose >= 3) cout << "Remove (" << ep.first.first->getId() << ", " << ep.first.second->getId() << " )" << endl;
            NeighborMap nb = ep.first.first->getNeighborMap();
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
            _edges.erase(_edges.begin() + i);
            if (verbose >= 5) printVertices();
            return;
        }
    }
}

/**
 * @brief Remove all edges between `vs` and `vt` by vertex's id.
 *
 * @param id_s
 * @param id_t
 */
void ZXGraph::removeEdgeById(const size_t& id_s, const size_t& id_t) {
    if (!isId(id_s))
        cerr << "Error: id_s provided does not exist!" << endl;
    else if (!isId(id_t))
        cerr << "Error: id_t provided does not exist!" << endl;
    else {
        ZXVertex* vs = findVertexById(id_s);
        ZXVertex* vt = findVertexById(id_t);
        removeEdge(vs, vt);
    }
}

// Find functions

/**
 * @brief Find Vertex by vertex's id.
 *
 * @param id
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::findVertexById(const size_t& id) const {
    for (size_t i = 0; i < _vertices.size(); i++) {
        if (_vertices[i]->getId() == id) return _vertices[i];
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
    for (size_t i = 0; i < _vertices.size(); i++) {
        if (_vertices[i]->getId() >= nextId) nextId = _vertices[i]->getId() + 1;
    }
    return nextId;
}

// Action
void ZXGraph::reset() {
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

ZXGraph* ZXGraph::copy() const {
    //! Check if EdgeType change simultaneously
    ZXGraph* newGraph = new ZXGraph(0);

    newGraph->setId(getId());

    vector<ZXVertex*> inputs, outputs, vertices;
    vector<EdgePair> edges;
    // new Inputs
    for (size_t i = 0; i < getInputs().size(); i++) {
        ZXVertex* oriVertex = getInputs()[i];
        ZXVertex* newVertex = new ZXVertex(oriVertex->getId(), oriVertex->getQubit(), oriVertex->getType());
        inputs.push_back(newVertex);
        vertices.push_back(newVertex);
        newGraph->_inputList[newVertex->getQubit()] = newVertex;
    }
    newGraph->setInputs(inputs);

    // new Outputs
    for (size_t i = 0; i < getOutputs().size(); i++) {
        ZXVertex* oriVertex = getOutputs()[i];
        ZXVertex* newVertex = new ZXVertex(oriVertex->getId(), oriVertex->getQubit(), oriVertex->getType());
        outputs.push_back(newVertex);
        vertices.push_back(newVertex);
        newGraph->_outputList[newVertex->getQubit()] = newVertex;
    }
    newGraph->setOutputs(outputs);

    // new Vertices (without I/O)
    for (size_t i = 0; i < getVertices().size(); i++) {
        if (getVertices()[i]->getType() != VertexType::BOUNDARY) {
            ZXVertex* oriVertex = getVertices()[i];
            ZXVertex* newVertex = new ZXVertex(oriVertex->getId(), oriVertex->getQubit(), oriVertex->getType());
            vertices.push_back(newVertex);
        }
    }
    newGraph->setVertices(vertices);

    for (size_t i = 0; i < getEdges().size(); i++) {
        EdgePair oriPair = getEdges()[i];
        ZXVertex* s = newGraph->findVertexById(oriPair.first.first->getId());
        ZXVertex* t = newGraph->findVertexById(oriPair.first.second->getId());
        EdgeType* et = new EdgeType(*oriPair.second);
        // cout << s->getId() << "," << t->getId() << ": " << *et << endl;
        s->addNeighbor(make_pair(t, et));
        t->addNeighbor(make_pair(s, et));
        edges.push_back(make_pair(make_pair(s, t), et));
    }
    newGraph->setEdges(edges);

    return newGraph;
}

void ZXGraph::sortIOByQubit() {
    sort(_inputs.begin(), _inputs.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
    sort(_outputs.begin(), _outputs.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
}

void ZXGraph::sortVerticeById() {
    sort(_vertices.begin(), _vertices.end(), [](ZXVertex* a, ZXVertex* b) { return a->getId() < b->getId(); });
}
void ZXGraph::liftQubit(const size_t& n) {
    for_each(_vertices.begin(), _vertices.end(), [&n](ZXVertex* v) { v->setQubit(v->getQubit() + n); });

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
void ZXGraph::printGraph() const {
    cout << "Graph " << _id << endl;
    cout << setw(15) << left << "Inputs: " << _inputs.size() << endl;
    cout << setw(15) << left << "Outputs: " << _outputs.size() << endl;
    cout << setw(15) << left << "Vertices: " << _vertices.size() << endl;
    cout << setw(15) << left << "Edges: " << _edges.size() << endl;
}

void ZXGraph::printInputs() const {
    for (size_t i = 0; i < _inputs.size(); i++) {
        cout << "Input " << i + 1 << setw(8) << left << ":" << _inputs[i]->getId() << endl;
    }
    cout << "Total #Inputs: " << _inputs.size() << endl;
}

void ZXGraph::printOutputs() const {
    for (size_t i = 0; i < _outputs.size(); i++) {
        cout << "Output " << i + 1 << setw(7) << left << ":" << _outputs[i]->getId() << endl;
    }
    cout << "Total #Outputs: " << _outputs.size() << endl;
}

void ZXGraph::printVertices() const {
    for (size_t i = 0; i < _vertices.size(); i++) {
        _vertices[i]->printVertex();
    }
    cout << "Total #Vertices: " << _vertices.size() << endl;
}

void ZXGraph::printEdges() const {
    for (size_t i = 0; i < _edges.size(); i++) {
        cout << "( " << _edges[i].first.first->getId() << ", " << _edges[i].first.second->getId() << " )\tType:\t" << EdgeType2Str(_edges[i].second) << endl;
    }
    cout << "Total #Edges: " << _edges.size() << endl;
}

EdgePair makeEdgeKey(ZXVertex* v1, ZXVertex* v2, EdgeType* et) {
    return make_pair(
        (v2->getId() < v1->getId()) ? make_pair(v2, v1) : make_pair(v1, v2), et);
}
EdgePair makeEdgeKey(EdgePair epair) {
    return make_pair(
        (epair.first.second->getId() < epair.first.first->getId()) ? make_pair(epair.first.second, epair.first.first) : make_pair(epair.first.first, epair.first.second), epair.second);
}

EdgeKey makeEdgeKey(ZXVertex* v1, ZXVertex* v2, EdgeType et) {
    return make_pair(
        (v2->getId() < v1->getId()) ? make_pair(v2, v1) : make_pair(v1, v2), et);
}
EdgeKey makeEdgeKey(EdgeKey epair) {
    return make_pair(
        (epair.first.second->getId() < epair.first.first->getId()) ? make_pair(epair.first.second, epair.first.first) : make_pair(epair.first.first, epair.first.second), epair.second);
}
