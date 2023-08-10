/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph structures ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_GRAPH_H
#define ZX_GRAPH_H

#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "phase.h"
#include "zxDef.h"

class ZXVertex;
class ZXGraph;

// See `zxVertex.cpp` for details
std::optional<EdgeType> str2EdgeType(const std::string& str);
std::optional<VertexType> str2VertexType(const std::string& str);
std::string EdgeType2Str(const EdgeType& et);
std::string VertexType2Str(const VertexType& vt);
EdgeType toggleEdge(const EdgeType& et);

inline EdgeType concatEdge(EdgeType const& etype) { return etype; }

inline EdgeType concatEdge(EdgeType const& etype, std::convertible_to<EdgeType> auto... others) {
    return (etype == EdgeType::HADAMARD) ^ (concatEdge(others...) == EdgeType::HADAMARD) ? EdgeType::HADAMARD : EdgeType::SIMPLE;
}

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

    void setId(const size_t& id) { _id = id; }
    void setQubit(const int& q) { _qubit = q; }
    void setPin(const size_t& p) { _pin = p; }
    void setPhase(const Phase& p) { _phase = p; }
    void setCol(const float& c) { _col = c; }
    void setType(const VertexType& vt) { _type = vt; }
    void setNeighbors(const Neighbors& n) { _neighbors = n; }

    // Add and Remove
    void addNeighbor(const NeighborPair& n) { _neighbors.insert(n); }
    void addNeighbor(ZXVertex* v, EdgeType et) { _neighbors.emplace(v, et); }
    size_t removeNeighbor(const NeighborPair& n) { return _neighbors.erase(n); }
    size_t removeNeighbor(ZXVertex* v, EdgeType et) { return removeNeighbor(std::make_pair(v, et)); }
    size_t removeNeighbor(ZXVertex* v) { return removeNeighbor(v, EdgeType::SIMPLE) + removeNeighbor(v, EdgeType::HADAMARD); }

    // Print functions
    void printVertex() const;
    void printNeighbors() const;

    // Test
    bool isZ() const { return getType() == VertexType::Z; }
    bool isX() const { return getType() == VertexType::X; }
    bool isHBox() const { return getType() == VertexType::H_BOX; }
    bool isBoundary() const { return getType() == VertexType::BOUNDARY; }
    bool isNeighbor(ZXVertex* v) const { return _neighbors.contains(std::make_pair(v, EdgeType::SIMPLE)) || _neighbors.contains(std::make_pair(v, EdgeType::HADAMARD)); }
    bool isNeighbor(const NeighborPair& n) const { return _neighbors.contains(n); }
    bool isNeighbor(ZXVertex* v, EdgeType et) const { return isNeighbor(std::make_pair(v, et)); }

    std::optional<EdgeType> getEdgeTypeBetween(ZXVertex* v) const {
        if (isNeighbor(v, EdgeType::SIMPLE)) return EdgeType::SIMPLE;
        if (isNeighbor(v, EdgeType::HADAMARD)) return EdgeType::HADAMARD;
        return std::nullopt;
    }
    bool hasNPiPhase() const { return _phase.denominator() == 1; }
    bool isClifford() const { return _phase.denominator() <= 2; }

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
    ZXGraph(size_t id = 0) : _id(id), _nextVId(0) {
        _globalTraCounter = 1;
    }

    ~ZXGraph() {
        for (auto& v : _vertices) {
            delete v;
        }
    }

    ZXGraph(ZXGraph const& other) : _id{other._id}, _nextVId{0}, _fileName{other._fileName}, _procedures{other._procedures} {
        std::unordered_map<ZXVertex*, ZXVertex*> oldV2newVMap;

        for (auto& v : other._vertices) {
            if (v->isBoundary()) {
                if (other._inputs.contains(v))
                    oldV2newVMap[v] = this->addInput(v->getQubit(), v->getCol());
                else
                    oldV2newVMap[v] = this->addOutput(v->getQubit(), v->getCol());
            } else if (v->isZ() || v->isX() || v->isHBox()) {
                oldV2newVMap[v] = this->addVertex(v->getQubit(), v->getType(), v->getPhase(), v->getCol());
            }
        }

        other.forEachEdge([&oldV2newVMap, this](EdgePair&& epair) {
            this->addEdge(oldV2newVMap[epair.first.first], oldV2newVMap[epair.first.second], epair.second);
        });
    }

    ZXGraph(ZXGraph&& other) noexcept {
        _id = std::exchange(other._id, 0);
        _nextVId = std::exchange(other._nextVId, 0);
        _fileName = std::exchange(other._fileName, "");
        _procedures = std::exchange(other._procedures, {});
        _inputs = std::exchange(other._inputs, ZXVertexList{});
        _outputs = std::exchange(other._outputs, ZXVertexList{});
        _vertices = std::exchange(other._vertices, ZXVertexList{});
        _topoOrder = std::exchange(other._topoOrder, std::vector<ZXVertex*>{});
        _inputList = std::exchange(other._inputList, std::unordered_map<size_t, ZXVertex*>{});
        _outputList = std::exchange(other._outputList, std::unordered_map<size_t, ZXVertex*>{});
        _globalTraCounter = std::exchange(other._globalTraCounter, 0);
    }

    /**
     * @brief Construct a new ZXGraph object from its components
     *
     */
    ZXGraph(const ZXVertexList& vertices,
            const ZXVertexList& inputs,
            const ZXVertexList& outputs,
            size_t id) : _id(id), _globalTraCounter(1) {
        _vertices = vertices;
        _inputs = inputs;
        _outputs = outputs;
        _nextVId = 0;
        for (auto v : _vertices) {
            v->setId(_nextVId);
            _nextVId++;
        }
        for (auto v : _inputs) {
            assert(vertices.contains(v));
            _inputList[v->getQubit()] = v;
        }
        for (auto v : _outputs) {
            assert(vertices.contains(v));
            _outputList[v->getQubit()] = v;
        }
    }

    ZXGraph& operator=(ZXGraph copy) {
        copy.swap(*this);
        return *this;
    }

    void release() {
        _nextVId = 0;
        _fileName = "";
        _procedures.clear();
        _inputs.clear();
        _outputs.clear();
        _vertices.clear();
        _topoOrder.clear();
        _inputList.clear();
        _outputList.clear();
        _globalTraCounter = 1;
    }

    void swap(ZXGraph& other) noexcept {
        std::swap(_id, other._id);
        std::swap(_nextVId, other._nextVId);
        std::swap(_fileName, other._fileName);
        std::swap(_procedures, other._procedures);
        std::swap(_inputs, other._inputs);
        std::swap(_outputs, other._outputs);
        std::swap(_vertices, other._vertices);
        std::swap(_topoOrder, other._topoOrder);
        std::swap(_inputList, other._inputList);
        std::swap(_outputList, other._outputList);
        std::swap(_globalTraCounter, other._globalTraCounter);
    }

    friend void swap(ZXGraph& a, ZXGraph& b) noexcept {
        a.swap(b);
    }

    // Getter and Setter
    void setId(size_t id) { _id = id; }

    void setInputs(const ZXVertexList& inputs) { _inputs = inputs; }
    void setOutputs(const ZXVertexList& outputs) { _outputs = outputs; }
    void setFileName(std::string const& f) { _fileName = f; }
    void addProcedures(std::vector<std::string> const& ps) { _procedures.insert(_procedures.end(), ps.begin(), ps.end()); }
    void addProcedure(std::string_view p) { _procedures.emplace_back(p); }

    size_t const& getId() const { return _id; }
    size_t const& getNextVId() const { return _nextVId; }
    ZXVertexList const& getInputs() const { return _inputs; }
    ZXVertexList const& getOutputs() const { return _outputs; }
    ZXVertexList const& getVertices() const { return _vertices; }
    size_t getNumEdges() const;
    size_t getNumInputs() const { return _inputs.size(); }
    size_t getNumOutputs() const { return _outputs.size(); }
    size_t getNumVertices() const { return _vertices.size(); }
    std::string getFileName() const { return _fileName; }
    std::vector<std::string> const& getProcedures() const { return _procedures; }

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

    double density();
    inline size_t TCount() const {
        return std::ranges::count_if(_vertices, [](ZXVertex* v) { return (v->getPhase().denominator() == 4); });
    }
    inline size_t nonCliffordCount() const {
        return std::ranges::count_if(_vertices, [](ZXVertex* v) { return !v->isClifford(); });
    }
    inline size_t nonCliffordPlusTCount() const { return nonCliffordCount() - TCount(); }

    // Add and Remove
    ZXVertex* addInput(int qubit, unsigned int col = 0);
    ZXVertex* addOutput(int qubit, unsigned int col = 0);
    ZXVertex* addVertex(int qubit, VertexType vt, Phase phase = Phase(), unsigned int col = 0);
    EdgePair addEdge(ZXVertex* vs, ZXVertex* vt, EdgeType et);

    size_t removeIsolatedVertices();
    size_t removeVertex(ZXVertex* v);

    size_t removeVertices(std::vector<ZXVertex*> const& vertices);
    size_t removeEdge(const EdgePair& ep);
    size_t removeEdge(ZXVertex* vs, ZXVertex* vt, EdgeType etype);
    size_t removeEdges(std::vector<EdgePair> const& eps);
    size_t removeAllEdgesBetween(ZXVertex* vs, ZXVertex* vt);

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
    void sortIOByQubit();
    void toggleVertex(ZXVertex* v);
    void liftQubit(const size_t& n);
    void relabelVertexIDs(size_t idStart) {
        std::ranges::for_each(this->_vertices, [&idStart](ZXVertex* v) { v->setId(idStart++); });
    }
    ZXGraph& compose(ZXGraph const& target);
    ZXGraph& tensorProduct(ZXGraph const& target);
    void addGadget(Phase p, const std::vector<ZXVertex*>& verVec);
    void removeGadget(ZXVertex* v);
    std::unordered_map<size_t, ZXVertex*> id2VertexMap() const;
    void normalize();

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
    ZXVertexList getNonBoundary();
    ZXVertex* getInputByQubit(const size_t& q);
    ZXVertex* getOutputByQubit(const size_t& q);
    void concatenate(ZXGraph const& tmp);
    std::unordered_map<size_t, ZXVertex*> const& getInputList() const { return _inputList; }
    std::unordered_map<size_t, ZXVertex*> const& getOutputList() const { return _outputList; }

    // I/O (in zxIO.cpp)
    bool readZX(const std::string& filename, bool keepID = false);
    bool writeZX(std::string const& filename, bool complete = false) const;
    bool writeTikz(std::string const& filename) const;
    bool writeTikz(std::ostream& tikzFile) const;
    bool writePdf(std::string const& filename) const;
    bool writeTex(std::string const& filename) const;
    bool writeTex(std::ostream& texFile) const;

    // Traverse (in zxTraverse.cpp)
    void updateTopoOrder() const;
    void updateBreadthLevel() const;
    template <typename F>
    void topoTraverse(F lambda) {
        updateTopoOrder();
        for_each(_topoOrder.begin(), _topoOrder.end(), lambda);
    }
    template <typename F>
    void topoTraverse(F lambda) const {
        updateTopoOrder();
        for_each(_topoOrder.begin(), _topoOrder.end(), lambda);
    }
    template <typename F>
    void forEachEdge(F lambda) const {
        for (auto& v : _vertices) {
            for (auto& [nb, etype] : v->getNeighbors()) {
                if (nb->getId() > v->getId())
                    lambda(makeEdgePair(v, nb, etype));
            }
        }
    }

    // divide into subgraphs and merge (in zxPartition.cpp)
    std::pair<std::vector<ZXGraph*>, std::vector<ZXCut>> createSubgraphs(std::vector<ZXVertexList> vertexLists);
    static ZXGraph* fromSubgraphs(const std::vector<ZXGraph*>& subgraphs, const std::vector<ZXCut>& cuts);

private:
    size_t _id;
    size_t _nextVId;
    std::string _fileName;
    std::vector<std::string> _procedures;
    ZXVertexList _inputs;
    ZXVertexList _outputs;
    ZXVertexList _vertices;
    std::unordered_map<size_t, ZXVertex*> _inputList;
    std::unordered_map<size_t, ZXVertex*> _outputList;
    std::vector<ZXVertex*> mutable _topoOrder;
    unsigned mutable _globalTraCounter;

    void DFS(ZXVertex*) const;
    void BFS(ZXVertex*) const;

    bool buildGraphFromParserStorage(const ZXParserDetail::StorageType& storage, bool keepID = false);

    void moveVerticesFrom(ZXGraph& g);
};

#endif  // ZX_GRAPH_H
