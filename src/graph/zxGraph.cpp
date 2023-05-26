/****************************************************************************
  FileName     [ zxGraph.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxGraph.h"

#include <iostream>

#include "textFormat.h"  // for TextFormat
#include "zxDef.h"

using namespace std;

namespace TF = TextFormat;
extern size_t verbose;

/*****************************************************/
/*   class ZXGraph Getter and setter functions       */
/*****************************************************/

/**
 * @brief Add procedures to ZXGraph
 *
 * @param p
 * @param procedures
 */
void ZXGraph::addProcedure(std::string p, const std::vector<std::string>& procedures) {
    for (auto pr : procedures)
        _procedures.emplace_back(pr);
    if (p != "") _procedures.emplace_back(p);
}

/**
 * @brief Get the number of edges in ZX-graph
 *
 * @return size_t
 */
size_t ZXGraph::getNumEdges() const {
    size_t n = 0;
    for (auto& v : _vertices) {
        n += v->getNumNeighbors();
    }
    return n / 2;
}

/*****************************************************/
/*   class ZXGraph Testing functions                 */
/*****************************************************/

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
    for (auto& v : _inputs) {
        if (v->getNumNeighbors() != 1) return false;
    }
    for (auto& v : _outputs) {
        if (v->getNumNeighbors() != 1) return false;
    }
    for (auto& v : _vertices) {
        for (auto& [nb, etype] : v->getNeighbors()) {
            if (!nb->getNeighbors().contains(make_pair(v, etype))) return false;
        }
    }
    return true;
}

/**
 * @brief Generate a CNOT subgraph to the ZXGraph (testing)
 *
 */
void ZXGraph::generateCNOT() {
    if (isEmpty()) {
        if (verbose >= 5) cout << "Generate a 2-qubit CNOT graph for testing\n";
        ZXVertex* i0 = addInput(0, 0, 0);
        ZXVertex* i1 = addInput(1, 0, 0);
        ZXVertex* vz = addVertex(0, VertexType::Z, Phase(), 0, 1);
        ZXVertex* vx = addVertex(1, VertexType::X, Phase(), 0, 1);
        ZXVertex* o0 = addOutput(0, 0, 2);
        ZXVertex* o1 = addOutput(1, 0, 2);

        addEdge(i0, vz, EdgeType::SIMPLE);
        addEdge(i1, vx, EdgeType::SIMPLE);
        addEdge(vz, vx, EdgeType::SIMPLE);
        addEdge(o0, vz, EdgeType::SIMPLE);
        addEdge(o1, vx, EdgeType::SIMPLE);
    } else {
        if (verbose >= 3) cout << "Note: The graph is not empty! Generation failed!\n";
    }
}

/**
 * @brief Check if `id` exists
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
 * @brief Check if ZXGraph is graph-like, report first error
 *
 * @return true
 * @return false
 */
bool ZXGraph::isGraphLike() const {
    // all internal edges are hadamard edges
    for (const auto& v : _vertices) {
        if (!v->isZ() && !v->isBoundary()) {
            if (verbose >= 5) {
                cout << "Note: vertex " << v->getId() << " is of type " << VertexType2Str(v->getType()) << endl;
            }
            return false;
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
 * @brief Check if ZXGraph is identity
 *
 * @return true
 * @return false
 */
bool ZXGraph::isIdentity() const {
    return all_of(_inputs.begin(), _inputs.end(), [this](ZXVertex* i) {
        return (i->getNumNeighbors() == 1) &&
               _outputs.contains(i->getFirstNeighbor().first) &&
               i->getFirstNeighbor().first->getQubit() == i->getQubit();
    });
}

size_t ZXGraph::numGadgets() const {
    return count_if(getVertices().begin(), getVertices().end(), [](ZXVertex* v) {
        return !v->isBoundary() && v->getNumNeighbors() == 1;
    });
}

/**
 * @brief Return the number of T-gate in the ZX-graph
 *
 * @return int
 */
int ZXGraph::TCount() const {
    return count_if(_vertices.begin(), _vertices.end(), [](ZXVertex* v) {
        return (v->getPhase().denominator() == 4);
    });
}

/**
 * @brief Return the number of non-clifford(and T) gate
 *
 * @param includeT if true, T gate will be counted
 * @return int
 */
int ZXGraph::nonCliffordCount(bool includeT) const {
    int num = 0;
    if (includeT) {
        for (const auto& v : _vertices) {
            if (v->getPhase().denominator() != 1 &&
                v->getPhase().denominator() != 2) num++;
        }
    } else {
        for (const auto& v : _vertices) {
            if (v->getPhase().denominator() != 1 &&
                v->getPhase().denominator() != 2 &&
                v->getPhase().denominator() != 4) num++;
        }
    }
    return num;
}

/*****************************************************/
/*   class ZXGraph Add functions                     */
/*****************************************************/

/**
 * @brief Add input to ZXVertexList
 *
 * @param qubit
 * @param checked
 * @param col
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::addInput(int qubit, bool checked, unsigned int col) {
    if (!checked) {
        if (isInputQubit(qubit)) {
            cerr << "Error: This qubit's input already exists!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = addVertex(qubit, VertexType::BOUNDARY, Phase(), true, col);
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
ZXVertex* ZXGraph::addOutput(int qubit, bool checked, unsigned int col) {
    if (!checked) {
        if (isOutputQubit(qubit)) {
            cerr << "Error: This qubit's output already exists!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = addVertex(qubit, VertexType::BOUNDARY, Phase(), true, col);
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
ZXVertex* ZXGraph::addVertex(int qubit, VertexType vt, Phase phase, bool checked, unsigned int col) {
    if (!checked) {
        if (vt == VertexType::BOUNDARY) {
            cerr << "Error: Use ADDInput / ADDOutput to add input vertex or output vertex!!" << endl;
            return nullptr;
        }
    }
    ZXVertex* v = new ZXVertex(_nextVId, qubit, vt, phase, col);
    _vertices.emplace(v);
    if (verbose >= 8) cout << "Add vertex (" << VertexType2Str(vt) << ") " << _nextVId << endl;
    _nextVId++;
    return v;
}

/**
 * @brief Add a set of inputs
 *
 * @param inputs
 */
void ZXGraph::addInputs(const ZXVertexList& inputs) {
    _inputs.insert(inputs.begin(), inputs.end());
}

/**
 * @brief Add a set of outputs
 *
 * @param outputs
 */
void ZXGraph::addOutputs(const ZXVertexList& outputs) {
    _outputs.insert(outputs.begin(), outputs.end());
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
        if (verbose >= 8) cout << "Note: converting this self-loop to phase " << phase << " on vertex " << vs->getId() << "..." << endl;
        vs->setPhase(vs->getPhase() + phase);
        return makeEdgePairDummy();
    }

    if (vs->getId() > vt->getId()) swap(vs, vt);

    if (vs->isNeighbor(vt, et)) {
        if (
            (vs->isZ() && vt->isX() && et == EdgeType::HADAMARD) ||
            (vs->isX() && vt->isZ() && et == EdgeType::HADAMARD) ||
            (vs->isZ() && vt->isZ() && et == EdgeType::SIMPLE) ||
            (vs->isX() && vt->isX() && et == EdgeType::SIMPLE)) {
            if (verbose >= 8) cout << "Note: redundant edge; merging into existing edge (" << vs->getId() << ", " << vt->getId() << ")..." << endl;
        } else if (
            (vs->isZ() && vt->isX() && et == EdgeType::SIMPLE) ||
            (vs->isX() && vt->isZ() && et == EdgeType::SIMPLE) ||
            (vs->isZ() && vt->isZ() && et == EdgeType::HADAMARD) ||
            (vs->isX() && vt->isX() && et == EdgeType::HADAMARD)) {
            if (verbose >= 8) cout << "Note: Hopf edge; cancelling out with existing edge (" << vs->getId() << ", " << vt->getId() << ")..." << endl;
            vs->removeNeighbor(make_pair(vt, et));
            vt->removeNeighbor(make_pair(vs, et));
        }
        // REVIEW - similar conditions for H-Boxes and boundaries?
    } else {
        vs->addNeighbor(make_pair(vt, et));
        vt->addNeighbor(make_pair(vs, et));
        if (verbose >= 8) cout << "Add edge (" << vs->getId() << ", " << vt->getId() << ")" << endl;
    }

    return makeEdgePair(vs, vt, et);
}

/**
 * @brief Add a set of vertices
 *
 * @param vertices
 * @param reordered
 */
void ZXGraph::addVertices(const ZXVertexList& vertices, bool reordered) {
    if (reordered) {
        for (const auto& v : vertices) {
            v->setId(_nextVId);
            _nextVId++;
        }
    }
    _vertices.insert(vertices.begin(), vertices.end());
}

/*****************************************************/
/*   class ZXGraph Remove functions                  */
/*****************************************************/

/**
 * @brief Remove all vertices with no neighbor.
 *
 */
size_t ZXGraph::removeIsolatedVertices() {
    vector<ZXVertex*> rmList;
    for (const auto& v : _vertices) {
        if (v->getNumNeighbors() == 0) rmList.push_back(v);
    }
    return removeVertices(rmList);
}

/**
 * @brief Remove `v` in ZXGraph and maintain the relationship between each vertex.
 *
 * @param v
 */
size_t ZXGraph::removeVertex(ZXVertex* v) {
    if (!_vertices.contains(v)) return 0;

    auto vNeighbors = v->getNeighbors();
    for (const auto& n : vNeighbors) {
        v->removeNeighbor(n);
        ZXVertex* nv = n.first;
        EdgeType ne = n.second;
        nv->removeNeighbor(make_pair(v, ne));
    }
    _vertices.erase(v);

    // Check if also in _inputs or _outputs
    if (_inputs.contains(v)) {
        _inputList.erase(v->getQubit());
        _inputs.erase(v);
    }
    if (_outputs.contains(v)) {
        _outputList.erase(v->getQubit());
        _outputs.erase(v);
    }

    if (verbose >= 8) cout << "Remove ID: " << v->getId() << endl;
    // deallocate ZXVertex
    delete v;
    return 1;
}

/**
 * @brief Remove all vertex in vertices by calling `removeVertex(ZXVertex* v, bool checked)`
 *
 * @param vertices
 */
size_t ZXGraph::removeVertices(vector<ZXVertex*> const& vertices) {
    return std::transform_reduce(
        vertices.begin(), vertices.end(), 0, std::plus{}, [this](ZXVertex* v) {
            return removeVertex(v);
        });
}

/**
 * @brief Remove an edge exactly equal to `ep`.
 *
 * @param ep
 */
size_t ZXGraph::removeEdge(const EdgePair& ep) {
    return removeEdge(ep.first.first, ep.first.second, ep.second);
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
        if (verbose >= 8) cout << "Remove edge (" << vs->getId() << ", " << vt->getId() << "), type: " << EdgeType2Str(etype) << endl;
    }
    return count / 2;
}

/**
 * @brief Remove each ep in `eps` by calling `ZXGraph::removeEdge`
 *
 * @param eps
 * @return size_t
 */
size_t ZXGraph::removeEdges(vector<EdgePair> const& eps) {
    return std::transform_reduce(
        eps.begin(), eps.end(), 0, std::plus{}, [this](EdgePair const& ep) {
            return removeEdge(ep);
        });
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

/*****************************************************/
/*   class ZXGraph Operation on graph functions.     */
/*****************************************************/

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
void ZXGraph::assignBoundary(int qubit, bool isInput, VertexType vt, Phase phase) {
    ZXVertex* v = addVertex(qubit, vt, phase);
    ZXVertex* boundary = isInput ? _inputList[qubit] : _outputList[qubit];
    for (auto& [nb, etype] : boundary->getNeighbors()) {
        addEdge(v, nb, etype);
    }
    removeVertex(boundary);
}

/**
 * @brief transfer the phase of the specified vertex to a unary gadget. This function does nothing
 *        if the target vertex is not a Z-spider.
 *
 * @param v
 * @param keepPhase if specified, keep this amount of phase on the vertex and only transfer the rest.
 */
void ZXGraph::transferPhase(ZXVertex* v, const Phase& keepPhase) {
    if (!v->isZ()) return;
    ZXVertex* leaf = this->addVertex(-2, VertexType::Z, v->getPhase() - keepPhase, true);
    ZXVertex* buffer = this->addVertex(-1, VertexType::Z, Phase(0), true);
    // REVIEW - No floating, directly take v
    leaf->setCol(v->getCol());
    buffer->setCol(v->getCol());
    v->setPhase(keepPhase);

    this->addEdge(leaf, buffer, EdgeType::HADAMARD);
    this->addEdge(buffer, v, EdgeType::HADAMARD);
}

// REVIEW - Probably needs better name and description. Helps are appreciated.
/**
 * @brief Add a Z-spider to buffer a vertex from another vertex, so that they don't come in
 *        contact with each other on the edge with specified edge type. If such edge does not
 *        exists, this function does nothing.
 *
 * @param toProtect the vertex to protect
 * @param fromVertex the vertex to buffer from
 * @param etype the edgetype the buffer should be added on
 */
ZXVertex* ZXGraph::addBuffer(ZXVertex* toProtect, ZXVertex* fromVertex, EdgeType etype) {
    if (!toProtect->isNeighbor(fromVertex, etype)) return nullptr;

    ZXVertex* bufferVertex = this->addVertex(toProtect->getQubit(), VertexType::Z, Phase(0), true);

    this->addEdge(toProtect, bufferVertex, toggleEdge(etype));
    this->addEdge(bufferVertex, fromVertex, EdgeType::HADAMARD);
    this->removeEdge(toProtect, fromVertex, etype);
    // REVIEW - Float version
    bufferVertex->setCol((toProtect->getCol() + fromVertex->getCol()) / 2);
    return bufferVertex;
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
    for (const auto& v : _vertices) {
        if (v->getId() == id) return v;
    }
    return nullptr;
}
