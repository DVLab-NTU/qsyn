/****************************************************************************
  FileName     [ zxMapping.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Mapper class for ZX-to-Tensor mapping ]
  Author       [ Chin-Yi Cheng, Mu-Te Joshua Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "zx2tsMapper.h"

#include "tensorMgr.h"
#include "textFormat.h"
extern size_t verbose;
extern TensorMgr* tensorMgr;

#include <map>
using namespace std;
namespace TF = TextFormat;

// map a ZX-diagram to a tensor
bool ZX2TSMapper::map() {
    if (!_zxgraph->isValid()) {
        cerr << "[Error] The ZX Graph is not valid!!!" << endl;
        return false;
    }
    if (verbose >= 2) cout << "---- TRAVERSE AND BUILD THE TENSOR ----" << endl;
    _zxgraph->topoTraverse([this](ZXVertex* v) { mapOneVertex(v); });
    for (size_t i = 0; i < _boundaryEdges.size(); i++)
        _zx2tsList.frontiers(i).emplace(_boundaryEdges[i], 0);

    TensorAxisList inputIds, outputIds;
    if (!tensorMgr) tensorMgr = new TensorMgr();
    size_t id = tensorMgr->nextID();
    QTensor<double>* result = tensorMgr->addTensor(id, "ZX " + to_string(_zxgraph->getId()));

    for (size_t i = 0; i < _zx2tsList.size(); ++i) {
        *result = tensordot(*result, _zx2tsList.tensor(i));
    }

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
    NeighborMap neighborMap = v->getNeighborMap();

    _zx2tsList.append(Frontiers(), QTensor<double>(1. + 0.i));
    _tensorId = _zx2tsList.size() - 1;
    assert(v->getType() == VertexType::BOUNDARY);
    EdgeKey edgeKey = makeEdgeKey(v, neighborMap.begin()->first, *(neighborMap.begin()->second));
    currTensor() = tensordot(currTensor(), QTensor<double>::identity(neighborMap.size()));
    _boundaryEdges.push_back(edgeKey);
    currFrontiers().emplace(edgeKey, 1);

    return;
}

// Check if a vertex belongs to a new subgraph that is not traversed
bool ZX2TSMapper::isOfNewGraph(const ZXVertex* v) {
    for (auto epair : v->getNeighborMap()) {
        if (isFrontier(epair)) {
            _tensorId = epair.first->getPin();
            return false;
        }
    }
    return true;
}

// Print the current and next frontiers
void ZX2TSMapper::printFrontiers() const {
    using Frontier = pair<EdgeKey, size_t>;
    vector<Frontier> tmp;
    for_each(currFrontiers().begin(), currFrontiers().end(),
             [&tmp](const Frontier& front) { tmp.emplace_back(front.first, front.second); });
    sort(tmp.begin(), tmp.end(), [](const Frontier& a, const Frontier& b) {
        size_t   id_a_s  = a.first.first.first->getId();
        size_t   id_a_t  = a.first.first.second->getId();
        size_t   id_b_s  = b.first.first.first->getId();
        size_t   id_b_t  = b.first.first.second->getId();
        EdgeType etype_a = a.first.second;
        EdgeType etype_b = b.first.second;
        size_t   axid_a  = a.second;
        size_t   axid_b  = b.second;
        if ( id_a_s <  id_b_s) return true;
        if ( id_a_s >  id_b_s) return false;
        if ( id_a_t <  id_b_t) return true;
        if ( id_a_t >  id_b_t) return false;
        if (etype_a < etype_b) return true;
        if (etype_a > etype_b) return false;
        if ( axid_a <  axid_b) return true;
        if ( axid_a >  axid_b) return false;
        return false;
    });
    cout << "  - Current frontiers: " << endl;
    for (auto i : tmp) {
        cout << "    "
             << i.first.first.first->getId() << "--"
             << i.first.first.second->getId() << " ("
             << EdgeType2Str(&(i.first.second))
             << ") axis id: " << i.second << endl;
    }
}

// Get the order of inputs and outputs
void ZX2TSMapper::getAxisOrders(TensorAxisList& axList, const std::unordered_map<size_t, ZXVertex*>& ioList, bool isOutput) {
    axList.resize(ioList.size());
    std::map<size_t, size_t> table;

    for (const auto& [qubitId, _] : ioList) {
        table[qubitId] = 0;
    }
    size_t count = 0;
    for (const auto& [qubitId, _] : table) {
        table[qubitId] = count;
        count++;
    }
    size_t accFrontierSizes = 0;
    for (size_t i = 0; i < _zx2tsList.size(); ++i) {
        for (auto& [qubitId, vertex] : ioList) {
            NeighborMap nebs = vertex->getNeighborMap();
            auto& [neighbor, etype] = *(nebs.begin());
            EdgeKey edgeKey = makeEdgeKey(vertex, neighbor, *etype);

            auto result = _zx2tsList.frontiers(i).equal_range(edgeKey);
            auto itr = result.first;
            if (itr != _zx2tsList.frontiers(i).end()) {
                axList[table[qubitId]] = _zx2tsList.frontiers(i).find(edgeKey)->second + accFrontierSizes;
                ++itr;
                if (isOutput && itr != result.second) {
                    axList[table[qubitId]] += 1;
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
    NeighborMap neighborMap = v->getNeighborMap();
    NeighborMap frontAlreadyRetrived;
    vector<pair<ZXVertex*, EdgeType>> seenFrontiers;
    for (auto& epair : neighborMap) {
        auto& [neighbor, etype] = epair;

        // skip self loops
        if (v == neighbor) {  
            if (verbose >= 8) {
                cout << "  - Skipping self loop: " << v->getId() << "--" << neighbor->getId() 
                     << " (" << EdgeType2Str(etype) << ")" << endl;
            }
            continue;
        }

        EdgeKey edgeKey = makeEdgeKey(v, neighbor, *etype);
        if (isFrontier(epair)) {
            const pair<ZXVertex*, EdgeType> tmpPair = make_pair(neighbor, *etype);
            if (!contains(seenFrontiers, tmpPair)) {
                seenFrontiers.push_back(tmpPair);
                auto result = currFrontiers().equal_range(edgeKey);
                
                for (auto jtr = result.first; jtr != result.second; jtr++) {
                    auto& [epair, id] = *jtr;
                    if ((epair.second) == EdgeType::HADAMARD)
                        _hadamardPins.push_back(id);
                    else
                        _simplePins.push_back(id);
                    _removeEdges.push_back(edgeKey);
                }
            }
        } else {
            _addEdges.push_back(edgeKey);
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
