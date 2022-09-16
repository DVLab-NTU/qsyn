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
#include "qcirMgr.h"
#include "zxGraph.h"

extern size_t verbose;

ZXGraph *HGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *H = temp->addVertex(baseId + 1, qubit, VertexType::H_BOX, verbose, Phase(1)); // pi
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, H, EdgeType::SIMPLE, verbose);
    temp->addEdge(H, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *XGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *X = temp->addVertex(baseId + 1, qubit, VertexType::X, verbose, Phase(1)); // pi
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, X, EdgeType::SIMPLE, verbose);
    temp->addEdge(X, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *SXGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *SX = temp->addVertex(baseId + 1, qubit, VertexType::X, verbose, Phase(1, 2)); // pi/2
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, SX, EdgeType::SIMPLE, verbose);
    temp->addEdge(SX, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *CXGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    ZXVertex *in_ctrl = temp->addInput(baseId + 3, ctrl_qubit, verbose);
    ZXVertex *in_targ = temp->addInput(baseId + 4, targ_qubit, verbose);
    ZXVertex *ctrl = temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, verbose, Phase(0));
    ZXVertex *targX = temp->addVertex(baseId + 2, targ_qubit, VertexType::X, verbose, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(baseId + 5, ctrl_qubit, verbose);
    ZXVertex *out_targ = temp->addOutput(baseId + 6, targ_qubit, verbose);
    temp->addEdge(in_ctrl, ctrl, EdgeType::SIMPLE, verbose);
    temp->addEdge(ctrl, out_ctrl, EdgeType::SIMPLE, verbose);
    temp->addEdge(in_targ, targX, EdgeType::SIMPLE, verbose);
    temp->addEdge(targX, out_targ, EdgeType::SIMPLE, verbose);
    temp->addEdge(ctrl, targX, EdgeType::SIMPLE, verbose);
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    baseId += 2;
    return temp;
}

ZXGraph *ZGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *Z = temp->addVertex(baseId + 1, qubit, VertexType::Z, verbose, Phase(1));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, Z, EdgeType::SIMPLE, verbose);
    temp->addEdge(Z, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *SGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *S = temp->addVertex(baseId + 1, qubit, VertexType::Z, verbose, Phase(1, 2));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, S, EdgeType::SIMPLE, verbose);
    temp->addEdge(S, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *TGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *T = temp->addVertex(baseId + 1, qubit, VertexType::Z, verbose, Phase(1, 4));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, T, EdgeType::SIMPLE, verbose);
    temp->addEdge(T, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *TDGGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *TDG = temp->addVertex(baseId + 1, qubit, VertexType::Z, verbose, Phase(-1, 4));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, TDG, EdgeType::SIMPLE, verbose);
    temp->addEdge(TDG, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *RZGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex *in = temp->addInput(baseId + 2, qubit, verbose);
    ZXVertex *RZ = temp->addVertex(baseId + 1, qubit, VertexType::Z, verbose, Phase(_rotatePhase));
    ZXVertex *out = temp->addOutput(baseId + 3, qubit, verbose);
    temp->addEdge(in, RZ, EdgeType::SIMPLE, verbose);
    temp->addEdge(RZ, out, EdgeType::SIMPLE, verbose);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *CZGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit : _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit : _qubits[1]._qubit;
    ZXVertex *in_ctrl = temp->addInput(baseId + 3, ctrl_qubit, verbose);
    ZXVertex *in_targ = temp->addInput(baseId + 4, targ_qubit, verbose);
    ZXVertex *ctrl = temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, verbose, Phase(0));
    ZXVertex *targZ = temp->addVertex(baseId + 2, targ_qubit, VertexType::Z, verbose, Phase(0));
    ZXVertex *out_ctrl = temp->addOutput(baseId + 5, ctrl_qubit, verbose);
    ZXVertex *out_targ = temp->addOutput(baseId + 6, targ_qubit, verbose);
    temp->addEdge(in_ctrl, ctrl, EdgeType::SIMPLE, verbose);
    temp->addEdge(ctrl, out_ctrl, EdgeType::SIMPLE, verbose);
    temp->addEdge(in_targ, targZ, EdgeType::SIMPLE, verbose);
    temp->addEdge(targZ, out_targ, EdgeType::SIMPLE, verbose);
    temp->addEdge(ctrl, targZ, EdgeType::HADAMARD, verbose);
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    baseId += 2;
    return temp;
}
