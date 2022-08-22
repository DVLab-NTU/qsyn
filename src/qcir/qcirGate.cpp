/****************************************************************************
  FileName     [ qcirGate.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "qcirMgr.h"
#include "qcirGate.h"

using namespace std;

extern QCirMgr *qCirMgr;

void QCirGate::addParent(size_t qubit, QCirGate *p)
{
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    if (_qubits[i]._qubit == qubit)
    {
      _qubits[i]._parent = p;
      break;
    }
  }
}

void QCirGate::addChild(size_t qubit, QCirGate *c)
{
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    if (_qubits[i]._qubit == qubit)
    {
      _qubits[i]._child = c;
      break;
    }
  }
}

void QCirGate::printGate() const
{
  cout << "Gate " << _id << ": " << _type << "   \t"
       << " Exec Time: " << _time << " \t"
       << " Qubit: ";
  for (size_t i = 0; i < _qubits.size(); i++)
  {
    cout << _qubits[i]._qubit << " ";
  }
  cout << endl;
}