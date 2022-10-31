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
#include "phase.h"
#include "qtensor.h"
#include "zxGraph.h"

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
  QCirGate(size_t id) : _id(id)
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
  virtual void printGateInfo(bool) const=0;
  virtual ZXGraph*  getZXform(size_t &baseId){ return NULL; };
  virtual QTensor<double>  getTSform() = 0;
  virtual void setRotatePhase(Phase p){};
private:
  
protected:
  size_t _id;
  string _type;
  size_t _nqubit;
  size_t _time;
  unsigned _DFSCounter;
  vector<BitInfo> _qubits;
  Phase _rotatePhase;
};

class HGate : public QCirGate
{ 
public:
  HGate(size_t id): QCirGate(id) {}
  ~HGate(){};
  virtual string getTypeStr() const override { return "h"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::hbox(2); }
  virtual void printGateInfo(bool) const; 
  virtual ZXGraph*  getZXform(size_t &baseId);
};

class CnRZGate : public QCirGate
{ 
public:
  CnRZGate(size_t id): QCirGate(id) {}
  ~CnRZGate(){};
  virtual string getTypeStr() const { 
    string tmp = "";
    for(size_t i=0; i<_qubits.size()-1; i++)
      tmp+="c";
    tmp +="rz";
    return tmp; 
  }
  virtual ZXGraph*  getZXform(size_t &baseId){ return NULL; };
  virtual void printGateInfo(bool) const;
};

class CnRXGate : public QCirGate
{ 
public:
  CnRXGate(size_t id): QCirGate(id) {}
  ~CnRXGate(){};
  virtual ZXGraph*  getZXform(size_t &baseId){ return NULL; };
  virtual void printGateInfo(bool) const;
};

class CnRYGate : public QCirGate
{ 
public:
  CnRYGate(size_t id): QCirGate(id) {}
  ~CnRYGate(){};
  virtual string getTypeStr() const { 
    string tmp = "";
    for(size_t i=0; i<_qubits.size()-1; i++)
      tmp+="c";
    tmp +="ry";
    return tmp; 
  }
  virtual void printGateInfo(bool) const;
};

class ZGate : public CnRZGate
{ 
public:
  ZGate(size_t id): CnRZGate(id) {
    _type = "z";
  }
  ~ZGate();
  virtual string getTypeStr() const { return "z"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rz(Phase(1)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class SGate : public CnRZGate
{ 
public:
  SGate(size_t id): CnRZGate(id) {
    _type = "s";
  }
  ~SGate();
  virtual string getTypeStr() const { return "s"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rz(Phase(1,2)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class SDGGate : public CnRZGate
{ 
public:
  SDGGate(size_t id): CnRZGate(id) {
    _type = "sdg";
  }
  ~SDGGate();
  virtual string getTypeStr() const { return "sd"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rz(Phase(-1,2)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class TGate : public CnRZGate
{ 
public:
  TGate(size_t id): CnRZGate(id) {}
  ~TGate();
  virtual string getTypeStr() const { return "t"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rz(Phase(1,4)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class TDGGate : public CnRZGate
{ 
public:
  TDGGate(size_t id): CnRZGate(id) {}
  ~TDGGate();
  virtual string getTypeStr() const { return "td"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rz(Phase(-1,4)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class RZGate : public CnRZGate
{ 
public:
  RZGate(size_t id): CnRZGate(id) {}
  ~RZGate();
  virtual string getTypeStr() const { return "rz"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rz(_rotatePhase); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
  virtual void setRotatePhase(Phase p){ _rotatePhase = p; }
};

class CZGate : public CnRZGate
{ 
public:
  CZGate(size_t id): CnRZGate(id) {}
  ~CZGate();
  virtual string getTypeStr() const { return "cz"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::cnz(1); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class CCZGate : public CnRZGate
{ 
public:
  CCZGate(size_t id): CnRZGate(id) {}
  ~CCZGate();
  virtual string getTypeStr() const { return "ccz"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::cnz(2); }
  virtual void printGateInfo(bool) const;
};

class XGate : public CnRXGate
{ 
public:
  XGate(size_t id): CnRXGate(id) {}
  ~XGate();
  virtual string getTypeStr() const { return "x"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rx(Phase(1)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class SXGate : public CnRXGate
{ 
public:
  SXGate(size_t id): CnRXGate(id) {}
  ~SXGate();
  virtual string getTypeStr() const { return "sx"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rx(Phase(1,2)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class RXGate : public CnRXGate
{ 
public:
  RXGate(size_t id): CnRXGate(id) {}
  ~RXGate();
  virtual string getTypeStr() const { return "rx"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::rx(_rotatePhase); }
  virtual void printGateInfo(bool) const;
  virtual void setRotatePhase(Phase p){ _rotatePhase = p; }
};

class CXGate : public CnRXGate
{ 
public:
  CXGate(size_t id): CnRXGate(id) {}
  ~CXGate();
  virtual string getTypeStr() const { return "cx"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::cnx(1); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class CCXGate : public CnRXGate
{ 
public:
  CCXGate(size_t id): CnRXGate(id) {}
  ~CCXGate();
  virtual string getTypeStr() const { return "ccx"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::cnx(2); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};
class YGate : public CnRYGate
{ 
public:
  YGate(size_t id): CnRYGate(id) {}
  ~YGate();
  virtual string getTypeStr() const { return "y"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::ry(Phase(1)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};

class SYGate : public CnRYGate
{ 
public:
  SYGate(size_t id): CnRYGate(id) {}
  ~SYGate();
  virtual string getTypeStr() const { return "sy"; }
  virtual QTensor<double>  getTSform() { return QTensor<double>::ry(Phase(1, 2)); }
  virtual ZXGraph*  getZXform(size_t &baseId);
  virtual void printGateInfo(bool) const;
};
#endif // QCIR_GATE_H