/****************************************************************************
  FileName     [ zx2tsMapper.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Mapper class for ZX-to-Tensor mapping ]
  Author       [ Chin-Yi Cheng, Mu-Te Joshua Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "zx2tsMapper.h"

#include "tensorMgr.h"
#include "textFormat.h"
#include "ordered_hashset.h"
extern size_t verbose;
extern TensorMgr* tensorMgr;

#include <map>
using namespace std;
namespace TF = TextFormat;

// map a ZX-diagram to a tensor
bool ZX2TSMapper::mapping() {
    if (!_zxgraph->isValid()) {
        cerr << "Error: The ZX-Graph is not valid!!" << endl;
        return false;
    }
    if (verbose >= 2) cout << "---- TRAVERSE AND BUILD THE TENSOR ----" << endl;
    _zxgraph->topoTraverse([this](ZXVertex* v) { mapOneVertex(v); });
    for (size_t i = 0; i < _boundaryEdges.size(); i++)
        _zx2tsList.frontiers(i).emplace(_boundaryEdges[i], 0);

    if (!tensorMgr) tensorMgr = new TensorMgr();
    size_t id = tensorMgr->nextID();
    QTensor<double>* result = tensorMgr->addTensor(id, "ZX " + to_string(_zxgraph->getId()));

    for (size_t i = 0; i < _zx2tsList.size(); ++i) {
        *result = tensordot(*result, _zx2tsList.tensor(i));
    }

    TensorAxisList inputIds, outputIds;
    getAxisOrders(inputIds, _zxgraph->getInputList(), false);
    getAxisOrders(outputIds, _zxgraph->getOutputList(), true);

    if (verbose >= 8) {
        cout << "Input  axis ids: "; printAxisList(inputIds);
        cout << "Output axis ids: "; printAxisList(outputIds);
    }

    *result = result->toMatrix(inputIds, outputIds);
    cout << "Stored the resulting tensor as tensor id " << id << endl;
    
    return true;
}

// map one vertex
void ZX2TSMapper::mapOneVertex(ZXVertex* v) {
    _simplePins.clear();
    _hadamardPins.clear();
    _removeEdges.clear();
    _addEdges.clear();
    _tensorId = 0;

    if (verbose >= 5) cout << "> Mapping vertex " << v->getId() << " (" << VertexType2Str(v->getType()) << "): ";
    if (isOfNewGraph(v)) {
        if (verbose >= 5) cout << "New Subgraph" << endl;
        initSubgraph(v);
    } else if (v->getType() == VertexType::BOUNDARY) {
        if (verbose >= 5) cout << "Boundary Node" << endl;
        updatePinsAndFrontiers(v);
        currTensor() = dehadamardize(currTensor());
    } else {
        if (verbose >= 5) cout << "Tensordot" << endl;
        updatePinsAndFrontiers(v);
        tensorDotVertex(v);
    }
    v->setPin(_tensorId);
    if (verbose >= 8) {
        printFrontiers();
    }
}

// Generate a new subgraph for mapping
void ZX2TSMapper::initSubgraph(ZXVertex* v) {
    Neighbors nbrs = v->getNeighbors();

    _zx2tsList.append(Frontiers(), QTensor<double>(1. + 0.i));
    _tensorId = _zx2tsList.size() - 1;
    assert(v->isBoundary());
    EdgePair edgeKey = makeEdgePair(v, nbrs.begin()->first, nbrs.begin()->second);
    currTensor() = tensordot(currTensor(), QTensor<double>::identity(nbrs.size()));
    _boundaryEdges.push_back(edgeKey);
    currFrontiers().emplace(edgeKey, 1);
}

// Check if a vertex belongs to a new subgraph that is not traversed
bool ZX2TSMapper::isOfNewGraph(const ZXVertex* v) {
    for (auto nbr : v->getNeighbors()) {
        if (isFrontier(nbr)) {
            _tensorId = nbr.first->getPin();
            return false;
        }
    }
    return true;
}

// Print the current and next frontiers
void ZX2TSMapper::printFrontiers() const {
    cout << "  - Current frontiers: " << endl;
    for (auto [epair, axid] : currFrontiers()) {
        cout << "    "
             << epair.first.first->getId() << "--"
             << epair.first.second->getId() << " ("
             << EdgeType2Str(epair.second)
             << ") axis id: " << axid << endl;
    } 
}

// Get the order of inputs and outputs
void ZX2TSMapper::getAxisOrders(TensorAxisList& axList, const std::unordered_map<size_t, ZXVertex*>& ioList, bool isOutput) {
    axList.resize(ioList.size());
    std::map<size_t, size_t> table;

    for (const auto& [qid, _] : ioList) {
        table[qid] = 0;
    }
    size_t count = 0;
    for (const auto& [qid, _] : table) {
        table[qid] = count;
        count++;
    }
    size_t accFrontierSizes = 0;
    for (size_t i = 0; i < _zx2tsList.size(); ++i) {
        for (auto& [qid, v] : ioList) {
            Neighbors nbrs = v->getNeighbors();
            auto& [neighbor, etype] = *(nbrs.begin());
            EdgePair edgeKey = makeEdgePair(v, neighbor, etype);

            auto result = _zx2tsList.frontiers(i).find(edgeKey);
            if (result != _zx2tsList.frontiers(i).end()) {
                axList[table[qid]] = result->second + accFrontierSizes;
                if (isOutput && itr != result.second) {
                    axList[table[qid]] += 1;
                }
            }
        }
        accFrontierSizes += _zx2tsList.frontiers(i).size();
    }
    // for (size_t i = 0; i < _zx2tsList.size(); ++i) {
    // }
}

// update information for the current and next frontiers
void ZX2TSMapper::updatePinsAndFrontiers(ZXVertex* v) {
    Neighbors nbrs = v->getNeighbors();
    
    unordered_set<NeighborPair> seenFrontiers; // only for look-up
    for (auto& nbr : nbrs) {
        auto& [w, etype] = nbr;

        EdgePair edgeKey = makeEdgePair(v, w, etype);
        if (!isFrontier(nbr)) {
            _addEdges.push_back(edgeKey);
        } else if (!seenFrontiers.contains(nbr)) {
            seenFrontiers.insert(nbr);
            auto& [front, axid] = *(currFrontiers().find(edgeKey));
            if ((front.second) == EdgeType::HADAMARD) {
                _hadamardPins.push_back(axid);
            } else {
                _simplePins.push_back(axid);
            }
            _removeEdges.push_back(edgeKey);
        }
    }
}

// Convert hadamard edges to normal edges and returns a corresponding tensor
QTensor<double> ZX2TSMapper::dehadamardize(const QTensor<double>& ts) {

    QTensor<double> HTensorProduct = tensorPow(
        QTensor<double>::hbox(2), _hadamardPins.size()
    );

    TensorAxisList connect_pin;
    for (size_t t = 0; t < _hadamardPins.size(); t++)
        connect_pin.push_back(2 * t);
    
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

// tensordot the current tensor to the vertex's tensor form
void ZX2TSMapper::tensorDotVertex(ZXVertex* v) {
    QTensor<double> dehadamarded = dehadamardize(currTensor());

    TensorAxisList connect_pin;
    for (size_t t = 0; t < _simplePins.size(); t++)
        connect_pin.push_back(t);

    currTensor() = tensordot(dehadamarded, v->getTSform(), _simplePins, connect_pin);

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
        connect_pin.push_back(_simplePins.size() + t);

    for (size_t t = 0; t < _addEdges.size(); t++) {
        size_t newId = currTensor().getNewAxisId(dehadamarded.dimension() + connect_pin[t]);
        currFrontiers().emplace(_addEdges[t], newId);  // origin pin (neighbot count) + 1,3,5,7,9
    }
}
