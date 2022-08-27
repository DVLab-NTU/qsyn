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
  QCirGate(size_t id, size_t phase) : _id(id), _phase(phase)
  {
    _qubits.clear();
    _time = 0;
    _DFSCounter=0;
  }
  ~QCirGate() {}

  // Basic access method
  virtual string getTypeStr() const = 0;
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

  const vector<BitInfo>& getQubits() const  { return _qubits; }

  void addQubit(size_t qubit, bool isTarget)
  {
    BitInfo temp = {._qubit = qubit, ._parent = NULL, ._child = NULL, ._isTarget = isTarget};
    _qubits.push_back(temp);
  }
  void setTypeStr(string type) {_type = type;}
  void setTime(size_t time) { _time = time; }
  void setParent(size_t qubit, QCirGate *p);
  void setChild(size_t qubit, QCirGate *c);
  
  //DFS
  bool isVisited(unsigned global) { return global == _DFSCounter; }
  void setVisited(unsigned global) { _DFSCounter = global; }
  void addDummyChild(QCirGate *c);
  // Printing functions
  void printGate() const;
  void reportGate() const;
  void reportFanin(int level) const;
  void reportFanout(int level) const;

private:
  
protected:
  size_t _id;
  string _type;
  size_t _nqubit;
  size_t _time;
  size_t _phase;
  unsigned _DFSCounter;
  vector<BitInfo> _qubits;
};

class HGate : public QCirGate
{ 
public:
  HGate(size_t id): QCirGate(id, 0) {}
  ~HGate(){};
  virtual string getTypeStr() const override { return "h"; } 
};

class CnRZGate : public QCirGate
{ 
public:
  CnRZGate(size_t id, size_t phase): QCirGate(id, phase) {}
  ~CnRZGate(){};
  virtual string getTypeStr() const { 
    string tmp = "";
    for(size_t i=0; i<_qubits.size()-1; i++)
      tmp+="c";
    tmp +="rz";
    return tmp; 
  }
};

class CnRXGate : public QCirGate
{ 
public:
  CnRXGate(size_t id, size_t phase): QCirGate(id, phase) {}
  ~CnRXGate(){};
};

class ZGate : public CnRZGate
{ 
public:
  ZGate(size_t id): CnRZGate(id, 0) {
    _type = "z";
  }
  ~ZGate();
  virtual string getTypeStr() const { return "z"; }
};

class SGate : public CnRZGate
{ 
public:
  SGate(size_t id): CnRZGate(id, 90) {
    _type = "s";
  }
  ~SGate();
  virtual string getTypeStr() const { return "s"; }
};

class TGate : public CnRZGate
{ 
public:
  TGate(size_t id): CnRZGate(id, 45) {}
  ~TGate();
  virtual string getTypeStr() const { return "t"; }
};

class TDGGate : public CnRZGate
{ 
public:
  TDGGate(size_t id): CnRZGate(id, 315) {}
  ~TDGGate();
  virtual string getTypeStr() const { return "td"; }
};

class PGate : public CnRZGate
{ 
public:
  PGate(size_t id, size_t phase): CnRZGate(id, phase) {}
  ~PGate();
  virtual string getTypeStr() const { return "p"; }
};

class CZGate : public CnRZGate
{ 
public:
  CZGate(size_t id): CnRZGate(id, 0) {}
  ~CZGate();
  virtual string getTypeStr() const { return "cz"; }
};

class XGate : public CnRXGate
{ 
public:
  XGate(size_t id): CnRXGate(id, 0) {}
  ~XGate();
  virtual string getTypeStr() const { return "x"; }
};

class SXGate : public CnRXGate
{ 
public:
  SXGate(size_t id): CnRXGate(id, 0) {}
  ~SXGate();
  virtual string getTypeStr() const { return "sx"; }
};

class CXGate : public CnRXGate
{ 
public:
  CXGate(size_t id): CnRXGate(id, 0) {}
  ~CXGate();
  virtual string getTypeStr() const { return "cx"; }
};

class CCXGate : public CnRXGate
{ 
public:
  CCXGate(size_t id): CnRXGate(id, 0) {}
  ~CCXGate();
  virtual string getTypeStr() const { return "ccx"; }
};
#endif // QCIR_GATE_H