/****************************************************************************
  FileName     [ zxMapping.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Mapping function for ZX ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <vector>

#include "util.h"
#include "zx2tsMapper.h"
#include "zxGraph.h"

using namespace std;
extern size_t verbose;

// Mapping functions



ZXVertexList ZXGraph::getNonBoundary() {
    ZXVertexList tmp;
    tmp.clear();
    for(const auto& v: _vertices){
        if (!v->isBoundary())
            tmp.emplace(v);
    }
    return tmp;
}

ZXVertex* ZXGraph::getInputFromHash(const size_t& q) {
    if (!_inputList.contains(q)) {
        cerr << "Input qubit id " << q << "not found" << endl;
        return nullptr;
    } else
        return _inputList[q];
}

ZXVertex* ZXGraph::getOutputFromHash(const size_t& q) {
    if (!_outputList.contains(q)) {
        cerr << "Output qubit id " << q << "not found" << endl;
        return nullptr;
    } else
        return _outputList[q];
}

void ZXGraph::concatenate(ZXGraph* tmp, bool remove_imm) {
    // Add Vertices
    this->addVertices(tmp->getNonBoundary(), true);
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInp = tmp->getInputList();
    for (auto it = tmpInp.begin(); it != tmpInp.end(); ++it) {
        size_t inpQubit = it->first;
        ZXVertex* targetInput = it->second->getNeighbors().begin()->first;
        ZXVertex* lastVertex = this->getOutputFromHash(inpQubit)->getNeighbors().begin()->first;
        tmp->removeEdge(make_pair(make_pair(it->second, targetInput), EdgeType(EdgeType::SIMPLE)));  // Remove old edge (disconnect old graph)
        
        lastVertex->disconnect(this->getOutputFromHash(inpQubit));

        this->addEdge(lastVertex, targetInput, EdgeType(EdgeType::SIMPLE));  // Add new edge
        delete it->second;
    }
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOup = tmp->getOutputList();
    for (auto it = tmpOup.begin(); it != tmpOup.end(); ++it) {
        size_t oupQubit = it->first;
        // ZXVertex* targetOutput = it->second->getNeighbors()[0].first;
        ZXVertex* targetOutput = it->second->getNeighbors().begin()->first;
        ZXVertex* ZXOup = this->getOutputFromHash(oupQubit);
        tmp->removeEdge(make_pair(make_pair(it->second, targetOutput), EdgeType(EdgeType::SIMPLE)));                           // Remove old edge (disconnect old graph)
        this->addEdge(targetOutput, ZXOup, EdgeType(EdgeType::SIMPLE));  // Add new edge
        delete it->second;
    }
    tmp->reset();
}

// Tensor mapping

QTensor<double> ZXVertex::getTSform(){
    QTensor<double> tensor = (1. + 0.i);
    if (isBoundary()){
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


void ZXGraph::toTensor() {
    for (auto& v : _vertices) {
        v->setPin(unsigned(-1));
    }
    ZX2TSMapper mapper(this);
    mapper.mapping();
}