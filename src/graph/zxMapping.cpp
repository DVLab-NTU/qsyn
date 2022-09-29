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

QTensor<double> ZXVertex::getTSform() const {
    QTensor<double> tensor = (1.+0.i);
    if(_type == VertexType::BOUNDARY)
        tensor = QTensor<double>::identity(_neighborMap.size());
    if(_type == VertexType::H_BOX)
        tensor = QTensor<double>::hbox(_neighborMap.size());
    else if(_type == VertexType::Z)
        tensor = QTensor<double>::zspider(_neighborMap.size(), _phase);
    else if(_type == VertexType::X)
        tensor = QTensor<double>::xspider(_neighborMap.size(), _phase);
    else 
        cerr << "Vertex " << _id << " type ERROR" << endl;
    return tensor;
}

vector<ZXVertex*> ZXGraph::getNonBoundary() { 
    vector<ZXVertex*> tmp; tmp.clear(); 
    for(size_t i=0; i<_vertices.size(); i++){
        if(_vertices[i]->getType()==VertexType::BOUNDARY) continue;
        else tmp.push_back(_vertices[i]);
    }
    return tmp;
}

vector<EdgePair> ZXGraph::getInnerEdges() { 
    vector<EdgePair> tmp; tmp.clear(); 
    for(size_t i=0; i<_edges.size(); i++){
        if(_edges[i].first.first->getType() == VertexType::BOUNDARY || _edges[i].first.second->getType() == VertexType::BOUNDARY) continue;
        else tmp.push_back(_edges[i]);
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

void ZXGraph::concatenate(ZXGraph* tmp, bool remove_imm){
    // Add Vertices
    this -> addVertices( tmp -> getNonBoundary() );
    this -> addEdges( tmp -> getInnerEdges() );
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInp = tmp -> getInputList();
    for ( auto it = tmpInp.begin(); it != tmpInp.end(); ++it ){
        size_t inpQubit = it->first;
        // ZXVertex* targetInput = it ->second -> getNeighbors()[0].first;
        ZXVertex* targetInput = it ->second -> getNeighborMap().begin()->first;
        // ZXVertex* lastVertex = this -> getOutputFromHash(inpQubit) -> getNeighbors()[0].first;
        ZXVertex* lastVertex = this -> getOutputFromHash(inpQubit) -> getNeighborMap().begin()->first;
        tmp -> removeEdge(it->second, targetInput); // Remove old edge (disconnect old graph)
        if(remove_imm)
            this -> removeEdge(lastVertex, this -> getOutputFromHash(inpQubit)); // Remove old edge (output and prev-output)
        else
            lastVertex->disconnect(this -> getOutputFromHash(inpQubit));
        this -> addEdge(lastVertex, targetInput, new EdgeType(EdgeType::SIMPLE)); // Add new edge
        delete it->second;
    }
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOup = tmp -> getOutputList();
    for ( auto it = tmpOup.begin(); it != tmpOup.end(); ++it ){
        size_t oupQubit = it->first;
        // ZXVertex* targetOutput = it -> second -> getNeighbors()[0].first;
        ZXVertex* targetOutput = it -> second -> getNeighborMap().begin()->first;
        ZXVertex* ZXOup = this -> getOutputFromHash(oupQubit);
        tmp -> removeEdge(it->second, targetOutput); // Remove old edge (disconnect old graph)    
        this -> addEdge(targetOutput, ZXOup, new EdgeType(EdgeType::SIMPLE)); // Add new edge
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

void ZXGraph::tensorMapping(){
    updateTopoOrder();
    // pair<pair<ZXVertex*, ZXVertex*>, EdgeType*> EdgePair;
    // pair<pair<Done, Not Done>, EdgeType*>
    // unordered_multimap<EdgePair,size_t> cuttingEdges;
    using Frontiers = unordered_multimap<EdgePair,size_t>;
    vector<pair<Frontiers, QTensor<double>>> tensorList; 
    vector<EdgePair> zeroPins;

    auto tensordotVertex = [this, &zeroPins, &tensorList](ZXVertex *V) -> void
    {
        size_t tensorId = 0;
        if(verbose >= 3) cout << "Vertex " << V->getId() << " (" << V->getType() << ")" << _topoOrder.size()<<endl;
        NeighborMap neighborMap = V -> getNeighborMap();
        cout << "A" << endl;
        // Check if the vertex is from a new subgraph
        bool newGraph = true;
        for(auto itr = neighborMap.begin(); itr != neighborMap.end(); itr++){
            // cout << "B1" << itr->first->getId() <<itr->first->getPin() << endl;
            if (itr->first->getPin() != unsigned(-1)){
                tensorId = itr->first->getPin();
                newGraph = false;
                break;
            }
        }
        cout << "B" << endl;
        // Initialize subgraph
        if(newGraph){
            Frontiers front;
            QTensor<double> ntensor(1.+0.i);
            pair<Frontiers, QTensor<double>> tmp(front,ntensor);
            tensorList.push_back(tmp);
            tensorId = tensorList.size()-1;
        }
        cout << "C: newGraph = "<< newGraph << endl;
        // Retrieve information for tensordot
        vector<size_t> normal_pin;      // Axes that can be tensordotted directly
        vector<size_t> hadard_pin;      // Axes that should be applied hadamards first
        vector<EdgePair> remove_edge;   // Old frontiers to be removed
        vector<EdgePair> add_edge;      // New frontiers to be added
        
        if(newGraph){
            assert(V->getType() == VertexType::BOUNDARY);
            pair<ZXVertex*, ZXVertex*> vPair(V, neighborMap.begin()->first);
            EdgePair edgeKey(vPair, neighborMap.begin()->second);
            QTensor<double> tmp = tensordot(tensorList[tensorId].second, QTensor<double>::identity(neighborMap.size()));
            tensorList[tensorId].second = tmp;
            zeroPins.push_back(edgeKey);
            tensorList[tensorId].first.insert(make_pair(edgeKey, 1));
            cout << "D: CreateNewGraph" << endl;
        }
        else {
            if(V->getType()==VertexType::BOUNDARY){
                cout << "Found Boundary for a graph" << endl;
                V -> setPin(tensorId);
                return;
            }
            vector<ZXVertex*> alreadyRetrived;
            cout << "D: ConnectOldGraph" << endl;
            for(auto itr = neighborMap.begin(); itr != neighborMap.end(); itr++){
                pair<ZXVertex*, ZXVertex*> vPair(V, itr->first);
                if(find(alreadyRetrived.begin(), alreadyRetrived.end(), itr->first) == alreadyRetrived.end()){
                    alreadyRetrived.push_back(itr->first);
                
                    EdgePair edgeKey(vPair, itr->second);
                    auto result = tensorList[tensorId].first.equal_range(edgeKey);
                    for (auto jtr = result.first; jtr != result.second; jtr++) {
                        if ((itr->first->getPin() != unsigned(-1))){    // If the edge is a frontier
                            if( *(jtr->first.second) == EdgeType::HADAMARD ) hadard_pin.push_back(jtr -> second);
                            else normal_pin.push_back(jtr -> second);
                            remove_edge.push_back(edgeKey);
                        }
                        else{
                            add_edge.push_back(edgeKey);
                        }
                    }
                }
            }

            cout << "E: Finish Retriving" << endl;
            // Hadamard Edges to Normal Edges
            // 1. generate hadamard product
            QTensor<double> HEdge = QTensor<double>::hbox(2);
            QTensor<double> HTensorProduct = tensorPow(HEdge, hadard_pin.size());
            // 2. tensor dot
            vector<size_t> connect_pin;
            cout << "E1: Finish TSPower" << endl;
            for(size_t t=0; t<hadard_pin.size(); t++) 
                connect_pin.push_back(2*t);
            QTensor<double> postHadamardTranspose = tensordot(tensorList[tensorId].second, HTensorProduct, hadard_pin, connect_pin);
            // 3. update pins
            cout << "E2: Finish H Trans" << endl;
            for(size_t t=0; t<hadard_pin.size(); t++){
                hadard_pin[t] = postHadamardTranspose.getNewAxisId(tensorList[tensorId].second.dimension() + connect_pin[t] + 1); //dimension of big tensor + 1,3,5,7,9
            }
            cout << "E3: Finish Updating" << endl;
            cout << V->getId() << endl;
            QTensor<double> tmp = V->getTSform();
            cout << "F: Tensor Dot" << endl;
            // Tensor Dot
            // 1. Concatenate pins (hadamard and normal)
            for(size_t i=0; i<hadard_pin.size(); i++)
                normal_pin.push_back(hadard_pin[i]);
            // 2. tensor dot
            connect_pin.clear();
            for(size_t t=0; t<normal_pin.size(); t++) 
                connect_pin.push_back(t);
            tensorList[tensorId].second = tensordot(postHadamardTranspose, tmp, normal_pin, connect_pin);
            
            // 3. update pins
            for(size_t i=0; i<remove_edge.size(); i++)
                tensorList[tensorId].first.erase(remove_edge[i]);   // Erase old edges

            connect_pin.clear();
            for(size_t t=0; t<add_edge.size(); t++) 
                connect_pin.push_back(t);
            for(size_t t=0; t<add_edge.size(); t++){
                size_t newId = tensorList[tensorId].second.getNewAxisId(postHadamardTranspose.dimension() + connect_pin[t]);
                tensorList[tensorId].first.emplace(add_edge[t], newId); //origin pin (neighbot count) + 1,3,5,7,9
            }
        }
        cout << "_________" << endl;
        V -> setPin(tensorId);
        cout << tensorList[0].second << endl;
    };
    if(verbose >= 3)  cout << "---- TRAVERSE AND BUILD THE TENSOR ----" << endl;
    topoTraverse(tensordotVertex);
    // if(verbose >= 8) cout << tensorList[0].second << endl;
}