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
