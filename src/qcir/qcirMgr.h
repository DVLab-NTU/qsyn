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
    _qgate.clear();
    _qubits.clear();
  }
  ~QCirMgr() {}

  // Access functions
  // return '0' if "gid" corresponds to an undefined gate.
  // QCirGate *getGate(unsigned gid) const { return 0; }
  size_t getNQubit() const { return _qubits.size(); }
  
  // Member functions about circuit construction
  void addAncilla(size_t num);
  void removeAncilla(size_t q);
  void appendGate(string type, vector<size_t> bits);
  void removeGate(size_t id);
  bool parseQASM(string qasm_file);
  
  void getDFS();

  // Member functions about circuit reporting
  void printGates() const;
  void printSummary() const;
  void printQubits() const;
  
private:
  void DFS();
  size_t _gateId;
  vector<QCirGate *> _qgate;
  vector<QCirQubit*> _qubits;
};

#endif // QCIR_MGR_H