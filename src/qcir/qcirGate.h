/****************************************************************************
  FileName     [ qcirGate.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define basic gate data structures ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_GATE_H
#define QCIR_GATE_H

#include <iostream>
#include <string>
#include <vector>

#include "phase.h"
#include "qtensor.h"
#include "zxGraph.h"

using namespace std;

class QCirGate;
struct BitInfo;
//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
struct BitInfo {
    size_t _qubit;
    QCirGate* _parent;
    QCirGate* _child;
    bool _isTarget;
};

enum class GateType {
    H,
    RZ,
    Z,
    S,
    SDG,
    T,
    TDG,
    RX,
    X,
    SX,
    RY,
    Y,
    SY,
    MCP,
    CRZ,
    CZ,
    CCZ,
    MCRX,
    CX,
    CCX,
    MCRY,
    ERRORTYPE  // Never use this
};

class QCirGate {
public:
    QCirGate(size_t id) : _id(id) {
        _qubits.clear();
        _time = 0;
        _DFSCounter = 0;
    }
    virtual ~QCirGate() {}

    // Basic access method
    virtual string getTypeStr() const = 0;
    virtual GateType getType() const = 0;
    size_t getId() const { return _id; }
    size_t getTime() const { return _time; }
    Phase getPhase() const { return _rotatePhase; }
    const vector<BitInfo>& getQubits() const { return _qubits; }
    const BitInfo getQubit(size_t qubit) const;

    void setId(size_t id) { _id = id; }
    void setTime(size_t time) { _time = time; }
    void setTypeStr(string type) { _type = type; }
    void setChild(size_t qubit, QCirGate* c);
    void setParent(size_t qubit, QCirGate* p);

    void addQubit(size_t qubit, bool isTarget);

    // DFS
    bool isVisited(unsigned global) { return global == _DFSCounter; }
    void setVisited(unsigned global) { _DFSCounter = global; }
    void addDummyChild(QCirGate* c);
    // Printing functions
    void printGate() const;

    virtual ZXGraph* getZXform() { return NULL; }
    virtual QTensor<double> getTSform() const = 0;
    virtual void setRotatePhase(Phase p) {}
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

    ZXGraph* mapSingleQubitGate(VertexType, Phase);
    void printSingleQubitGate(string, bool = false) const;
    void printMultipleQubitsGate(string, bool = false, bool = false) const;
    vector<vector<ZXVertex*> > makeCombi(vector<ZXVertex*> verVec, int k);
    void makeCombiUtil(vector<vector<ZXVertex*> >& comb, vector<ZXVertex*>& tmp, vector<ZXVertex*> verVec, int left, int k);
};

class HGate : public QCirGate {
public:
    HGate(size_t id) : QCirGate(id) { _type = "h"; }
    virtual ~HGate() {}
    virtual string getTypeStr() const { return "h"; }
    virtual GateType getType() const { return GateType::H; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::H_BOX, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::hbox(2); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("H", st); }
};

class CnPGate : public QCirGate {
public:
    CnPGate(size_t id) : QCirGate(id) { _type = "mcp"; }
    virtual ~CnPGate(){};
    virtual string getTypeStr() const { return _qubits.size() > 2 ? _type : "cp"; }
    virtual GateType getType() const { return GateType::MCP; }

    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::cnz(_qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RZ", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class CrzGate : public CnPGate {
public:
    CrzGate(size_t id) : CnPGate(id) { _type = "crz"; }
    virtual ~CrzGate() {}
    virtual string getTypeStr() const { return "crz"; }
    virtual GateType getType() const { return GateType::CRZ; }
    virtual ZXGraph* getZXform();
    // todo: implentment the correct TSform
    virtual QTensor<double> getTSform() const { return QTensor<double>::cnz(_qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RZ", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class CnRXGate : public QCirGate {
public:
    CnRXGate(size_t id) : QCirGate(id) { _type = "mcrx"; }
    virtual ~CnRXGate(){};

    virtual string getTypeStr() const { return _qubits.size() > 2 ? _type : "crx"; }
    virtual GateType getType() const { return GateType::MCRX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::cnx(_qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RX", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class CnRYGate : public QCirGate {
public:
    CnRYGate(size_t id) : QCirGate(id) { _type = "mcry"; }
    virtual ~CnRYGate(){};
    virtual string getTypeStr() const { return _qubits.size() > 2 ? _type : "cry"; }
    virtual GateType getType() const { return GateType::MCRY; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RY", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class ZGate : public CnPGate {
public:
    ZGate(size_t id) : CnPGate(id) { _type = "z"; }
    virtual ~ZGate() {}
    virtual string getTypeStr() const { return "z"; }
    virtual GateType getType() const { return GateType::Z; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rz(Phase(1)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Z", st); }
};

class SGate : public CnPGate {
public:
    SGate(size_t id) : CnPGate(id) { _type = "s"; }
    virtual ~SGate() {}
    virtual string getTypeStr() const { return "s"; }
    virtual GateType getType() const { return GateType::S; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rz(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("S", st); }
};

class SDGGate : public CnPGate {
public:
    SDGGate(size_t id) : CnPGate(id) { _type = "sdg"; }
    virtual ~SDGGate() {}
    virtual string getTypeStr() const { return "sd"; }
    virtual GateType getType() const { return GateType::SDG; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(-1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rz(Phase(-1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Sdg", st); }
};

class TGate : public CnPGate {
public:
    TGate(size_t id) : CnPGate(id) { _type = "t"; }
    virtual ~TGate() {}
    virtual string getTypeStr() const { return "t"; }
    virtual GateType getType() const { return GateType::T; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1, 4)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rz(Phase(1, 4)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("T", st); }
};

class TDGGate : public CnPGate {
public:
    TDGGate(size_t id) : CnPGate(id) { _type = "tdg"; }
    virtual ~TDGGate() {}
    virtual string getTypeStr() const { return "td"; }
    virtual GateType getType() const { return GateType::TDG; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(-1, 4)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rz(Phase(-1, 4)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Tdg", st); }
};

class RZGate : public CnPGate {
public:
    RZGate(size_t id) : CnPGate(id) { _type = "rz"; }
    virtual ~RZGate() {}
    virtual string getTypeStr() const { return "rz"; }
    virtual GateType getType() const { return GateType::RZ; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(_rotatePhase)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rz(_rotatePhase); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RZ", st); }
};

class CZGate : public CnPGate {
public:
    CZGate(size_t id) : CnPGate(id) { _type = "cz"; }
    virtual ~CZGate() {}
    virtual string getTypeStr() const { return "cz"; }
    virtual GateType getType() const { return GateType::CZ; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::cnz(1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

class CCZGate : public CnPGate {
public:
    CCZGate(size_t id) : CnPGate(id) { _type = "ccz"; }
    virtual ~CCZGate() {}
    virtual string getTypeStr() const { return "ccz"; }
    virtual GateType getType() const { return GateType::CCZ; }

    virtual QTensor<double> getTSform() const { return QTensor<double>::cnz(2); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

class XGate : public CnRXGate {
public:
    XGate(size_t id) : CnRXGate(id) { _type = "x"; }
    virtual ~XGate() {}
    virtual string getTypeStr() const { return "x"; }
    virtual GateType getType() const { return GateType::X; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rx(Phase(1)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("X", st); }
};

class SXGate : public CnRXGate {
public:
    SXGate(size_t id) : CnRXGate(id) { _type = "sx"; }
    virtual ~SXGate() {}
    virtual string getTypeStr() const { return "sx"; }
    virtual GateType getType() const { return GateType::SX; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rx(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SX", st); }
};

class RXGate : public CnRXGate {
public:
    RXGate(size_t id) : CnRXGate(id) { _type = "rx"; }
    virtual ~RXGate() {}
    virtual string getTypeStr() const { return "rx"; }
    virtual GateType getType() const { return GateType::RX; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, _rotatePhase); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rx(_rotatePhase); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RX", st); }
};

class CXGate : public CnRXGate {
public:
    CXGate(size_t id) : CnRXGate(id) { _type = "cx"; }
    virtual ~CXGate() {}
    virtual string getTypeStr() const { return "cx"; }
    virtual GateType getType() const { return GateType::CX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::cnx(1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};

class CCXGate : public CnRXGate {
public:
    CCXGate(size_t id) : CnRXGate(id) { _type = "ccx"; }
    virtual ~CCXGate() {}
    virtual string getTypeStr() const { return "ccx"; }
    virtual GateType getType() const { return GateType::CCX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::cnx(2); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};
class YGate : public CnRYGate {
public:
    YGate(size_t id) : CnRYGate(id) { _type = "y"; }
    virtual ~YGate() {}
    virtual string getTypeStr() const { return "y"; }
    virtual GateType getType() const { return GateType::Y; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::ry(Phase(1)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Y", st); }
};

class SYGate : public CnRYGate {
public:
    SYGate(size_t id) : CnRYGate(id) { _type = "sy"; }
    virtual ~SYGate() {}
    virtual string getTypeStr() const { return "sy"; }
    virtual GateType getType() const { return GateType::SY; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::ry(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SY", st); }
};
#endif  // QCIR_GATE_H