/****************************************************************************
  FileName     [ qcirGate.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define basic gate data structures ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_GATE_H
#define QCIR_GATE_H

#include <string>
#include <vector>
#include <iostream>

using namespace std;

class QCirGate;
struct BitInfo;
//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
struct BitInfo
{
  size_t _qubit;
  QCirGate *_parent;
  QCirGate *_child;
  bool _isTarget;
};

class QCirGate
{
public:
  QCirGate(size_t id, string type, size_t nqubit) : _id(id), _type(type), _nqubit(nqubit)
  {
    _qubits.clear();
    _time = 0;
  }
  ~QCirGate() {}

  // Basic access method
  string getTypeStr() const { return _type; }
  size_t getId() const { return _id; }
  size_t getTime() const { return _time; }
  BitInfo getQubit(size_t qubit) const {
    for (size_t i = 0; i < _qubits.size(); i++)
    {
      if(_qubits[i]._qubit==qubit) return _qubits[i];
    }
    cerr << "Not Found" << endl;
    return _qubits[0];
  }

  void addQubit(size_t qubit, bool isTarget)
  {
    BitInfo temp = {._qubit = qubit, ._parent = NULL, ._child = NULL, ._isTarget = isTarget};
    _qubits.push_back(temp);
  }
  void setTime(size_t time) { _time = time; }
  void addParent(size_t qubit, QCirGate *p);
  void addChild(size_t qubit, QCirGate *c);
  // Printing functions
  void printGate() const;
  void reportGate() const;
  void reportFanin(int level) const;
  void reportFanout(int level) const;

private:
  size_t _id;
  string _type;
  size_t _nqubit;
  size_t _time;
  vector<BitInfo> _qubits;

protected:
};

#endif // QCIR_GATE_H