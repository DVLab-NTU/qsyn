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
    temp->addVertex(baseId + 1, qubit, VertexType::H_BOX);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(3.14159)); // pi
    baseId ++;
    return temp;
}

ZXGraph* XGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::X);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(3.14159)); // pi
    baseId ++;
    return temp;
}

ZXGraph* SXGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::X);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(1.57080)); // pi/2
    baseId ++;
    return temp;
}

ZXGraph* CXGate::getZXform(size_t &baseId)
{
    cout << "Base: "<<baseId<<endl;
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit: _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit: _qubits[1]._qubit;
    temp->addInput(baseId - 1, ctrl_qubit);
    temp->addInput(baseId - 2, targ_qubit);
    temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z);
    temp->addVertex(baseId + 2, targ_qubit, VertexType::X);
    temp->addOutput(baseId - 3, ctrl_qubit);
    temp->addOutput(baseId - 4, targ_qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 3,EdgeType::SIMPLE);
    temp->addEdgeById(baseId - 2,baseId + 2,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 2,baseId - 4,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1, baseId + 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(0)); // 0
    temp->findVertexById(baseId + 2)->setPhase(Phase(0)); // 0
    baseId +=2;
    return temp;
}

ZXGraph* ZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(3.14159)); // pi
    baseId ++;
    return temp;
}

ZXGraph* SGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(1.57080)); // pi/2
    baseId ++;
    return temp;
}

ZXGraph* TGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(0.78540)); // pi/4
    baseId ++;
    return temp;
}

ZXGraph* TDGGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(Phase(-0.78540)); // -pi/4
    baseId ++;
    return temp;
}

ZXGraph* RZGate::getZXform(size_t &baseId)
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(baseId - 1, qubit);
    temp->addVertex(baseId + 1, qubit, VertexType::Z);
    temp->addOutput(baseId - 2, qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 2,EdgeType::SIMPLE);
    temp->findVertexById(baseId + 1)->setPhase(_rotatePhase); 
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
    temp->addVertex(baseId + 1, ctrl_qubit, VertexType::Z);
    temp->addVertex(baseId + 2, targ_qubit, VertexType::Z);
    temp->addOutput(baseId - 3, ctrl_qubit);
    temp->addOutput(baseId - 4, targ_qubit);
    temp->addEdgeById(baseId - 1,baseId + 1,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1,baseId - 3,EdgeType::SIMPLE);
    temp->addEdgeById(baseId - 2,baseId + 2,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 2,baseId - 4,EdgeType::SIMPLE);
    temp->addEdgeById(baseId + 1, baseId + 2,EdgeType::HADAMARD);
    temp->findVertexById(baseId + 1)->setPhase(Phase(0)); // 0
    temp->findVertexById(baseId + 2)->setPhase(Phase(0)); // 0
    baseId +=2;
    return temp;
}
