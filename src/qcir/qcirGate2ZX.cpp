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

ZXGraph *HGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *H = temp->addVertex(baseId + 1, qubit, VertexType::H_BOX, Phase(1)); // pi
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, H, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(H, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *XGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *X = temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1)); // pi
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, X, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(X, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *SXGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *SX = temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1, 2)); // pi/2
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, SX, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(SX, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *CXGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in_ctrl = temp->addInput(baseId + 3, ctrl_qubit);
    ZXVertex *in_targ = temp->addInput(baseId + 4, targ_qubit);
    ZXVertex *ctrl = temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex *targX = temp->addVertex(baseId + 2, targ_qubit, VertexType::X, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(baseId + 5, ctrl_qubit);
    ZXVertex *out_targ = temp->addOutput(baseId + 6, targ_qubit);
    temp->addEdge(in_ctrl, ctrl, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, out_ctrl, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_targ, targX, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(targX, out_targ, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, targX, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId += 2;
    return temp;
}

ZXGraph *CCXGate::getZXform(size_t &baseId) // Decomposed into 21 vertices (6X + 6Z + 4T + 3Tdg + 2H)
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
    ZXVertex *in_ctrl_1 = temp->addInput(baseId + 22, ctrl_qubit_1);
    ZXVertex *in_ctrl_2 = temp->addInput(baseId + 23, ctrl_qubit_2);
    ZXVertex *in_targ = temp->addInput(baseId + 24, targ_qubit);
    for (size_t i=0; i<Vertices_info.size(); i++){
        Vertices_list.push_back(temp->addVertex(baseId+1+i, Vertices_info[i].second, Vertices_info[i].first.first, Vertices_info[i].first.second));}
    ZXVertex *out_ctrl_1 = temp->addOutput(baseId + 25, ctrl_qubit_1);
    ZXVertex *out_ctrl_2 = temp->addOutput(baseId + 26, ctrl_qubit_2);
    ZXVertex *out_targ = temp->addOutput(baseId + 27, targ_qubit);

    temp->addEdge(in_ctrl_1, Vertices_list[4], new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_ctrl_2, Vertices_list[1], new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_targ, Vertices_list[0], new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(out_ctrl_1, Vertices_list[19], new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(out_ctrl_2, Vertices_list[20], new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(out_targ, Vertices_list[16], new EdgeType(EdgeType::SIMPLE));
    for (size_t i=0; i<adj_pair.size(); i++) {temp->addEdge(Vertices_list[adj_pair[i].first], Vertices_list[adj_pair[i].second], new EdgeType(EdgeType::SIMPLE));}
    
    temp->setInputHash(ctrl_qubit_1, in_ctrl_1);
    temp->setOutputHash(ctrl_qubit_1, out_ctrl_1);
    temp->setInputHash(ctrl_qubit_2, in_ctrl_2);
    temp->setOutputHash(ctrl_qubit_2, out_ctrl_2);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId += 21;
    return temp;
}

ZXGraph *ZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *Z = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, Z, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(Z, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *SGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *S = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1, 2));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, S, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(S, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *SDGGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *Sdg = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, Sdg, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(Sdg, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *TGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *T = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1, 4));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, T, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(T, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *TDGGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *TDG = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(-1, 4));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, TDG, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(TDG, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *RZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 2, qubit);
    ZXVertex *RZ = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(_rotatePhase));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit);
    temp->addEdge(in, RZ, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(RZ, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId++;
    return temp;
}

ZXGraph *CZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;    
    ZXVertex *in_ctrl = temp->addInput(baseId + 3, ctrl_qubit);
    ZXVertex *in_targ = temp->addInput(baseId + 4, targ_qubit);
    ZXVertex *ctrl = temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex *targZ = temp->addVertex(baseId + 2, targ_qubit, VertexType::Z, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(baseId + 5, ctrl_qubit);
    ZXVertex *out_targ = temp->addOutput(baseId + 6, targ_qubit);
    temp->addEdge(in_ctrl, ctrl, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, out_ctrl, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(in_targ, targZ, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(targZ, out_targ, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(ctrl, targZ, new EdgeType(EdgeType::HADAMARD));
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId += 2;
    return temp;
}

ZXGraph *YGate::getZXform(size_t &baseId) // Y = iXZ
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 3, qubit);
    ZXVertex *X = temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1));
    ZXVertex *Z = temp->addVertex(baseId + 2, qubit, VertexType::Z, Phase(1));
    ZXVertex *out = temp->addOutput(baseId + 4, qubit);
    temp->addEdge(in, X, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(X, Z, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(Z, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId+=2;
    return temp;
}

ZXGraph *SYGate::getZXform(size_t &baseId) // SY = S。SX。Sdg
  {  
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    if(verbose >= 5) cout << "**** Generate ZX of Gate " << getId() << " (" << getTypeStr() << ") ****" << endl;  
    ZXVertex *in = temp->addInput(baseId + 4, qubit);
    ZXVertex *S = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1, 2));
    ZXVertex *SX = temp->addVertex(baseId + 2, qubit, VertexType::X, Phase(1, 2));
    ZXVertex *Sdg = temp->addVertex(baseId + 3, qubit, VertexType::Z, Phase(-1, 2));
    ZXVertex *out = temp->addOutput(baseId + 5, qubit);
    temp->addEdge(in, S, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(S, SX, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(SX, Sdg, new EdgeType(EdgeType::SIMPLE));
    temp->addEdge(Sdg, out, new EdgeType(EdgeType::SIMPLE));
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    if(verbose >= 5) cout << "***********************************" << endl;
    baseId+=3;
    return temp;
}