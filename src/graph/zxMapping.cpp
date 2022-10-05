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
    this->addVertices( tmp->getNonBoundary() );
    this->addEdges( tmp->getInnerEdges() );
    // Reconnect Input
    unordered_map<size_t, ZXVertex*> tmpInp = tmp->getInputList();
    for ( auto it = tmpInp.begin(); it != tmpInp.end(); ++it ){
        size_t inpQubit = it->first;
        // ZXVertex* targetInput = it ->second->getNeighbors()[0].first;
        ZXVertex* targetInput = it ->second->getNeighborMap().begin()->first;
        // ZXVertex* lastVertex = this->getOutputFromHash(inpQubit)->getNeighbors()[0].first;
        ZXVertex* lastVertex = this->getOutputFromHash(inpQubit)->getNeighborMap().begin()->first;
        tmp->removeEdge(it->second, targetInput); // Remove old edge (disconnect old graph)
        if(remove_imm)
            this->removeEdge(lastVertex, this->getOutputFromHash(inpQubit)); // Remove old edge (output and prev-output)
        else
            lastVertex->disconnect(this->getOutputFromHash(inpQubit));
        this->addEdge(lastVertex, targetInput, new EdgeType(EdgeType::SIMPLE)); // Add new edge
        delete it->second;
    }
    // Reconnect Output
    unordered_map<size_t, ZXVertex*> tmpOup = tmp->getOutputList();
    for ( auto it = tmpOup.begin(); it != tmpOup.end(); ++it ){
        size_t oupQubit = it->first;
        // ZXVertex* targetOutput = it->second->getNeighbors()[0].first;
        ZXVertex* targetOutput = it->second->getNeighborMap().begin()->first;
        ZXVertex* ZXOup = this->getOutputFromHash(oupQubit);
        tmp->removeEdge(it->second, targetOutput); // Remove old edge (disconnect old graph)    
        this->addEdge(targetOutput, ZXOup, new EdgeType(EdgeType::SIMPLE)); // Add new edge
        delete it->second;
    }
    tmp->reset();
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

class ZX2TSMapper {
public:
    using Frontiers = unordered_multimap<EdgePair,size_t>;

    ZX2TSMapper(ZXGraph* zxg): _zxgraph(zxg) {}
    class TensorList {
    public:
        const Frontiers& frontiers(const size_t& id) const {
            return _tensorList[id].first;
        }
        const QTensor<double>& tensor(const size_t& id) const {
            return _tensorList[id].second;
        }
        Frontiers& frontiers(const size_t& id) {
            return _tensorList[id].first;
        }
        QTensor<double>& tensor(const size_t& id) {
            return _tensorList[id].second;
        }
        void append(const Frontiers& f, const QTensor<double>& q) {
            _tensorList.emplace_back(f, q);
        }
        size_t size() {
            return _tensorList.size();
        }
    private:
        vector<pair<Frontiers, QTensor<double>>> _tensorList; 
    };
    void tensordotVertex(ZXVertex *V) {
        size_t tensorId = 0;
        
        if(verbose >= 3) cout << "Vertex " << V->getId() << " (" << V->getType() << ")" <<endl;
        NeighborMap neighborMap = V->getNeighborMap();

        auto isFrontier = [](const pair<ZXVertex*, EdgeType*>& epair) -> bool {
            return (epair.first->getPin() == unsigned(-1));
        };

        // Check if the vertex is from a new subgraph
        bool newGraph = true;
        for(auto epair : neighborMap){
            if (!isFrontier(epair)){
                tensorId = epair.first->getPin();
                newGraph = false;
                break;
            }
        }
        // New subgraph
        if (newGraph) {
            _tensorList.append(Frontiers(), QTensor<double>(1.+0.i));
            tensorId = _tensorList.size()-1;
        }

        Frontiers&       currFrontiers = _tensorList.frontiers(tensorId);
        QTensor<double>& currTensor    = _tensorList.tensor(tensorId);

        

        // New subgraph: early return
        if (newGraph) {
            assert(V->getType() == VertexType::BOUNDARY);
            EdgePair edgeKey = makeEdgeKey(V, neighborMap.begin()->first, neighborMap.begin()->second);
            currTensor = tensordot(currTensor, QTensor<double>::identity(neighborMap.size()));
            _zeroPins.push_back(edgeKey);
            currFrontiers.emplace(edgeKey, 1);
            V->setPin(tensorId);
            return;
        }

        // Existing subgraph
        // Retrieve information for tensordot
        TensorAxisList   normal_pin;    // Axes that can be tensordotted directly
        TensorAxisList   hadard_pin;    // Axes that should be applied hadamards first
        vector<EdgePair> remove_edge;   // Old frontiers to be removed
        vector<EdgePair> add_edge;      // New frontiers to be added

        if(V->getType()==VertexType::BOUNDARY){
            V->setPin(tensorId);
            if(verbose >= 3) cout << "Boundary Node" <<endl;
            return;
        }
        NeighborMap frontAlreadyRetrived;
        for(auto epair : neighborMap){
            ZXVertex* const& neighbor = epair.first;
            EdgeType* const& etype = epair.second;

            EdgePair edgeKey = makeEdgeKey(V, neighbor, etype);
            if (!isFrontier(epair) && !frontAlreadyRetrived.contains(neighbor)){
                frontAlreadyRetrived.emplace(neighbor, etype);
                auto result = currFrontiers.equal_range(edgeKey);
                for (auto jtr = result.first; jtr != result.second; jtr++) {
                    auto& [epair, id] = *jtr;
                    if( *(epair.second) == EdgeType::HADAMARD ) hadard_pin.push_back(id);
                    else normal_pin.push_back(id);
                    
                    remove_edge.push_back(edgeKey);
                }
            }
            else {
                add_edge.push_back(edgeKey);
            }
        }

        if(verbose >= 7) {
            cout << "**************" << endl;
            cout << "Current Frontier Edges: " << endl;
            for(size_t i=0;i<remove_edge.size();i++){
                cout << remove_edge[i].first.first->getId()<<"--"<< remove_edge[i].first.second->getId() << endl;
            }
            cout << "Next Frontier Edges: " << endl;
            for(size_t i=0;i<add_edge.size();i++){
                cout << add_edge[i].first.first->getId()<<"--"<< add_edge[i].first.second->getId() << endl;
            }
            cout << "**************" << endl;
        }
        // Hadamard Edges to Normal Edges
        // 1. generate hadamard product
        QTensor<double> HTensorProduct = tensorPow(QTensor<double>::hbox(2), hadard_pin.size());
        // 2. tensor dot
        TensorAxisList connect_pin;
        for(size_t t=0; t<hadard_pin.size(); t++) 
            connect_pin.push_back(2*t);
        QTensor<double> postHadamardTranspose = tensordot(currTensor, HTensorProduct, hadard_pin, connect_pin);
        // 3. update pins
        for(size_t t=0; t<hadard_pin.size(); t++){
            hadard_pin[t] = postHadamardTranspose.getNewAxisId(currTensor.dimension() + connect_pin[t] + 1); //dimension of big tensor + 1,3,5,7,9
        }
        if(verbose >= 7) cout << "Start tensor dot..." << endl;
        // Tensor Dot
        // 1. Concatenate pins (hadamard and normal)
        normal_pin = concatAxisList(hadard_pin, normal_pin);
        // 2. tensor dot
        connect_pin.clear();
        for(size_t t=0; t<normal_pin.size(); t++) 
            connect_pin.push_back(t);
        currTensor = tensordot(postHadamardTranspose, V->getTSform(), normal_pin, connect_pin);
        
        // 3. update pins
        for(size_t i=0; i<remove_edge.size(); i++)
            currFrontiers.erase(remove_edge[i]);   // Erase old edges

        for(auto& frontier : currFrontiers) {
            frontier.second = currTensor.getNewAxisId(frontier.second);
        }

        connect_pin.clear();
        for(size_t t=0; t<add_edge.size(); t++) 
            connect_pin.push_back(normal_pin.size() + t);

        for(size_t t=0; t<add_edge.size(); t++){
            size_t newId = currTensor.getNewAxisId(postHadamardTranspose.dimension() + connect_pin[t]);
            currFrontiers.emplace(add_edge[t], newId); // origin pin (neighbot count) + 1,3,5,7,9
        }
        V->setPin(tensorId);
    }

    void map() {
        _zxgraph->updateTopoOrder();
        if(verbose >= 3)  cout << "---- TRAVERSE AND BUILD THE TENSOR ----" << endl;
        _zxgraph->topoTraverse([this](ZXVertex* v) { tensordotVertex(v); });
        

        for(size_t i=0; i<_zeroPins.size(); i++)
            _tensorList.frontiers(i).emplace(_zeroPins[i], 0);


        TensorAxisList inputIds, outputIds;
        getAxisOrders(inputIds , _zxgraph->getInputList());
        getAxisOrders(outputIds, _zxgraph->getOutputList());

        _tensorList.tensor(0) =  _tensorList.tensor(0).transpose(concatAxisList(inputIds, outputIds));
        if(verbose >= 3)  cout <<  _tensorList.tensor(0) << endl;
    }

private:
    ZXGraph* _zxgraph;
    TensorList _tensorList;
    vector<EdgePair> _zeroPins;

    EdgePair makeEdgeKey(ZXVertex* v1, ZXVertex* v2, EdgeType* et) {
        return make_pair(
            (v2->getId() < v1->getId()) ? make_pair(v2, v1) : make_pair(v1, v2),
            et
        );
    }

    void getAxisOrders(TensorAxisList& axList, const std::unordered_map<size_t, ZXVertex *>& ioList){
        axList.resize(ioList.size());
        for (auto& [qubitId, vertex] : ioList) {
            NeighborMap nebs = vertex->getNeighborMap();
            auto& [neighbor, etype] = *(nebs.begin());
            EdgePair edgeKey = makeEdgeKey(vertex, neighbor, etype);
            axList[qubitId] = _tensorList.frontiers(0).find(edgeKey)->second;
        }
    }

};

void ZXGraph::tensorMapping(){
        ZX2TSMapper mapper(this);
        mapper.map();
}