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
/*   class ZXVertex member functions  */
/**************************************/


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
    // if(_neighbors.size()==0) return;
    for (const auto& [nb, etype]: _neighbors) {
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
    v->removeNeighbor(make_pair(this, EdgeType::SIMPLE));
    v->removeNeighbor(make_pair(this, EdgeType::HADAMARD));
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



/*****************************************************/
/*   class ZXGraph Getter and setter functions       */
/*****************************************************/

/**
 * @brief Get the number of edges in ZX-graph
 * 
 * @return size_t 
 */
size_t ZXGraph::getNumEdges() const {
    size_t n = 0;
    for (auto& v: _vertices) {
        n += v->getNumNeighbors(); 
    }
    return n/2;
}



/*****************************************************/
/*   class ZXGraph Testing functions                 */
/*****************************************************/

/**
 * @brief Check if `id` is an existed vertex id
 * 
 * @param id 
 * @return true 
 * @return false 
 */
bool ZXGraph::isId(size_t id) const {
    for (auto v : _vertices) {
        if (v->getId() == id) return true;
    }
    return false;
}

/**
 * @brief Generate a CNOT subgraph to the ZXGraph (testing)
 * 
 */
void ZXGraph::generateCNOT() {
    cout << "Generate a 2-qubit CNOT graph for testing" << endl;
    ZXVertex* i0 = addInput(0);
    ZXVertex* i1 = addInput(1);
    ZXVertex* vz = addVertex(0, VertexType::Z);
    ZXVertex* vx = addVertex(1, VertexType::X);
    ZXVertex* o0 = addOutput(0);
    ZXVertex* o1 = addOutput(1);

    addEdge(i0, vz, EdgeType::SIMPLE);
    addEdge(i1, vx, EdgeType::SIMPLE);
    addEdge(vz, vx, EdgeType::SIMPLE);
    addEdge(o0, vz, EdgeType::SIMPLE);
    addEdge(o1, vx, EdgeType::SIMPLE);
}

/**
 * @brief Check if ZXGraph is graph-like, report first error
 *
 * @param
 * @return bool
 */
bool ZXGraph::isGraphLike() const {
    
    // all internal edges are hadamard edges
    for (const auto& v : _vertices) {
        if (!v->isZ() || !v->isBoundary()) {
            if (verbose >= 5) {
                cout << "Note: vertex " << v->getId() << " is of type " << VertexType2Str(v->getType()) << endl;
            }
        }
        for (const auto& [nb, etype] : v->getNeighbors()) {
            if (v->isBoundary() || nb->isBoundary()) continue;
            if (etype != EdgeType::HADAMARD) {
                if (verbose >= 5) {
                    cout << "Note: internal simple edge (" << v->getId() << ", " << nb->getId() << ")" << endl;
                }
                return false;
            }
        }
    }

    // 4. B-Z-B and B has only an edge
    for (const auto& v : _inputs) {
        if (v->getNumNeighbors() != 1) {
            if (verbose >= 5) {
                cout << "Note: boundary " << v->getId() << " has " 
                     << v->getNumNeighbors() << " neighbors; expected 1" << endl;
            }
            return false;
        }
    }
    for (const auto& v : _outputs) {
        if (v->getNumNeighbors() != 1) {
            if (verbose >= 5) {
                cout << "Note: boundary " << v->getId() << " has " 
                     << v->getNumNeighbors() << " neighbors; expected 1" << endl;
            }
            return false;
        }
    }
    
    // guard B-B edge?
    return true;
}

/**
 * @brief Check if the ZX-graph is an empty one (no vertice)
 * 
 * @return true 
 * @return false 
 */
bool ZXGraph::isEmpty() const {
    return (_inputs.empty() && _outputs.empty() && _vertices.empty());
}

/**
 * @brief Check if the ZX-graph is valid (i/o connected to 1 vertex, each neighbor matches)
 * 
 * @return true 
 * @return false 
 */
bool ZXGraph::isValid() const {
    for (auto& v: _inputs) {
        if (v->getNumNeighbors() != 1) return false;
    }
    for (auto& v: _outputs) {
        if (v->getNumNeighbors() != 1) return false;
    }
    for (auto& v: _vertices) {
        for (auto& [nb, etype]: v->getNeighbors()) {
            if (!nb->getNeighbors().contains(make_pair(v, etype))) return false;
        }
    }
    return true;
}



/*****************************************************/
/*   class ZXGraph Add and Remove functions          */
/*****************************************************/

/**
 * @brief Add input to ZXVertexList
 * 
 * @param qubit 
 * @param checked 
 * @return ZXVertex* 
 */
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

/**
 * @brief Add output to ZXVertexList
 * 
 * @param qubit 
 * @param checked 
 * @return ZXVertex* 
 */
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

/**
 * @brief Add vertex to ZXVertexList
 * 
 * @param qubit 
 * @param vt 
 * @param phase 
 * @param checked 
 * @return ZXVertex* 
 */
ZXVertex* ZXGraph::addVertex(int qubit, VertexType vt, Phase phase, bool checked) {
    if (!checked) {
        if (vt == VertexType::BOUNDARY) {
            cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = new ZXVertex(_nextVId, qubit, vt, phase);
    _vertices.emplace(v);
    if (verbose >= 5) cout << "Add vertex (" << VertexType2Str(vt) << ")" << _nextVId << endl;
    _nextVId++;
    return v;
}

/**
 * @brief Add a set of inputs
 * 
 * @param inputs 
 */
void ZXGraph::addInputs(const ZXVertexList&  inputs) {
    _inputs.insert(inputs.begin(), inputs.end());
}

/**
 * @brief Add a set of outputs
 * 
 * @param outputs 
 */
void ZXGraph::addOutputs(const ZXVertexList&  outputs) {
    _outputs.insert(outputs.begin(), outputs.end());
}

/**
 * @brief Add a set of vertices
 * 
 * @param vertices 
 * @param reordered 
 */
void ZXGraph::addVertices(const ZXVertexList& vertices, bool reordered) {
    //REVIEW - Reordered Id
    if(reordered){
        for(const auto& v: vertices) {
            v->setId(_nextVId);
            _nextVId++;
        }
    }
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

    if (vs->isNeighbor(vt, et)) {
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


/**
 * @brief Remove `v` in ZXGraph and maintain the relationship between each vertex.
 *
 * @param v
 */
size_t ZXGraph::removeVertex(ZXVertex* v) {
    if (!_vertices.contains(v)) return 0;
    
    auto vNeighbors = v->getNeighbors();
    for(const auto& n: vNeighbors) {
        v -> removeNeighbor(n);
        ZXVertex* nv = n.first;
        EdgeType ne = n.second;
        nv -> removeNeighbor(make_pair(v, ne));
    }
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

    if (verbose >= 5) cout << "Remove ID: " << v->getId() << endl;
    // deallocate ZXVertex
    delete v;
    return 1;
}

/**
 * @brief Remove all vertex in vertices by calling `removeVertex(ZXVertex* v, bool checked)`
 *
 * @param vertices
 */
size_t ZXGraph::removeVertices(vector<ZXVertex*> vertices) {
    size_t count = 0;
    for (const auto& v : vertices) {
        //REVIEW should call removeVertex
        count += removeVertex(v);
    }
    return count;
}

/**
 * @brief Remove vertex by vertex's id. (Used in ZXCmd.cpp)
 *
 * @param id
 */
// size_t ZXGraph::removeVertexById(const size_t& id) {
//     ZXVertex* v = findVertexById(id);
//     if (v != nullptr)
//         removeVertex(v);
//     else
//         cerr << "Error: The ZXGraph does not contain vertex with id " << v->getId() <<" !!" << endl;
// }

/**
 * @brief Remove all vertices with no neighbor.
 *
 */
size_t ZXGraph::removeIsolatedVertices() {
    vector<ZXVertex*> removing;
    for (const auto& v : _vertices) {
        if (v->getNumNeighbors() == 0) {
            removing.push_back(v);
        }
    }
    return removeVertices(removing);
}

/**
 * @brief Remove all edges between `vs` and `vt` by pointer.
 *
 * @param vs
 * @param vt
 * @param checked
 */
size_t ZXGraph::removeAllEdgesBetween(ZXVertex* vs, ZXVertex* vt, bool checked) {
    return removeEdge(vs, vt, EdgeType::SIMPLE) + removeEdge(vs, vt, EdgeType::HADAMARD);
}

/**
 * @brief Remove an edge between `vs` and `vt`, with EdgeType `etype`
 * 
 * @param vs
 * @param vt
 * @param etype
 */
size_t ZXGraph::removeEdge(ZXVertex* vs, ZXVertex* vt, EdgeType etype) {
    size_t count = vs->removeNeighbor(vt, etype) + vt->removeNeighbor(vs, etype);
    if (count == 1) {
        throw out_of_range("Graph connection error in " + to_string(vs->getId()) + " and " + to_string(vt->getId()));
    }
    if (count == 2) {
        if (verbose >= 5) cout << "Remove edge ( " << vs->getId() << ", " << vt->getId() << " ), type: " << EdgeType2Str(etype) << endl;
    }
    return count / 2;
}

/**
 * @brief Remove an edge exactly equal to `ep`.
 *
 * @param ep
 */
size_t ZXGraph::removeEdge(const EdgePair& ep) {
    return removeEdge(ep.first.first, ep.first.second, ep.second);
}

size_t ZXGraph::removeEdges(const vector<EdgePair>& eps) {
    size_t count = 0;
    for (const auto& ep : eps) {
        count += removeEdge(ep);
    }
    return count;
}


/**
 * @brief Remove all edges between `vs` and `vt` by vertex's id.
 *
 * @param id_s
 * @param id_t
 * @param etype 
 */
// void ZXGraph::removeEdgeById(const size_t& id_s, const size_t& id_t, EdgeType etype) {
//     ZXVertex* vs = findVertexById(id_s);
//     if (!vs) {
//         cerr << "Error: The ZXGraph does not contain vertex with id " << vs->getId() <<" !!" << endl;
//         return;
//     }
//     ZXVertex* vt = findVertexById(id_t);
//     if (!vt) {
//         cerr << "Error: The ZXGraph does not contain vertex with id " << vt->getId() <<" !!" << endl;
//         return;
//     }
    
//     if (etype == EdgeType::ERRORTYPE) {
//         removeAllEdgesBetween(vs, vt);
//     } else {
//         removeEdge(makeEdgePair(vs, vt, etype));
//     }
// }

/**
 * @brief adjoint the zxgraph
 * 
 */
void ZXGraph::adjoint() {
    swap(_inputs, _outputs);
    swap(_inputList, _outputList);
    for (ZXVertex* const& v : _vertices) v->setPhase(-v->getPhase());
}

/**
 * @brief Assign rotation/value to the specified boundary 
 *
 * @param qubit
 * @param isInput
 * @param ty
 * @param phase
 */
void ZXGraph::assignBoundary(size_t qubit, bool isInput, VertexType vt, Phase phase){
    ZXVertex* v = addVertex(qubit, vt, phase);
    ZXVertex* boundary = isInput ? _inputList[qubit] : _outputList[qubit];
    for (auto& [nb, etype] : boundary->getNeighbors()) {
        addEdge(v, nb, etype);
    }
    removeVertex(boundary);
}


/*****************************************************/
/*   class ZXGraph Find functions.                   */
/*****************************************************/

/**
 * @brief Find the next id that is never been used.
 *`
 * @return size_t
 */
size_t ZXGraph::findNextId() const {
    size_t nextId = 0;
    for (auto& v : _vertices) {
        if (v->getId() >= nextId) nextId = v->getId() + 1;
    }
    return nextId;
}

/**
 * @brief Find Vertex by vertex's id.
 *
 * @param id
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::findVertexById(const size_t& id) const {
    for(const auto& v: _vertices){
        if(v->getId() == id) return v;
    }
    return nullptr;
}



/*****************************************************/
/*   class ZXGraph Action functions                  */
/*****************************************************/

/**
 * @brief Reset a ZX-graph (make empty)
 * 
 */
void ZXGraph::reset() {
    _inputs_depr.clear();
    _outputs_depr.clear();
    _vertices_depr.clear();
    _edges_depr.clear();
    _inputList.clear();
    _outputList.clear();
    _topoOrder.clear();
    _globalDFScounter = 1;
}

/**
 * @brief Toggle EdgeType that connected to `v`. ( H -> S / S -> H)
 *        Ex: [(3, S), (4, H), (5, S)] -> [(3, H), (4, S), (5, H)]
 * 
 * @param v 
 */
void ZXGraph::toggleEdges(ZXVertex* v){
    Neighbors toggledNeighbors;
    for (auto& itr : v->getNeighbors()){
        toggledNeighbors.insert(make_pair(itr.first, toggleEdge(itr.second)));
        itr.first->removeNeighbor(make_pair(v, itr.second));
        itr.first->addNeighbor(make_pair(v, toggleEdge(itr.second)));
    }
    v->setNeighbors(toggledNeighbors);
}

/**
 * @brief Copy an identitcal ZX-graph
 * 
 * @return ZXGraph* 
 */
ZXGraph* ZXGraph::copy() const {
    ZXGraph* newGraph = new ZXGraph(0);
    // Copy all vertices (included i/o) first
    unordered_map<ZXVertex*, ZXVertex*> oldV2newVMap;
    for(const auto& v : _vertices) {
        if(v->getType() == VertexType::BOUNDARY){
            if(_inputs.contains(v)) oldV2newVMap[v] = newGraph->addInput(v->getQubit());
            else oldV2newVMap[v] = newGraph->addOutput(v->getQubit());

        }
        else if(v->getType() == VertexType::Z || v->getType() == VertexType::X || v->getType() == VertexType::H_BOX){
            oldV2newVMap[v] = newGraph->addVertex(v->getQubit(), v->getType(), v->getPhase());
        }
    }
    // Link all edges
    // cout << "Link all edges" << endl;
    // unordered_map<size_t, ZXVertex*> id2VertexMap = newGraph->id2VertexMap();
    forEachEdge([&oldV2newVMap, newGraph](const EdgePair& epair){
        newGraph->addEdge(oldV2newVMap[epair.first.first], oldV2newVMap[epair.first.second], epair.second);
    });
    return newGraph;
}

/**
 * @brief Compose `target` to the original ZX-graph (horizontal concat)
 * 
 * @param target 
 * @return ZXGraph* 
 */
ZXGraph* ZXGraph::compose(ZXGraph* target){
    // Check ori-outputNum == target-inputNum
    if(this->getNumOutputs() != target->getNumInputs()) cerr << "";
    else{
        ZXGraph* copiedGraph = target->copy();
    }
    return this;
}

/**
 * @brief Tensor `target` to the original ZX-graph (vertical concat)
 * 
 * @param target 
 * @return ZXGraph* 
 */
ZXGraph* ZXGraph::tensorProduct(ZXGraph* target){
    ZXGraph* copiedGraph = target->copy();

    // Lift Qubit
    int oriMaxQubit = INT_MIN, oriMinQubit = INT_MAX;
    int copiedMinQubit = INT_MAX;
    for(const auto& i : getInputs()){
        if(i->getQubit() > oriMaxQubit) oriMaxQubit = i->getQubit();
        if(i->getQubit() < oriMinQubit) oriMinQubit = i->getQubit();
    }
    for(const auto& i : getOutputs()){
        if(i->getQubit() > oriMaxQubit) oriMaxQubit = i->getQubit();
        if(i->getQubit() < oriMinQubit) oriMinQubit = i->getQubit();
    }

    for(const auto& i : copiedGraph->getInputs()){
        if(i->getQubit() < copiedMinQubit) copiedMinQubit = i->getQubit();
    }
    for(const auto& i : copiedGraph->getOutputs()){
        if(i->getQubit() < copiedMinQubit) copiedMinQubit = i->getQubit();
    }
    size_t liftQ = (oriMaxQubit-oriMinQubit+1) - copiedMinQubit;
    copiedGraph->liftQubit(liftQ);

    // Update Id of copiedGraph to make them unique to the original graph
    for(const auto& v : copiedGraph->getVertices()){
        v->setId(_nextVId);
        _nextVId++;
    }
    
    // Merge copiedGraph to original graph
    this->addInputs(copiedGraph->getInputs());
    this->addOutputs(copiedGraph->getOutputs());
    this->addVertices(copiedGraph->getVertices());
    this->mergeInputList(copiedGraph->getInputList());
    this->mergeOutputList(copiedGraph->getOutputList());

    return this;
}

// void ZXGraph::sortIOByQubit() {
//     // NOTE - when re-implementing, notice that to sort ordered_hashset, use the member function (oset.sort()) instead of std::sort()
//     // sort(_inputs_depr.begin(), _inputs_depr.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
//     // sort(_outputs_depr.begin(), _outputs_depr.end(), [](ZXVertex* a, ZXVertex* b) { return a->getQubit() < b->getQubit(); });
// }

// void ZXGraph::sortVerticeById() {
//     // sort(_vertices_depr.begin(), _vertices_depr.end(), [](ZXVertex* a, ZXVertex* b) { return a->getId() < b->getId(); });
// }

/**
 * @brief Lift each vertex's qubit in ZX-graph with `n`.
 *        Ex: origin: 0 -> after lifting: n
 * 
 * @param n 
 */
void ZXGraph::liftQubit(const size_t& n) {
    for(const auto& v : _vertices){ v->setQubit(v->getQubit() + n); }

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

/**
 * @brief Generate a id-2-ZXVertex* map
 * 
 * @return unordered_map<size_t, ZXVertex*> 
 */
unordered_map<size_t, ZXVertex*> ZXGraph::id2VertexMap() const{
    unordered_map<size_t, ZXVertex*> id2VertexMap;
    for(const auto& v : _vertices) id2VertexMap[v->getId()] = v;
    return id2VertexMap;
}



/*****************************************************/
/*   class ZXGraph Print functions                   */
/*****************************************************/

/**
 * @brief Print information of ZX-graph
 * 
 */
void ZXGraph::printGraph() const {
    cout << "Graph " << _id << "( "
         << getNumInputs() << " inputs, "
         << getNumOutputs() << " outputs, "
         << getNumVertices() << " vertices, "
         << getNumEdges() << " edges )\n";
    // cout << setw(15) << left << "Inputs: " << getNumInputs() << endl;
    // cout << setw(15) << left << "Outputs: " << getNumOutputs() << endl;
    // cout << setw(15) << left << "Vertices: " << getNumVertices() << endl;
    // cout << setw(15) << left << "Edges: " << getNumEdges() << endl;
}

/**
 * @brief Print Inputs of ZX-graph
 * 
 */
void ZXGraph::printInputs() const {
    cout << "Input ( ";
    for (const auto& v : _inputs) cout << v->getId() << " ";
    cout << ")\nTotal #Inputs: " << getNumInputs() << endl;
}

/**
 * @brief Print Outputs of ZX-graph
 * 
 */
void ZXGraph::printOutputs() const {
    cout << "Output ( ";
    for (const auto& v : _outputs) cout << v->getId() << " ";
    cout << ")\nTotal #Outputs: " << getNumOutputs() << endl;
}

/**
 * @brief Print Inputs and Outputs of ZX-graph
 * 
 */
void ZXGraph::printIO() const {
    cout << "Input ( ";
    for (const auto& v : _inputs) cout << v->getId() << " ";
    cout << ")\nOutput ( ";
    for (const auto& v : _outputs) cout << v->getId() << " ";
    cout << ")\nTotal #(I,O): (" << getNumInputs() << "," << getNumOutputs() << ")\n";
}

/**
 * @brief Print Vertices of ZX-graph
 * 
 */
void ZXGraph::printVertices() const {
    cout << "\n";
    for (const auto& v : _vertices) {
        v->printVertex();
    }
    cout << "Total #Vertices: " << getNumVertices() << endl;
    cout << "\n";
}

/**
 * @brief Print Edges of ZX-graph
 * 
 */
void ZXGraph::printEdges() const {
    forEachEdge([](const EdgePair& epair) {
        cout << "( " << epair.first.first->getId() << ", " << epair.first.second->getId() << " )\tType:\t" << EdgeType2Str(epair.second) << endl;
    });
    cout << "Total #Edges: " << getNumEdges() << endl;
}



/*****************************************************/
/*   Vertex Type & Edge Type functions               */
/*****************************************************/

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
