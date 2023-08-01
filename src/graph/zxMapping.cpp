/****************************************************************************
  FileName     [ zxMapping.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>

#include "zxGraph.h"

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
void ZXGraph::concatenate(ZXGraph const& other) {
    /* Visualiztion of what is done:
       ┌────┐                                ┌────┐
    i0─┤    ├─o0         ┌─────┐          i0─┤    ├─ o0 ┌─────┐
    i1─┤main├─o1  +  i1'─┤     ├─o1' -->  i1─┤main├─────┤     ├─o1
    i2─┤    ├─o2     i2'─┤other├─o2       i2─┤    ├─────┤other├─o2
       └────┘            └─────┘             └────┘     └─────┘
    */

    if (other.getNumInputs() != other.getNumOutputs()) {
        cerr << "Error: the graph being concatenated does not have the same number of inputs and outputs. Concatenation aborted!!\n";
        return;
    }

    ZXGraph copy{other};
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInputs = copy.getInputList();
    for (auto& [qubit, i] : tmpInputs) {
        auto [otherInputVertex, otherInputEtype] = i->getFirstNeighbor();
        auto [mainOutputVertex, mainOutputEtype] = this->getOutputByQubit(qubit)->getFirstNeighbor();

        this->removeEdge(mainOutputVertex, this->getOutputByQubit(qubit), mainOutputEtype);
        this->addEdge(mainOutputVertex, otherInputVertex, concatEdge(mainOutputEtype, otherInputEtype));
        copy.removeVertex(i);
    }

    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOutputs = copy.getOutputList();
    for (auto& [qubit, o] : tmpOutputs) {
        auto [otherOutputVertex, etype] = o->getFirstNeighbor();
        this->addEdge(otherOutputVertex, this->getOutputByQubit(qubit), etype);
        copy.removeVertex(o);
    }
    this->moveVerticesFrom(copy);
}