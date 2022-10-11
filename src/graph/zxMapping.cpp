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
// void ZXGraph::clearHashes() {
//     for ( auto it = _inputList.begin(); it != _inputList.end(); ++it ) delete it->second;
//     for ( auto it = _outputList.begin(); it != _outputList.end(); ++it ) delete it->second;
//     _inputList.clear(); _outputList.clear();
// }

QTensor<double> ZXVertex::getTSform(){
    QTensor<double> tensor = (1. + 0.i);
    if (_type == VertexType::BOUNDARY){
        tensor = QTensor<double>::identity(_neighborMap.size());
        return tensor;
    }
        
    // Check self loop
    size_t hedge = 0, sedge = 0;
    auto neighborSelf = _neighborMap.equal_range(this);
    for (auto jtr = neighborSelf.first; jtr != neighborSelf.second; jtr++) {
        if(*(jtr->second) == EdgeType::HADAMARD) hedge++;
        else if(*(jtr->second) == EdgeType::SIMPLE) sedge++;
        else cerr << "Wrong Type!!" << endl;
    }
    if(verbose>=9) cout << hedge << " " << sedge << endl;
    Phase tsPhase = _phase;
    assert(hedge%2==0); assert(sedge%2==0);
    for(size_t i=0; i<hedge/2; i++) tsPhase += Phase(1);
    
    
    if (_type == VertexType::H_BOX)
        tensor = QTensor<double>::hbox(_neighborMap.size()-sedge-hedge);
    else if (_type == VertexType::Z)
        tensor = QTensor<double>::zspider(_neighborMap.size()-sedge-hedge, tsPhase);
    else if (_type == VertexType::X)
        tensor = QTensor<double>::xspider(_neighborMap.size()-sedge-hedge, tsPhase);
    else
        cerr << "Vertex " << _id << " type ERROR" << endl;
    return tensor;
}

vector<ZXVertex*> ZXGraph::getNonBoundary() {
    vector<ZXVertex*> tmp;
    tmp.clear();
    for (size_t i = 0; i < _vertices.size(); i++) {
        if (_vertices[i]->getType() == VertexType::BOUNDARY)
            continue;
        else
            tmp.push_back(_vertices[i]);
    }
    return tmp;
}

vector<EdgePair> ZXGraph::getInnerEdges() {
    vector<EdgePair> tmp;
    tmp.clear();
    for (size_t i = 0; i < _edges.size(); i++) {
        if (_edges[i].first.first->getType() == VertexType::BOUNDARY || _edges[i].first.second->getType() == VertexType::BOUNDARY)
            continue;
        else
            tmp.push_back(_edges[i]);
    }
    return tmp;
}

ZXVertex* ZXGraph::getInputFromHash(size_t q) {
    if (_inputList.find(q) == _inputList.end()) {
        cerr << "Input qubit id " << q << "not found" << endl;
        return nullptr;
    } else
        return _inputList[q];
}

ZXVertex* ZXGraph::getOutputFromHash(size_t q) {
    if (_outputList.find(q) == _outputList.end()) {
        cerr << "Output qubit id " << q << "not found" << endl;
        return nullptr;
    } else
        return _outputList[q];
}

void ZXGraph::concatenate(ZXGraph* tmp, bool remove_imm) {
    // Add Vertices
    this->addVertices(tmp->getNonBoundary());
    this->addEdges(tmp->getInnerEdges());
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInp = tmp->getInputList();
    for (auto it = tmpInp.begin(); it != tmpInp.end(); ++it) {
        size_t inpQubit = it->first;
        // ZXVertex* targetInput = it ->second->getNeighbors()[0].first;
        ZXVertex* targetInput = it->second->getNeighborMap().begin()->first;
        // ZXVertex* lastVertex = this->getOutputFromHash(inpQubit)->getNeighbors()[0].first;
        ZXVertex* lastVertex = this->getOutputFromHash(inpQubit)->getNeighborMap().begin()->first;
        tmp->removeEdge(it->second, targetInput);  // Remove old edge (disconnect old graph)
        if (remove_imm)
            this->removeEdge(lastVertex, this->getOutputFromHash(inpQubit));  // Remove old edge (output and prev-output)
        else
            lastVertex->disconnect(this->getOutputFromHash(inpQubit));
        this->addEdge(lastVertex, targetInput, new EdgeType(EdgeType::SIMPLE));  // Add new edge
        delete it->second;
    }
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOup = tmp->getOutputList();
    for (auto it = tmpOup.begin(); it != tmpOup.end(); ++it) {
        size_t oupQubit = it->first;
        // ZXVertex* targetOutput = it->second->getNeighbors()[0].first;
        ZXVertex* targetOutput = it->second->getNeighborMap().begin()->first;
        ZXVertex* ZXOup = this->getOutputFromHash(oupQubit);
        tmp->removeEdge(it->second, targetOutput);                           // Remove old edge (disconnect old graph)
        this->addEdge(targetOutput, ZXOup, new EdgeType(EdgeType::SIMPLE));  // Add new edge
        delete it->second;
    }
    tmp->reset();
}

void ZXGraph::cleanRedundantEdges() {
    vector<EdgePair> tmp;
    tmp.clear();
    for (size_t i = 0; i < _edges.size(); i++) {
        ZXVertex* v = _edges[i].first.second;
        if (v->getType() == VertexType::BOUNDARY) {  // is output
            if (!v->isNeighbor(_edges[i].first.first)) {
                // remove
                //  k++;
            } else
                tmp.push_back(_edges[i]);
        } else
            tmp.push_back(_edges[i]);
    }
    _edges.clear();
    _edges = tmp;
}

void ZXGraph::tensorMapping() {
    ZX2TSMapper mapper(this);
    mapper.map();
}