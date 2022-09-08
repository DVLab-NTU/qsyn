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

ZXGraph* HGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::H_BOX, Phase(1)); //pi
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* XGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1)); //pi
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* SXGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::X, Phase(1,2)); //pi/2
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* CXGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit: _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit: _qubits[1]._qubit;
    temp->addInput(baseId - 1, ctrl_qubit);
    temp->addInput(baseId - 2, targ_qubit);
    temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, Phase(0));
    temp->addVertex(baseId + 2, targ_qubit, VertexType::X, Phase(0));
    temp->addOutput(baseId - 3, ctrl_qubit);
    temp->addOutput(baseId - 4, targ_qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 3,EdgeType::SIMPLE);
    temp->addEdgeById(baseId - 2,baseId + 2,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 2,baseId - 4,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1, baseId + 2,EdgeType::SIMPLE);
    baseId +=2;
    return temp;
}

ZXGraph* ZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1));
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* SGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1,2));
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* TGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(1,4));
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* TDGGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(-1,4));
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* RZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z, Phase(_rotatePhase));
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    baseId ++;
    return temp;
}

ZXGraph* CZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit: _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit: _qubits[1]._qubit;
    temp->addInput(baseId - 1, ctrl_qubit);
    temp->addInput(baseId - 2, targ_qubit);
    temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z, Phase(0));
    temp->addVertex(baseId + 2, targ_qubit, VertexType::Z, Phase(0));
    temp->addOutput(baseId - 3, ctrl_qubit);
    temp->addOutput(baseId - 4, targ_qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 3,EdgeType::SIMPLE);
    temp->addEdgeById(baseId - 2,baseId + 2,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 2,baseId - 4,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1, baseId + 2,EdgeType::HADAMARD);
    baseId +=2;
    return temp;
}
