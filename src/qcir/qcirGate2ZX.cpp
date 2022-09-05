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
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::H_BOX);
    temp->findVertexById(nodeId)->setPhase(Phase(3.14159)); // pi
    temp->printVertices();
    return temp;
}

ZXGraph* XGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::X);
    temp->findVertexById(nodeId)->setPhase(Phase(3.14159)); // pi
    temp->printVertices();
    return temp;
}

ZXGraph* SXGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::X);
    temp->findVertexById(nodeId)->setPhase(Phase(1.57080)); // pi/2
    temp->printVertices();
    return temp;
}

ZXGraph* ZGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::Z);
    temp->findVertexById(nodeId)->setPhase(Phase(3.14159)); // pi
    temp->printVertices();
    return temp;
}

ZXGraph* SGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::Z);
    temp->findVertexById(nodeId)->setPhase(Phase(1.57080)); // pi/2
    temp->printVertices();
    return temp;
}

ZXGraph* TGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::Z);
    temp->findVertexById(nodeId)->setPhase(Phase(0.78540)); // pi/4
    temp->printVertices();
    return temp;
}

ZXGraph* TDGGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::Z);
    temp->findVertexById(nodeId)->setPhase(Phase(-0.78540)); // -pi/4
    temp->printVertices();
    return temp;
}

ZXGraph* RZGate::getZXform()
{
    ZXGraph *temp = new ZXGraph(_id);
    size_t nodeId = 0;
    size_t qubit = _qubits[0]._qubit;
    temp->addVertex(nodeId, qubit, VertexType::Z);
    temp->findVertexById(nodeId)->setPhase(_rotatePhase); 
    temp->printVertices();
    return temp;
}