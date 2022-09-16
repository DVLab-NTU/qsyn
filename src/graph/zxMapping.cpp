/****************************************************************************
  FileName     [ zxMapping.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Mapping function for ZX ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include "zxGraph.h"
#include "util.h"

using namespace std;
extern size_t verbose;

// Mapping functions
// void ZXGraph::clearHashes() {
//     for ( auto it = _inputList.begin(); it != _inputList.end(); ++it ) delete it->second;
//     for ( auto it = _outputList.begin(); it != _outputList.end(); ++it ) delete it->second;
//     _inputList.clear(); _outputList.clear();
// }

vector<ZXVertex*> ZXGraph::getNonBoundary() { 
    vector<ZXVertex*> tmp; tmp.clear(); 
    for(size_t i=0; i<_vertices.size(); i++){
        if(_vertices[i]->getType()==VertexType::BOUNDARY) continue;
        else tmp.push_back(_vertices[i]);
    }
    return tmp;
}

ZXVertex* ZXGraph::getInputFromHash(size_t q) { 
    if (_inputList.find(q) == _inputList.end()) {
        cerr << "Input qubit id " << q << "not found" << endl;
        return nullptr;
    } 
    else 
        return _inputList[q];
}

ZXVertex* ZXGraph::getOutputFromHash(size_t q) { 
    if (_outputList.find(q) == _outputList.end()) {
        cerr << "Output qubit id " << q << "not found" << endl;
        return nullptr;
    } 
    else 
        return _outputList[q];
}

void ZXGraph::concatenate(ZXGraph* tmp, size_t verbose, bool remove_imm){
    // Add Vertices
    this -> addVertices( tmp -> getNonBoundary() );
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInp = tmp -> getInputList();
    for ( auto it = tmpInp.begin(); it != tmpInp.end(); ++it ){
        size_t inpQubit = it->first;
        ZXVertex* targetInput = it ->second -> getNeighbors()[0].first;
        ZXVertex* lastVertex = this -> getOutputFromHash(inpQubit) -> getNeighbors()[0].first;
        tmp -> removeEdge(it->second, targetInput, verbose); // Remove old edge (disconnect old graph)
        if(remove_imm)
            this -> removeEdge(lastVertex, this -> getOutputFromHash(inpQubit), verbose); // Remove old edge (output and prev-output)
        else
            lastVertex->disconnect(this -> getOutputFromHash(inpQubit), verbose);
        this -> addEdge(lastVertex, targetInput, EdgeType::SIMPLE, verbose); // Add new edge
        delete it->second;
    }
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOup = tmp -> getOutputList();
    for ( auto it = tmpOup.begin(); it != tmpOup.end(); ++it ){
        size_t oupQubit = it->first;
        ZXVertex* targetOutput = it -> second -> getNeighbors()[0].first;
        ZXVertex* ZXOup = this -> getOutputFromHash(oupQubit);
        tmp -> removeEdge(it->second, targetOutput, verbose); // Remove old edge (disconnect old graph)    
        this -> addEdge(targetOutput, ZXOup, EdgeType::SIMPLE, verbose); // Add new edge
        delete it->second;
    }
    tmp -> reset();
}

void ZXGraph::cleanRedundantEdges(){
    vector<EdgePair> tmp;
    tmp.clear();
    for(size_t i=0; i<_edges.size(); i++){
        ZXVertex* v = _edges[i].first.second;
        if(v->getType()==VertexType::BOUNDARY){ // is output
            if(! v->isNeighbor(_edges[i].first.first)){
                //remove
                // k++;
            }
            else tmp.push_back(_edges[i]);
        }
        else tmp.push_back(_edges[i]);
    }
    _edges.clear();
    _edges = tmp;
}

