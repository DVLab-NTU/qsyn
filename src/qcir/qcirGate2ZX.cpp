/****************************************************************************
  FileName     [ qcirGate2ZX.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate mapping functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cassert>
#include "qcir.h"
#include "zxGraph.h"

extern size_t verbose;

/// @brief map single qubit gate to zx
/// @param vt 
/// @param ph 
/// @return 
ZXGraph *QCirGate::mapSingleQubitGate(VertexType vt, Phase ph){
    
    ZXGraph* g = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex* in = g->addInput(qubit);
    ZXVertex* gate = g->addVertex(qubit, vt, ph);
    ZXVertex* out = g->addOutput(qubit);
    g->addEdge(in, gate, EdgeType(EdgeType::SIMPLE));
    g->addEdge(gate, out, EdgeType(EdgeType::SIMPLE));
    g->setInputHash(qubit, in);
    g->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    return g;
}

// Single Qubit Gate X/Z
//REVIEW - Rewrite

/// @brief get ZX-graph
/// @return 
ZXGraph *HGate::getZXform(){
    return mapSingleQubitGate(VertexType::H_BOX, Phase(1));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *XGate::getZXform(){
    return mapSingleQubitGate(VertexType::X, Phase(1));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *SXGate::getZXform(){
    return mapSingleQubitGate(VertexType::X, Phase(1, 2));
}

ZXGraph *ZGate::getZXform(){
    return mapSingleQubitGate(VertexType::Z, Phase(1));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *SGate::getZXform(){
    return mapSingleQubitGate(VertexType::Z, Phase(1, 2));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *SDGGate::getZXform(){
    return mapSingleQubitGate(VertexType::Z, Phase(-1, 2));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *TGate::getZXform(){
    return mapSingleQubitGate(VertexType::Z, Phase(1, 4));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *TDGGate::getZXform(){
    return mapSingleQubitGate(VertexType::Z, Phase(-1, 4));
}

/// @brief get ZX-graph
/// @return 
ZXGraph *RZGate::getZXform(){
    return mapSingleQubitGate(VertexType::Z, Phase(_rotatePhase));
}

// Double or More Qubit Gate
//NOTE - Not necessary to rewrite

/// @brief get ZX-graph
/// @return 
ZXGraph *CXGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in_ctrl = temp->addInput(ctrl_qubit);
    ZXVertex *in_targ = temp->addInput(targ_qubit);
    ZXVertex *ctrl = temp->addVertex(ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex *targX = temp->addVertex(targ_qubit, VertexType::X, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(ctrl_qubit);
    ZXVertex *out_targ = temp->addOutput(targ_qubit);
    temp->addEdge(in_ctrl, ctrl, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, out_ctrl, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_targ, targX, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(targX, out_targ, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, targX, EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if(verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/// @brief get ZX-graph
/// @return 
ZXGraph *CCXGate::getZXform() // Decomposed into 21 vertices (6X + 6Z + 4T + 3Tdg + 2H)
{
    ZXGraph *temp = new ZXGraph(_id);
    
    size_t ctrl_qubit_1 = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t ctrl_qubit_2 = _qubits[0]._isTarget ? _qubits[2]._qubit : (_qubits[1]._isTarget ? _qubits[2]._qubit : _qubits[1]._qubit);
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : (_qubits[1]._isTarget ? _qubits[1]._qubit : _qubits[2]._qubit);
    
    // Build vertices table and adjacency pairs of edges
    vector<pair<pair<VertexType, Phase>, size_t>>  Vertices_info = {{{VertexType::H_BOX, Phase(1)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2},
    {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1, 4)}, targ_qubit},{{VertexType::Z, Phase(0)}, ctrl_qubit_1},{{VertexType::X, Phase(0)}, targ_qubit}, 
    {{VertexType::Z, Phase(1,4)}, targ_qubit}, {{VertexType::Z, Phase(0)}, ctrl_qubit_2}, {{VertexType::X, Phase(0)}, targ_qubit}, {{VertexType::Z, Phase(-1,4)}, targ_qubit},
    {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, targ_qubit},{{VertexType::Z, Phase(1,4)}, ctrl_qubit_2}, {{VertexType::Z, Phase(1,4)}, targ_qubit}, 
    {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}, {{VertexType::H_BOX, Phase(1)}, targ_qubit},{{VertexType::Z, Phase(1,4)}, ctrl_qubit_1}, 
    {{VertexType::Z, Phase(-1,4)}, ctrl_qubit_1}, {{VertexType::Z, Phase(0)}, ctrl_qubit_1}, {{VertexType::X, Phase(0)}, ctrl_qubit_2}};

    vector<pair<size_t, size_t>> adj_pair={{0,2},{1,2},{2,3},{4,5},{3,5},{5,6},{6,8},{1,7},{7,8},{8,9},{9,11},{10,11},{4,10},
    {10,14},{11,13},{7,12},{14,15},{12,15},{13,16},{14,17},{17,19},{15,18},{18,20},{19,20}};

    vector<ZXVertex*> Vertices_list={};

    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in_ctrl_1 = temp->addInput(ctrl_qubit_1);
    ZXVertex *in_ctrl_2 = temp->addInput(ctrl_qubit_2);
    ZXVertex *in_targ = temp->addInput(targ_qubit);
    for (size_t i=0; i<Vertices_info.size(); i++){
        Vertices_list.push_back(temp->addVertex(Vertices_info[i].second, Vertices_info[i].first.first, Vertices_info[i].first.second));}
    ZXVertex *out_ctrl_1 = temp->addOutput(ctrl_qubit_1);
    ZXVertex *out_ctrl_2 = temp->addOutput(ctrl_qubit_2);
    ZXVertex *out_targ = temp->addOutput(targ_qubit);

    temp->addEdge(in_ctrl_1 , Vertices_list[4] , EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_ctrl_2 , Vertices_list[1] , EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_targ   , Vertices_list[0] , EdgeType(EdgeType::SIMPLE));
    temp->addEdge(out_ctrl_1, Vertices_list[19], EdgeType(EdgeType::SIMPLE));
    temp->addEdge(out_ctrl_2, Vertices_list[20], EdgeType(EdgeType::SIMPLE));
    temp->addEdge(out_targ  , Vertices_list[16], EdgeType(EdgeType::SIMPLE));
    for (size_t i=0; i<adj_pair.size(); i++) {temp->addEdge(Vertices_list[adj_pair[i].first], Vertices_list[adj_pair[i].second], EdgeType(EdgeType::SIMPLE));}
    
    temp->setInputHash(ctrl_qubit_1, in_ctrl_1);
    temp->setOutputHash(ctrl_qubit_1, out_ctrl_1);
    temp->setInputHash(ctrl_qubit_2, in_ctrl_2);
    temp->setOutputHash(ctrl_qubit_2, out_ctrl_2);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if(verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/// @brief get ZX-graph
/// @return 
ZXGraph *CZGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;    
    ZXVertex *in_ctrl = temp->addInput(ctrl_qubit);
    ZXVertex *in_targ = temp->addInput(targ_qubit);
    ZXVertex *ctrl = temp->addVertex(ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex *targZ = temp->addVertex(targ_qubit, VertexType::Z, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(ctrl_qubit);
    ZXVertex *out_targ = temp->addOutput(targ_qubit);
    temp->addEdge(in_ctrl, ctrl, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, out_ctrl, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_targ, targZ, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(targZ, out_targ, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, targZ, EdgeType(EdgeType::HADAMARD));
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if(verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

// Y Gate
//NOTE - Cannot use mapSingleQubitGate

/// @brief get ZX-graph
/// @return 
ZXGraph *YGate::getZXform() // Y = iXZ
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(qubit);
    ZXVertex *X = temp->addVertex(qubit, VertexType::X, Phase(1));
    ZXVertex *Z = temp->addVertex(qubit, VertexType::Z, Phase(1));
    ZXVertex *out = temp->addOutput(qubit);
    temp->addEdge(in, X, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(X, Z, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(Z, out, EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/// @brief get ZX-graph
/// @return 
ZXGraph *SYGate::getZXform() // SY = S。SX。Sdg
  {  
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(qubit);
    ZXVertex *S = temp->addVertex(qubit, VertexType::Z, Phase(1, 2));
    ZXVertex *SX = temp->addVertex(qubit, VertexType::X, Phase(1, 2));
    ZXVertex *Sdg = temp->addVertex(qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex *out = temp->addOutput(qubit);
    temp->addEdge(in, S, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(S, SX, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(SX, Sdg, EdgeType(EdgeType::SIMPLE));
    temp->addEdge(Sdg, out, EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    return temp;
}

/// @brief get ZX-graph of cnrz
/// @return 
ZXGraph *CnRZGate::getZXform(){
    ZXGraph *temp = new ZXGraph(_id);
    //NOTE -  _qubits can get the qubit id, only the last one is the target

    //FIXME - below creates n idendities.
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    for(const auto bitinfo:_qubits){
        size_t qubit = bitinfo._qubit;
        ZXVertex* in = temp->addInput(qubit);
        ZXVertex* gate = temp->addVertex(qubit, VertexType::Z, Phase(0));
        ZXVertex* out = temp->addOutput(qubit);
        temp->addEdge(in, gate, EdgeType(EdgeType::SIMPLE));
        temp->addEdge(gate, out, EdgeType(EdgeType::SIMPLE));
        temp->setInputHash(qubit, in);
        temp->setOutputHash(qubit, out);
    }
    return temp;
}