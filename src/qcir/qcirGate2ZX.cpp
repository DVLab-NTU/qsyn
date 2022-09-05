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

ZXGraph* HGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::H_BOX);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(3.14159)); // pi
    temp->printVertices();
    return temp;
}

ZXGraph* XGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::X);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(3.14159)); // pi
    temp->printVertices();
    return temp;
}

ZXGraph* SXGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::X);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(1.57080)); // pi/2
    temp->printVertices();
    return temp;
}

ZXGraph* CXGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit: _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit: _qubits[1]._qubit;
    temp->addInput(0, ctrl_qubit, VertexType::BOUNDARY);
    temp->addInput(1, targ_qubit, VertexType::BOUNDARY);
    temp->addVertex(2, ctrl_qubit, VertexType::Z);
    temp->addVertex(3, targ_qubit, VertexType::X);
    temp->addOutput(4, ctrl_qubit, VertexType::BOUNDARY);
    temp->addOutput(5, ctrl_qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,2,EdgeType::SIMPLE);
    temp->addEdgeById(2,4,EdgeType::SIMPLE);
    temp->addEdgeById(1,3,EdgeType::SIMPLE);
    temp->addEdgeById(3,5,EdgeType::SIMPLE);
    temp->addEdgeById(2,3,EdgeType::SIMPLE);
    temp->findVertexById(2)->setPhase(Phase(0)); // 0
    temp->findVertexById(3)->setPhase(Phase(0)); // 0
    temp->printVertices();
    return temp;
}

ZXGraph* ZGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::Z);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(3.14159)); // pi
    temp->printVertices();
    return temp;
}

ZXGraph* SGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::Z);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(1.57080)); // pi/2
    temp->printVertices();
    return temp;
}

ZXGraph* TGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::Z);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(0.78540)); // pi/4
    temp->printVertices();
    return temp;
}

ZXGraph* TDGGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::Z);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(Phase(-0.78540)); // -pi/4
    temp->printVertices();
    return temp;
}

ZXGraph* RZGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t qubit = _qubits[0]._qubit;
    temp->addInput(0, qubit, VertexType::BOUNDARY);
    temp->addVertex(1, qubit, VertexType::Z);
    temp->addOutput(2, qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,1,EdgeType::SIMPLE);
    temp->addEdgeById(1,2,EdgeType::SIMPLE);
    temp->findVertexById(1)->setPhase(_rotatePhase); 
    temp->printVertices();
    return temp;
}

ZXGraph* CZGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t ctrl_qubit = _qubits[0]._isTarget ? _qubits[1]._qubit: _qubits[0]._qubit;
    size_t targ_qubit = _qubits[0]._isTarget ? _qubits[0]._qubit: _qubits[1]._qubit;
    temp->addInput(0, ctrl_qubit, VertexType::BOUNDARY);
    temp->addInput(1, targ_qubit, VertexType::BOUNDARY);
    temp->addVertex(2, ctrl_qubit, VertexType::Z);
    temp->addVertex(3, targ_qubit, VertexType::Z);
    temp->addOutput(4, ctrl_qubit, VertexType::BOUNDARY);
    temp->addOutput(5, ctrl_qubit, VertexType::BOUNDARY);
    temp->addEdgeById(0,2,EdgeType::SIMPLE);
    temp->addEdgeById(2,4,EdgeType::SIMPLE);
    temp->addEdgeById(1,3,EdgeType::SIMPLE);
    temp->addEdgeById(3,5,EdgeType::SIMPLE);
    temp->addEdgeById(2,3,EdgeType::HADAMARD); // hadamard edge between z z
    temp->findVertexById(2)->setPhase(Phase(0)); // 0
    temp->findVertexById(3)->setPhase(Phase(0)); // 0
    temp->printVertices();
    return temp;
}
