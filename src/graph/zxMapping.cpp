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
 * @brief Strips the boundary other ZXGraph `other` and reconnect it to the output of the main graph. The main graph's output IDs are preserved
 *
 * @param other the other graph to concatenate with. This graph should have the same number of inputs and outputs
 */
void ZXGraph::concatenate(ZXGraph* other) {
    /* Visualiztion of what is done:
       ┌────┐                                ┌────┐
    i0─┤    ├─o0         ┌─────┐          i0─┤    ├─ o0 ┌─────┐
    i1─┤main├─o1  +  i1'─┤     ├─o1' -->  i1─┤main├─────┤     ├─o1
    i2─┤    ├─o2     i2'─┤other├─o2       i2─┤    ├─────┤other├─o2
       └────┘            └─────┘             └────┘     └─────┘
    */


    if (other->getNumInputs() != other->getNumOutputs()) {
        cerr << "Error: the graph being concatenated does not have the same number of inputs and outputs. Concatenation aborted!!\n";
        return;
    }
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInputs = other->getInputList();
    for (auto& [qubit, i] : tmpInputs) {
        auto [otherInputVertex, otherInputEtype] = i->getFirstNeighbor();
        auto [mainOutputVertex, mainOutputEtype] = this->getOutputByQubit(qubit)->getFirstNeighbor();

        this->removeEdge(mainOutputVertex, this->getOutputByQubit(qubit), mainOutputEtype);
        this->addEdge(mainOutputVertex, otherInputVertex, (mainOutputEtype == EdgeType::HADAMARD) ? toggleEdge(otherInputEtype) : otherInputEtype);
        other->removeVertex(i);
    }
    
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOutputs = other->getOutputList();
    for (auto& [qubit, o] : tmpOutputs) {
        auto [otherOutputVertex, etype] = o->getFirstNeighbor();
        this->addEdge(otherOutputVertex, this->getOutputByQubit(qubit), etype);
        other->removeVertex(o);
    }
    this->moveVerticesFrom(*other);
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