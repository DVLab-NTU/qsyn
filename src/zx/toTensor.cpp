/****************************************************************************
  FileName     [ zx2tsMapper.cpp ]
  PackageName  [ zx ]
  Synopsis     [ Define class ZX-to-Tensor Mapper member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "./toTensor.hpp"

#include <cassert>

#include "./zxGraph.hpp"
#include "tensor/tensorMgr.hpp"
#include "util/logger.hpp"

extern bool stop_requested();

extern size_t verbose;
extern dvlab::utils::Logger logger;
extern TensorMgr tensorMgr;

using namespace std;

class ZX2TSMapper {
public:
    using Frontiers = ordered_hashmap<EdgePair, size_t, EdgePairHash>;

    ZX2TSMapper() {}

    class ZX2TSList {
    public:
        const Frontiers& frontiers(const size_t& id) const {
            return _zx2tsList[id].first;
        }
        const QTensor<double>& tensor(const size_t& id) const {
            return _zx2tsList[id].second;
        }
        Frontiers& frontiers(const size_t& id) {
            return _zx2tsList[id].first;
        }
        QTensor<double>& tensor(const size_t& id) {
            return _zx2tsList[id].second;
        }
        void append(const Frontiers& f, const QTensor<double>& q) {
            _zx2tsList.emplace_back(f, q);
        }
        size_t size() {
            return _zx2tsList.size();
        }

    private:
        std::vector<std::pair<Frontiers, QTensor<double>>> _zx2tsList;
    };

    std::optional<QTensor<double>> map(ZXGraph const& zxgraph);

private:
    std::vector<EdgePair> _boundaryEdges;  // EdgePairs of the boundaries
    ZX2TSList _zx2tsList;                  // The tensor list for each set of frontiers
    size_t _tensorId;                      // Current tensor id for the _tensorId

    TensorAxisList _simplePins;          // Axes that can be tensordotted directly
    TensorAxisList _hadamardPins;        // Axes that should be applied hadamards first
    std::vector<EdgePair> _removeEdges;  // Old frontiers to be removed
    std::vector<EdgePair> _addEdges;     // New frontiers to be added

    Frontiers& currFrontiers() { return _zx2tsList.frontiers(_tensorId); }
    QTensor<double>& currTensor() { return _zx2tsList.tensor(_tensorId); }
    const Frontiers& currFrontiers() const { return _zx2tsList.frontiers(_tensorId); }
    const QTensor<double>& currTensor() const { return _zx2tsList.tensor(_tensorId); }

    void mapOneVertex(ZXVertex* v);

    // mapOneVertex Subroutines
    void initSubgraph(ZXVertex* v);
    void tensorDotVertex(ZXVertex* v);
    void updatePinsAndFrontiers(ZXVertex* v);
    QTensor<double> dehadamardize(const QTensor<double>& ts);

    bool isOfNewGraph(const ZXVertex* v);
    bool isFrontier(const NeighborPair& nbr) const;

    struct InOutAxisList {
        TensorAxisList inputs;
        TensorAxisList outputs;
    };
    InOutAxisList getAxisOrders(ZXGraph const& zxgraph);
};

std::optional<QTensor<double>> toTensor(ZXGraph const& zxgraph) {
    ZX2TSMapper mapper;
    return mapper.map(zxgraph);
}

/**
 * @brief convert a zxgraph to a tensor
 *
 * @return std::optional<QTensor<double>> containing a QTensor<double> if the conversion succeeds
 */
std::optional<QTensor<double>> ZX2TSMapper::map(ZXGraph const& zxgraph) {
    if (!zxgraph.isValid()) {
        logger.error("The ZXGraph is not valid!!");
        return std::nullopt;
    }

    for (auto& v : zxgraph.getVertices()) {
        v->setPin(unsigned(-1));
    }

    zxgraph.topoTraverse([this](ZXVertex* v) { mapOneVertex(v); });

    if (stop_requested()) {
        logger.error("Conversion is interrupted!!");
        return std::nullopt;
    }
    QTensor<double> result;

    for (size_t i = 0; i < _zx2tsList.size(); ++i) {
        result = tensordot(result, _zx2tsList.tensor(i));
    }

    for (size_t i = 0; i < _boundaryEdges.size(); ++i) {
        // Don't care whether key collision happen: getAxisOrders takes care of such cases
        _zx2tsList.frontiers(i).emplace(_boundaryEdges[i], 0);
    }

    auto [inputIds, outputIds] = getAxisOrders(zxgraph);

    logger.trace("Input  Axis IDs: {}", fmt::join(inputIds, " "));
    logger.trace("Output Axis IDs: {}", fmt::join(outputIds, " "));

    result = result.toMatrix(inputIds, outputIds);

    return result;
}

/**
 * @brief Consturct tensor of a single vertex
 *
 * @param v the tensor of whom
 */
void ZX2TSMapper::mapOneVertex(ZXVertex* v) {
    if (stop_requested()) return;

    _simplePins.clear();
    _hadamardPins.clear();
    _removeEdges.clear();
    _addEdges.clear();
    _tensorId = 0;

    bool isNewGraph = isOfNewGraph(v);
    bool isBoundary = v->isBoundary();

    logger.debug("Mapping vertex {:>4} ({}): {}", v->getId(), v->getType(), isNewGraph ? "New Subgraph" : isBoundary ? "Boundary"
                                                                                                                     : "Tensordot");

    if (isNewGraph) {
        initSubgraph(v);
    } else if (isBoundary) {
        updatePinsAndFrontiers(v);
        currTensor() = dehadamardize(currTensor());
    } else {
        updatePinsAndFrontiers(v);
        tensorDotVertex(v);
    }
    v->setPin(_tensorId);

    logger.debug("Done. Current tensor dimension: {}", currTensor().dimension());
    logger.trace("Current frontiers:");
    for (auto& [epair, axid] : _zx2tsList.frontiers(_tensorId)) {
        auto& [vpair, etype] = epair;
        logger.trace("  {}--{} ({}) axis id: {}", vpair.first->getId(), vpair.second->getId(), etype, axid);
    }
}

/**
 * @brief Generate a new subgraph for mapping
 *
 * @param v the boundary vertex to start the mapping
 */
void ZX2TSMapper::initSubgraph(ZXVertex* v) {
    auto [nb, etype] = *(v->getNeighbors().begin());

    _zx2tsList.append(Frontiers(), QTensor<double>(1. + 0.i));
    _tensorId = _zx2tsList.size() - 1;
    assert(v->isBoundary());

    EdgePair edgeKey = makeEdgePair(v, nb, etype);
    currTensor() = tensordot(currTensor(), QTensor<double>::identity(v->getNumNeighbors()));
    _boundaryEdges.emplace_back(edgeKey);
    currFrontiers().emplace(edgeKey, 1);
}

/**
 * @brief Check if a vertex belongs to a new subgraph that is not traversed
 *
 * @param v vertex
 * @return true or
 * @return false and set the _tensorId to the current tensor
 */
bool ZX2TSMapper::isOfNewGraph(const ZXVertex* v) {
    for (auto nbr : v->getNeighbors()) {
        if (isFrontier(nbr)) {
            _tensorId = nbr.first->getPin();
            return false;
        }
    }
    return true;
}

/**
 * @brief get the tensor axis-zxgraph qubit correspondence
 *
 * @param zxgraph
 * @return std::pair<TensorAxisList, TensorAxisList> input and output tensor axis lists
 */
ZX2TSMapper::InOutAxisList ZX2TSMapper::getAxisOrders(ZXGraph const& zxgraph) {
    InOutAxisList axisLists;
    axisLists.inputs.resize(zxgraph.getNumInputs());
    axisLists.outputs.resize(zxgraph.getNumOutputs());
    std::map<int, size_t> inputTable, outputTable;  // std:: to avoid name collision with ZX2TSMapper::map
    for (auto v : zxgraph.getInputs()) {
        inputTable[v->getQubit()] = 0;
    }
    size_t count = 0;
    for (auto [qid, _] : inputTable) {
        inputTable[qid] = count;
        ++count;
    }

    for (auto v : zxgraph.getOutputs()) {
        outputTable[v->getQubit()] = 0;
    }
    count = 0;
    for (auto [qid, _] : outputTable) {
        outputTable[qid] = count;
        ++count;
    }
    size_t accFrontierSize = 0;
    for (size_t i = 0; i < _zx2tsList.size(); ++i) {
        // cout << "> Tensor " << i << endl;
        // printFrontiers(i);
        bool hasBoundary2BoundaryEdge = false;
        for (auto& [epair, axid] : _zx2tsList.frontiers(i)) {
            const auto& [v1, v2] = epair.first;
            bool v1IsInput = zxgraph.getInputs().contains(v1);
            bool v2IsInput = zxgraph.getInputs().contains(v2);
            bool v1IsOutput = zxgraph.getOutputs().contains(v1);
            bool v2IsOutput = zxgraph.getOutputs().contains(v2);

            if (v1IsInput) axisLists.inputs[inputTable[v1->getQubit()]] = axid + accFrontierSize;
            if (v2IsInput) axisLists.inputs[inputTable[v2->getQubit()]] = axid + accFrontierSize;
            if (v1IsOutput) axisLists.outputs[outputTable[v1->getQubit()]] = axid + accFrontierSize;
            if (v2IsOutput) axisLists.outputs[outputTable[v2->getQubit()]] = axid + accFrontierSize;
            assert(!(v1IsInput && v1IsOutput));
            assert(!(v2IsInput && v2IsOutput));

            // If seeing boundary-to-boundary edge, decrease one of the axis id by one to avoid id collision
            if (v1IsInput && (v2IsInput || v2IsOutput)) {
                assert(_zx2tsList.frontiers(i).size() == 1);
                axisLists.inputs[inputTable[v1->getQubit()]]--;
                hasBoundary2BoundaryEdge = true;
            }
            if (v1IsOutput && (v2IsInput || v2IsOutput)) {
                assert(_zx2tsList.frontiers(i).size() == 1);
                axisLists.outputs[outputTable[v1->getQubit()]]--;
                hasBoundary2BoundaryEdge = true;
            }
        }
        accFrontierSize += _zx2tsList.frontiers(i).size() + (hasBoundary2BoundaryEdge ? 1 : 0);
    }

    return axisLists;
}

/**
 * @brief Update information for the current and next frontiers
 *
 * @param v the current vertex
 */
void ZX2TSMapper::updatePinsAndFrontiers(ZXVertex* v) {
    Neighbors nbrs = v->getNeighbors();

    // unordered_set<NeighborPair> seenFrontiers; // only for look-up
    for (auto& nbr : nbrs) {
        auto& [nb, etype] = nbr;

        EdgePair edgeKey = makeEdgePair(v, nb, etype);
        if (!isFrontier(nbr)) {
            _addEdges.emplace_back(edgeKey);
        } else {
            auto& [front, axid] = *(currFrontiers().find(edgeKey));
            if ((front.second) == EdgeType::HADAMARD) {
                _hadamardPins.emplace_back(axid);
            } else {
                _simplePins.emplace_back(axid);
            }
            _removeEdges.emplace_back(edgeKey);
        }
    }
}

/**
 * @brief Convert hadamard edges to normal edges and returns a corresponding tensor
 *
 * @param ts original tensor before converting
 * @return QTensor<double>
 */
QTensor<double> ZX2TSMapper::dehadamardize(const QTensor<double>& ts) {
    QTensor<double> HTensorProduct = tensorPow(
        QTensor<double>::hbox(2), _hadamardPins.size());

    TensorAxisList connect_pin;
    for (size_t t = 0; t < _hadamardPins.size(); t++)
        connect_pin.emplace_back(2 * t);

    QTensor<double> tmp = tensordot(ts, HTensorProduct, _hadamardPins, connect_pin);

    // post-tensordot axis update
    for (auto& [_, axisId] : currFrontiers()) {
        if (!contains(_hadamardPins, axisId)) {
            axisId = tmp.getNewAxisId(axisId);
        } else {
            size_t id = findIndex(_hadamardPins, axisId);
            axisId = tmp.getNewAxisId(ts.dimension() + connect_pin[id] + 1);
        }
    }

    // update _simplePins and _hadamardPins
    for (size_t t = 0; t < _hadamardPins.size(); t++) {
        _hadamardPins[t] = tmp.getNewAxisId(ts.dimension() + connect_pin[t] + 1);  // dimension of big tensor + 1,3,5,7,9
    }
    for (size_t t = 0; t < _simplePins.size(); t++)
        _simplePins[t] = tmp.getNewAxisId(_simplePins[t]);

    _simplePins = concatAxisList(_hadamardPins, _simplePins);
    return tmp;
}

/**
 * @brief Tensordot the current tensor to the tensor of vertex v
 *
 * @param v current vertex
 */
void ZX2TSMapper::tensorDotVertex(ZXVertex* v) {
    QTensor<double> dehadamarded = dehadamardize(currTensor());

    TensorAxisList connect_pin;
    for (size_t t = 0; t < _simplePins.size(); t++)
        connect_pin.emplace_back(t);

    currTensor() = tensordot(dehadamarded, getTSform(v), _simplePins, connect_pin);

    // remove dotted frontiers
    for (size_t i = 0; i < _removeEdges.size(); i++)
        currFrontiers().erase(_removeEdges[i]);  // Erase old edges

    // post-tensordot axis id update
    for (auto& frontier : currFrontiers()) {
        frontier.second = currTensor().getNewAxisId(frontier.second);
    }

    // add new frontiers
    connect_pin.clear();
    for (size_t t = 0; t < _addEdges.size(); t++)
        connect_pin.emplace_back(_simplePins.size() + t);

    for (size_t t = 0; t < _addEdges.size(); t++) {
        size_t newId = currTensor().getNewAxisId(dehadamarded.dimension() + connect_pin[t]);
        currFrontiers().emplace(_addEdges[t], newId);  // origin pin (neighbot count) + 1,3,5,7,9
    }
}

/**
 * @brief Check the neighbor pair (edge) is in frontier
 *
 * @param nbr
 * @return true
 * @return false
 */
bool ZX2TSMapper::isFrontier(const NeighborPair& nbr) const {
    return (nbr.first->getPin() != unsigned(-1));
}

/**
 * @brief Get Tensor form of Z, X spider, or H box
 *
 * @param v the ZXVertex
 * @return QTensor<double>
 */
QTensor<double> getTSform(ZXVertex* v) {
    if (v->isBoundary()) return QTensor<double>::identity(v->getNumNeighbors());
    if (v->isHBox()) return QTensor<double>::hbox(v->getNumNeighbors());
    if (v->isZ()) return QTensor<double>::zspider(v->getNumNeighbors(), v->getPhase());
    if (v->isX()) return QTensor<double>::xspider(v->getNumNeighbors(), v->getPhase());

    cerr << "Error: Invalid vertex type!! (" << v->getId() << ")" << endl;
    return {1. + 0.i};
}
