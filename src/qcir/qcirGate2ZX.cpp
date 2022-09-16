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

ZXGraph *HGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* H = temp->addVertex(baseId + 1, qubit, VertexType::H_BOX, Phase(1)); // pi
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, H, EdgeType::SIMPLE, silent);
    temp->addEdge(H, out, EdgeType::SIMPLE, silent);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *XGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* X = temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1)); // pi
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, X, EdgeType::SIMPLE, silent);
    temp->addEdge(X, out, EdgeType::SIMPLE, silent);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *SXGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* SX = temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1, 2)); // pi/2
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, SX, EdgeType::SIMPLE, silent);
    temp->addEdge(SX, out, EdgeType::SIMPLE, silent);
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
    ZXVertex* in_ctrl = temp->addInput(baseId - 1, ctrl_qubit);
    ZXVertex* in_targ = temp->addInput(baseId - 2, targ_qubit);
    ZXVertex* ctrl = temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex* targX = temp->addVertex(baseId + 2, targ_qubit, VertexType::X, Phase(0));
    ZXVertex* out_ctrl = temp->addOutput(baseId - 3, ctrl_qubit);
    ZXVertex* out_targ = temp->addOutput(baseId - 4, targ_qubit);
    temp->addEdge(in_ctrl, ctrl, EdgeType::SIMPLE, silent);
    temp->addEdge(ctrl, out_ctrl, EdgeType::SIMPLE, silent);
    temp->addEdge(in_targ, targX, EdgeType::SIMPLE, silent);
    temp->addEdge(targX, out_targ, EdgeType::SIMPLE, silent);
    temp->addEdge(ctrl, targX, EdgeType::SIMPLE, silent);
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
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* Z = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1));
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, Z, EdgeType::SIMPLE, silent);
    temp->addEdge(Z, out, EdgeType::SIMPLE, silent);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *SGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* S = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1, 2));
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, S, EdgeType::SIMPLE, silent);
    temp->addEdge(S, out, EdgeType::SIMPLE, silent);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *TGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* T = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1, 4));
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, T, EdgeType::SIMPLE, silent);
    temp->addEdge(T, out, EdgeType::SIMPLE, silent);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *TDGGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* TDG = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(-1, 4));
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, TDG, EdgeType::SIMPLE, silent);
    temp->addEdge(TDG, out, EdgeType::SIMPLE, silent);
    temp->setInputHash(qubit, in);
    temp->setOutputHash(qubit, out);
    baseId++;
    return temp;
}

ZXGraph *RZGate::getZXform(size_t &baseId, bool silent)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    ZXVertex* in = temp->addInput(baseId - 1, qubit);
    ZXVertex* RZ = temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(_rotatePhase));
    ZXVertex* out = temp->addOutput(baseId - 2, qubit);
    temp->addEdge(in, RZ, EdgeType::SIMPLE, silent);
    temp->addEdge(RZ, out, EdgeType::SIMPLE, silent);
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
    ZXVertex* in_ctrl = temp->addInput(baseId - 1, ctrl_qubit);
    ZXVertex* in_targ = temp->addInput(baseId - 2, targ_qubit);
    ZXVertex* ctrl = temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, Phase(0));
    ZXVertex* targZ = temp->addVertex(baseId + 2, targ_qubit, VertexType::Z, Phase(0));
    ZXVertex* out_ctrl = temp->addOutput(baseId - 3, ctrl_qubit);
    ZXVertex* out_targ = temp->addOutput(baseId - 4, targ_qubit);
    temp->addEdge(in_ctrl, ctrl, EdgeType::SIMPLE, silent);
    temp->addEdge(ctrl, out_ctrl, EdgeType::SIMPLE, silent);
    temp->addEdge(in_targ, targZ, EdgeType::SIMPLE, silent);
    temp->addEdge(targZ, out_targ, EdgeType::SIMPLE, silent);
    temp->addEdge(ctrl, targZ, EdgeType::HADAMARD, silent);
    temp->setInputHash(ctrl_qubit, in_ctrl);
    temp->setOutputHash(ctrl_qubit, out_ctrl);
    temp->setInputHash(targ_qubit, in_targ);
    temp->setOutputHash(targ_qubit, out_targ);
    baseId += 2;
    return temp;
}
