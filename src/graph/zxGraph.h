/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph structures ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_H
#define ZX_GRAPH_H

#include <complex>
#include <cstddef>  // for size_t, NULL
#include <string>   // for string

#include "phase.h"  // for Phase
#include "zxDef.h"  // for VertexType, EdgeType, EdgePair, NeighborPair

template <typename T>
class QTensor;

class ZXVertex;
class ZXGraph;

// See `zxVertex.cpp` for details
EdgeType str2EdgeType(const std::string& str);
VertexType str2VertexType(const std::string& str);
std::string EdgeType2Str(const EdgeType& et);
std::string VertexType2Str(const VertexType& vt);
EdgeType toggleEdge(const EdgeType& et);

EdgePair makeEdgePair(ZXVertex* v1, ZXVertex* v2, EdgeType et);
EdgePair makeEdgePair(EdgePair epair);
EdgePair makeEdgePairDummy();

class ZXVertex {
    // See `zxVertex.cpp` for details
public:
    ZXVertex(size_t id, int qubit, VertexType vt, Phase phase = Phase(), unsigned int col = 0) {
        _id = id;
        _type = vt;
        _qubit = qubit;
        _phase = phase;
        _col = col;
        _DFSCounter = 0;
        _pin = unsigned(-1);
        _neighbors.clear();
    }
    ~ZXVertex() {}

    // Getter and Setter

    const size_t& getId() const { return _id; }
    const int& getQubit() const { return _qubit; }
    const size_t& getPin() const { return _pin; }
    const Phase& getPhase() const { return _phase; }
    const VertexType& getType() const { return _type; }
    const float& getCol() const { return _col; }
    const Neighbors& getNeighbors() const { return _neighbors; }
    const NeighborPair& getFirstNeighbor() const { return *(_neighbors.begin()); }
    const NeighborPair& getSecondNeighbor() const { return *next((_neighbors.begin())); }

    std::vector<ZXVertex*> getCopiedNeighbors();
    size_t getNumNeighbors() const { return _neighbors.size(); }
    QTensor<double> getTSform();

    void setId(const size_t& id) { _id = id; }
    void setQubit(const int& q) { _qubit = q; }
    void setPin(const size_t& p) { _pin = p; }
    void setPhase(const Phase& p) { _phase = p; }
    void setCol(const float& c) { _col = c; }
    void setType(const VertexType& vt) { _type = vt; }
    void setNeighbors(const Neighbors& n) { _neighbors = n; }

    // Add and Remove
    void addNeighbor(const NeighborPair& n) { _neighbors.insert(n); }
    size_t removeNeighbor(const NeighborPair& n) { return _neighbors.erase(n); }
    size_t removeNeighbor(ZXVertex* v, EdgeType et) { return removeNeighbor(std::make_pair(v, et)); }

    // Print functions
    void printVertex() const;
    void printNeighbors() const;

    // Action
    void disconnect(ZXVertex* v, bool checked = false);

    // Test
    bool isZ() const { return getType() == VertexType::Z; }
    bool isX() const { return getType() == VertexType::X; }
    bool isHBox() const { return getType() == VertexType::H_BOX; }
    bool isBoundary() const { return getType() == VertexType::BOUNDARY; }
    bool isNeighbor(ZXVertex* v) const { return _neighbors.contains(std::make_pair(v, EdgeType::SIMPLE)) || _neighbors.contains(std::make_pair(v, EdgeType::HADAMARD)); }
    bool isNeighbor(const NeighborPair& n) const { return _neighbors.contains(n); }
    bool isNeighbor(ZXVertex* v, EdgeType et) const { return isNeighbor(std::make_pair(v, et)); }
    bool hasNPiPhase() const { return _phase.denominator() == 1; }

    // DFS
    bool isVisited(unsigned global) { return global == _DFSCounter; }
    void setVisited(unsigned global) { _DFSCounter = global; }

private:
    size_t _id;
    int _qubit;
    size_t _pin;
    Phase _phase;
    unsigned _DFSCounter;
    Neighbors _neighbors;
    VertexType _type;
    float _col;
};

class ZXGraph {
public:
    ZXGraph(size_t id) : _id(id), _nextVId(0) {
        _globalTraCounter = 1;
    }

    ~ZXGraph() {
        for (const auto& v : _vertices) delete v;
    }

    // Getter and Setter
    void setId(size_t id) { _id = id; }

    void setInputs(const ZXVertexList& inputs) { _inputs = inputs; }
    void setOutputs(const ZXVertexList& outputs) { _outputs = outputs; }
    void setVertices(const ZXVertexList& vertices) { _vertices = vertices; }
    void setFileName(std::string f) { _fileName = f; }
    void addProcedure(std::string = "", const std::vector<std::string>& = {});

    const size_t& getId() const { return _id; }
    const size_t& getNextVId() const { return _nextVId; }
    const ZXVertexList& getInputs() const { return _inputs; }
    const ZXVertexList& getOutputs() const { return _outputs; }
    const ZXVertexList& getVertices() const { return _vertices; }
    size_t getNumEdges() const;
    size_t getNumInputs() const { return _inputs.size(); }
    size_t getNumOutputs() const { return _outputs.size(); }
    size_t getNumVertices() const { return _vertices.size(); }
    std::string getFileName() const { return _fileName; }
    const std::vector<std::string>& getProcedures() const { return _procedures; }

    // For testings
    bool isEmpty() const;
    bool isValid() const;
    void generateCNOT();
    bool isId(size_t id) const;
    bool isGraphLike() const;
    bool isIdentity() const;
    size_t numGadgets() const;
    bool isInputQubit(int qubit) const { return (_inputList.contains(qubit)); }
    bool isOutputQubit(int qubit) const { return (_outputList.contains(qubit)); }

    bool isGadgetLeaf(ZXVertex*) const;
    bool isGadgetAxel(ZXVertex*) const;
    bool hasDanglingNeighbors(ZXVertex*) const;

    int TCount() const;
    int nonCliffordCount(bool includeT = false) const;

    // Add and Remove
    ZXVertex* addInput(int qubit, bool checked = false, unsigned int col = 0);
    ZXVertex* addOutput(int qubit, bool checked = false, unsigned int col = 0);
    ZXVertex* addVertex(int qubit, VertexType ZXVertex, Phase phase = Phase(), bool checked = false, unsigned int col = 0);

    void addInputs(const ZXVertexList& inputs);
    void addOutputs(const ZXVertexList& outputs);
    EdgePair addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et);
    void addVertices(const ZXVertexList& vertices, bool reordered = false);

    size_t removeIsolatedVertices();
    size_t removeVertex(ZXVertex* v);
    size_t removeVertices(std::vector<ZXVertex*> const& vertices);
    size_t removeEdge(const EdgePair& ep);
    size_t removeEdge(ZXVertex* vs, ZXVertex* vt, EdgeType etype);
    size_t removeEdges(std::vector<EdgePair> const& eps);
    size_t removeAllEdgesBetween(ZXVertex* vs, ZXVertex* vt, bool checked = false);

    // Operation on graph
    void adjoint();
    void assignBoundary(int qubit, bool input, VertexType type, Phase phase);

    // helper functions for simplifiers
    void transferPhase(ZXVertex* v, const Phase& keepPhase = Phase(0));
    ZXVertex* addBuffer(ZXVertex* toProtect, ZXVertex* fromVertex, EdgeType etype);

    // Find functions
    size_t findNextId() const;
    ZXVertex* findVertexById(const size_t& id) const;

    // Action functions (zxGraphAction.cpp)
    void reset();
    void sortIOByQubit();
    ZXGraph* copy(bool doReordering = true) const;
    void toggleEdges(ZXVertex* v);
    void liftQubit(const size_t& n);
    ZXGraph* compose(ZXGraph* target);
    ZXGraph* tensorProduct(ZXGraph* target);
    void addGadget(Phase p, const std::vector<ZXVertex*>& verVec);
    void removeGadget(ZXVertex* v);
    std::unordered_map<size_t, ZXVertex*> id2VertexMap() const;
    void mergeInputList(std::unordered_map<size_t, ZXVertex*> lst) { _inputList.merge(lst); }
    void mergeOutputList(std::unordered_map<size_t, ZXVertex*> lst) { _outputList.merge(lst); }
    void disownVertices();

    // Print functions (zxGraphPrint.cpp)
    void printGraph() const;
    void printInputs() const;
    void printOutputs() const;
    void printIO() const;
    void printVertices() const;
    void printVertices(std::vector<size_t> cand) const;
    void printQubits(std::vector<int> cand = {}) const;
    void printEdges() const;

    void printDifference(ZXGraph* other) const;
    void draw() const;

    // For mapping (in zxMapping.cpp)
    void toTensor();
    ZXVertexList getNonBoundary();
    ZXVertex* getInputFromHash(const size_t& q);
    ZXVertex* getOutputFromHash(const size_t& q);
    void concatenate(ZXGraph* tmp);
    void setInputHash(const size_t& q, ZXVertex* v) { _inputList[q] = v; }
    void setOutputHash(const size_t& q, ZXVertex* v) { _outputList[q] = v; }
    void setInputList(const std::unordered_map<size_t, ZXVertex*>& lst) { _inputList = lst; }
    void setOutputList(const std::unordered_map<size_t, ZXVertex*>& lst) { _outputList = lst; }
    const std::unordered_map<size_t, ZXVertex*>& getInputList() const { return _inputList; }
    const std::unordered_map<size_t, ZXVertex*>& getOutputList() const { return _outputList; }

    // I/O (in zxIO.cpp)
    bool readZX(const std::string& filename, bool keepID = false);
    bool writeZX(const std::string& filename, bool complete = false);
    bool writeTikz(std::string);
    bool writeTex(std::string, bool toPDF = true);

    // Traverse (in zxTraverse.cpp)
    void updateTopoOrder();
    void updateBreadthLevel();
    template <typename F>
    void topoTraverse(F lambda) {
        updateTopoOrder();
        for_each(_topoOrder.begin(), _topoOrder.end(), lambda);
    }
    template <typename F>
    void forEachEdge(F lambda) const {
        for (auto v : _vertices) {
            for (auto [nb, etype] : v->getNeighbors()) {
                if (nb->getId() > v->getId())
                    lambda(makeEdgePair(v, nb, etype));
            }
        }
    }

private:
    size_t _id;
    size_t _nextVId;
    std::string _fileName;
    std::vector<std::string> _procedures;
    ZXVertexList _inputs;
    ZXVertexList _outputs;
    ZXVertexList _vertices;
    std::vector<ZXVertex*> _topoOrder;
    std::unordered_map<size_t, ZXVertex*> _inputList;
    std::unordered_map<size_t, ZXVertex*> _outputList;
    unsigned _globalTraCounter;

    void DFS(ZXVertex*);
    void BFS(ZXVertex*);

    bool buildGraphFromParserStorage(const ZXParserDetail::StorageType& storage, bool keepID = false);
};

#endif