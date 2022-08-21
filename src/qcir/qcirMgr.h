/****************************************************************************
  FileName     [ cirMgr.h ]
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
#include "qcirDef.h"

extern QCirMgr *qCirMgr;
using namespace std;

class QCirMgr
{
public:
  QCirMgr() {}
  ~QCirMgr() {}

  // Access functions
  // return '0' if "gid" corresponds to an undefined gate.
  QCirGate *getGate(unsigned gid) const { return 0; }

  // Member functions about circuit construction
  bool parseQASM(string qasm_file);
  void addGate(QCirGate *);
  // Member functions about circuit reporting
  void printGates() const;
  void printSummary() const;
  void printNetlist() const;
  void printPIs() const;
  void printPOs() const;
  void printFloatGates() const;
  void printFECPairs() const;
  void writeAag(ostream &) const;
  void writeGate(ostream &, QCirGate *) const;

private:
  vector<QCirGate *> _qgate;
};

#endif // QCIR_MGR_H