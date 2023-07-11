/****************************************************************************
  FileName     [ zxMapping.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t

#include "qtensor.h"      // for QTensor
#include "zx2tsMapper.h"  // for ZX2TSMapper
#include "zxGraph.h"      // for ZXGraph, ZXVertex

using namespace std;
extern size_t verbose;

/**
 * @brief Get non-boundary vertices
 *
 * @return ZXVertexList
 */
ZXVertexList ZXGraph::getNonBoundary() {
    ZXVertexList tmp;
    tmp.clear();
    for (const auto& v : _vertices) {
        if (!v->isBoundary())
            tmp.emplace(v);
    }
    return tmp;
}

/**
 * @brief Get input vertex of qubit q
 *
 * @param q qubit
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::getInputByQubit(const size_t& q) {
    if (!_inputList.contains(q)) {
        cerr << "Input qubit id " << q << "not found" << endl;
        return nullptr;
    } else
        return _inputList[q];
}

/**
 * @brief Get output vertex of qubit q
 *
 * @param q qubit
 * @return ZXVertex*
 */
ZXVertex* ZXGraph::getOutputByQubit(const size_t& q) {
    if (!_outputList.contains(q)) {
        cerr << "Output qubit id " << q << "not found" << endl;
        return nullptr;
    } else
        return _outputList[q];
}

/**
 * @brief Concatenate a ZX-graph of a gate to the ZX-graph of big circuit
 *
 * @param tmp the graph of a gate
 */
void ZXGraph::concatenate(ZXGraph* tmp) {
    // Add Vertices
    this->addVertices(tmp->getNonBoundary(), true);
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInp = tmp->getInputList();
    for (auto& [inpQubit, v] : tmpInp) {
        auto [targetInput, gateEtype] = v->getFirstNeighbor();
        auto [lastVertex, circuitEtype] = this->getOutputByQubit(inpQubit)->getFirstNeighbor();
        tmp->removeEdge(make_pair(make_pair(v, targetInput), gateEtype));  // Remove old edge (disconnect old graph)

        lastVertex->disconnect(this->getOutputByQubit(inpQubit));

        this->addEdge(lastVertex, targetInput, (circuitEtype == EdgeType::HADAMARD) ? toggleEdge(gateEtype) : gateEtype);  // Add new edge
        delete v;
    }
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOup = tmp->getOutputList();
    for (auto& [oupQubit, v] : tmpOup) {
        auto [targetOutput, etype] = v->getFirstNeighbor();
        ZXVertex* ZXOup = this->getOutputByQubit(oupQubit);
        tmp->removeEdge(make_pair(make_pair(v, targetOutput), etype));  // Remove old edge (disconnect old graph)
        this->addEdge(targetOutput, ZXOup, etype);                      // Add new edge
        delete v;
    }
    tmp->disownVertices();
}

/**
 * @brief Get Tensor form of Z, X spider, or H box
 *
 * @return QTensor<double>
 */
QTensor<double> ZXVertex::getTSform() {
    QTensor<double> tensor = (1. + 0.i);
    if (isBoundary()) {
        tensor = QTensor<double>::identity(_neighbors.size());
        return tensor;
    }

    if (isHBox())
        tensor = QTensor<double>::hbox(_neighbors.size());
    else if (isZ())
        tensor = QTensor<double>::zspider(_neighbors.size(), _phase);
    else if (isX())
        tensor = QTensor<double>::xspider(_neighbors.size(), _phase);
    else
        cerr << "Error: Invalid vertex type!! (" << _id << ")" << endl;
    return tensor;
}

/**
 * @brief Generate tensor form of ZX-graph
 *
 */
void ZXGraph::toTensor() {
    for (auto& v : _vertices) {
        v->setPin(unsigned(-1));
    }
    ZX2TSMapper mapper(this);
    mapper.map();
}