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
  size_t getId() const                              { return _id; }
  size_t getTime() const                            { return _time; }
  Phase getPhase() const                            { return _rotatePhase; }
  const vector<BitInfo>& getQubits() const          { return _qubits; }
  const BitInfo getQubit(size_t qubit) const;

  void setId(size_t id)                             { _id = id; }
  void setTime(size_t time)                         { _time = time; }
  void setTypeStr(string type)                      { _type = type; }
  void setChild(size_t qubit, QCirGate *c);
  void setParent(size_t qubit, QCirGate *p);
  
  void addQubit(size_t qubit, bool isTarget);
  
  
  //DFS
  bool isVisited(unsigned global)                   { return global == _DFSCounter; }
  void setVisited(unsigned global)                  { _DFSCounter = global; }
  void addDummyChild(QCirGate *c);
  // Printing functions
  void printGate() const;
  
  virtual ZXGraph*  getZXform()                     { return NULL; }
  virtual QTensor<double> getTSform() const = 0;
  virtual void setRotatePhase(Phase p)              { }
  virtual void printGateInfo(bool) const = 0;
  
private:
  
protected:
  size_t _id;
  string _type;
  size_t _time;
  size_t _nqubit;
  unsigned _DFSCounter;
  vector<BitInfo> _qubits;
  Phase _rotatePhase;

  ZXGraph *mapSingleQubitGate(VertexType, Phase);
  void printSingleQubitGate(string, bool=false) const;
  void printMultipleQubitsGate(string, bool=false, bool=false) const;
  vector<vector<ZXVertex* > > makeCombi(vector<ZXVertex* > verVec, int k);
  void makeCombiUtil(vector<vector<ZXVertex* > >& comb, vector<ZXVertex* >& tmp, vector<ZXVertex* > verVec, int left, int k);

};

class HGate : public QCirGate
{ 
public:
  HGate(size_t id): QCirGate(id)                  { _type = "h"; }
  ~HGate(){};
  virtual string getTypeStr() const               { return "h"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::H_BOX, Phase(1)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::hbox(2); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("H", st); } 
};

class CnRZGate : public QCirGate
{ 
public:
  CnRZGate(size_t id): QCirGate(id)               { _type = "cnrz"; }
  ~CnRZGate(){};
  virtual string getTypeStr() const { string tmp = "";
    for(size_t i = 0; i < _qubits.size() - 1; i++) tmp += "c";
    tmp += "rz"; return tmp; 
  }

  virtual ZXGraph* getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::cnz(_qubits.size()-1); }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate(" RZ", true, st); } 
  
  virtual void setRotatePhase(Phase p)            { _rotatePhase = p; }
};

class CnRXGate : public QCirGate
{ 
public:
  CnRXGate(size_t id): QCirGate(id)               { _type = "cnrx"; }
  ~CnRXGate(){};
  
  virtual string getTypeStr() const { string tmp = "";
    for(size_t i = 0; i < _qubits.size() - 1; i++) tmp += "c";
    tmp += "rx"; return tmp; 
  }
  virtual ZXGraph*  getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::cnx(_qubits.size()-1); }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate(" RX", true, st); } 
  
  virtual void setRotatePhase(Phase p)            { _rotatePhase = p; }
};

class CnRYGate : public QCirGate
{ 
public:
  CnRYGate(size_t id): QCirGate(id)               { _type = "cnry"; }
  ~CnRYGate(){};
  virtual string getTypeStr() const { string tmp = "";
    for(size_t i = 0; i < _qubits.size() - 1; i++) tmp+="c";
    tmp += "ry"; return tmp; 
  }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate(" RY", true, st); } 

  virtual void setRotatePhase(Phase p)            { _rotatePhase = p; }
};

class ZGate : public CnRZGate
{ 
public:
  ZGate(size_t id): CnRZGate(id)                  { _type = "z"; }
  ~ZGate();
  virtual string getTypeStr() const               { return "z"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::Z, Phase(1)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rz(Phase(1)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("Z", st); }
};

class SGate : public CnRZGate
{ 
public:
  SGate(size_t id): CnRZGate(id)                  { _type = "s"; }
  ~SGate();
  virtual string getTypeStr() const               { return "s"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::Z, Phase(1, 2)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rz(Phase(1,2)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("S", st); } 
};

class SDGGate : public CnRZGate
{ 
public:
  SDGGate(size_t id): CnRZGate(id)                { _type = "sdg"; }
  ~SDGGate();
  virtual string getTypeStr() const               { return "sd"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::Z, Phase(-1, 2)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rz(Phase(-1, 2)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("Sdg", st); } 
};

class TGate : public CnRZGate
{ 
public:
  TGate(size_t id): CnRZGate(id)                  { _type = "t"; }
  ~TGate();
  virtual string getTypeStr() const               { return "t"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::Z, Phase(1, 4)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rz(Phase(1, 4)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("T", st); } 
};

class TDGGate : public CnRZGate
{ 
public:
  TDGGate(size_t id): CnRZGate(id)                { _type = "tdg"; }
  ~TDGGate();
  virtual string getTypeStr() const               { return "td"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::Z, Phase(-1, 4)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rz(Phase(-1, 4)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("Tdg", st); } 
};

class RZGate : public CnRZGate
{ 
public:
  RZGate(size_t id): CnRZGate(id)                 { _type = "rz"; }
  ~RZGate();
  virtual string getTypeStr() const               { return "rz"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::Z, Phase(_rotatePhase)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rz(_rotatePhase); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("RZ", st); } 
};

class CZGate : public CnRZGate
{ 
public:
  CZGate(size_t id): CnRZGate(id)                 { _type = "cz"; }
  ~CZGate();
  virtual string getTypeStr() const               { return "cz"; }
  virtual ZXGraph* getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::cnz(1); }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate("Z", false, st); } 
};

class CCZGate : public CnRZGate
{ 
public:
  CCZGate(size_t id): CnRZGate(id)                { _type = "ccz"; }
  ~CCZGate();
  virtual string getTypeStr() const               { return "ccz"; }

  virtual QTensor<double> getTSform() const       { return QTensor<double>::cnz(2); }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate("Z", false, st); } 
};

class XGate : public CnRXGate
{ 
public:
  XGate(size_t id): CnRXGate(id)                  { _type = "x"; }
  ~XGate();
  virtual string getTypeStr() const               { return "x"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::X, Phase(1)); }
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rx(Phase(1)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("X", st); } 
};

class SXGate : public CnRXGate
{ 
public:
  SXGate(size_t id): CnRXGate(id)                 { _type = "sx"; }
  ~SXGate();
  virtual string getTypeStr() const               { return "sx"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::X, Phase(1, 2));}
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rx(Phase(1, 2)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("SX", st); } 
};

class RXGate : public CnRXGate
{ 
public:
  RXGate(size_t id): CnRXGate(id)                 { _type = "rx"; }
  ~RXGate();
  virtual string getTypeStr() const               { return "rx"; }
  virtual ZXGraph* getZXform()                    { return mapSingleQubitGate(VertexType::X, _rotatePhase);}
  virtual QTensor<double> getTSform() const       { return QTensor<double>::rx(_rotatePhase); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("RX", st); } 
};

class CXGate : public CnRXGate
{ 
public:
  CXGate(size_t id): CnRXGate(id)                 { _type = "cx"; }
  ~CXGate();
  virtual string getTypeStr() const               { return "cx"; }
  virtual ZXGraph* getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::cnx(1); }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate("X", false, st); } 
};

class CCXGate : public CnRXGate
{ 
public:
  CCXGate(size_t id): CnRXGate(id)                { _type = "ccx"; }
  ~CCXGate();
  virtual string getTypeStr() const               { return "ccx"; }
  virtual ZXGraph* getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::cnx(2); }
  virtual void printGateInfo(bool st) const       { printMultipleQubitsGate("X", false, st); } 
};
class YGate : public CnRYGate
{ 
public:
  YGate(size_t id): CnRYGate(id)                  { _type = "y"; }
  ~YGate();
  virtual string getTypeStr() const               { return "y"; }
  virtual ZXGraph* getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::ry(Phase(1)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("Y", st); } 
};

class SYGate : public CnRYGate
{ 
public:
  SYGate(size_t id): CnRYGate(id)                 { _type = "sy"; }
  ~SYGate();
  virtual string getTypeStr() const               { return "sy"; }
  virtual ZXGraph* getZXform();
  virtual QTensor<double> getTSform() const       { return QTensor<double>::ry(Phase(1, 2)); }
  virtual void printGateInfo(bool st) const       { printSingleQubitGate("SY", st); } 
};
#endif // QCIR_GATE_H