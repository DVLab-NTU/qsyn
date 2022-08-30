/****************************************************************************
  FileName     [ qcirMgr.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define quantum circuit manager ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_MGR_H
#define QCIR_MGR_H

#include <vector>
#include <stack>
#include <string>
#include <fstream>
#include <iostream>
#include "qcirGate.h"
#include "qcirQubit.h"
#include "qcirDef.h"

extern QCirMgr *qCirMgr;
using namespace std;

class QCirMgr
{
public:
  QCirMgr()
  {
    _gateId = 0;
    _qubitId = 0;
    _globalDFScounter = 1;
    _dirty = false;
    _qgate.clear();
    _qubits.clear();
    while(!_topoOrder.empty())  _topoOrder.pop();
  }
  ~QCirMgr() {}

  // Access functions
  // return '0' if "gid" corresponds to an undefined gate.
  QCirGate *getGate(size_t gid) const;
  QCirQubit *getQubit(size_t qid) const;
  size_t getNQubit() const { return _qubits.size(); }
  
  // Member functions about circuit construction
  void addQubit(size_t num);
  bool removeQubit(size_t q);
  void appendGate(string type, vector<size_t> bits);
  bool removeGate(size_t id);
  bool parseQASM(string qasm_file);
  
  void updateGateTime();
  // DFS functions
  void updateTopoOrder();

  // Member functions about circuit reporting
  void printGates();
  bool printGateInfo(size_t,bool);
  void printSummary() const;
  void printQubits();
  
private:
  void DFS(QCirGate*);
  bool _dirty;
  unsigned _globalDFScounter;
  size_t _gateId;
  size_t _qubitId;
  vector<QCirGate *> _qgate;
  vector<QCirQubit*> _qubits;
  stack<QCirGate *> _topoOrder;
};

#endif // QCIR_MGR_H