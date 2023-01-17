/****************************************************************************
  FileName     [ qcirGate.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class qcirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_GATE_H
#define QCIR_GATE_H

#include <cstddef>  // for size_t, NULL
#include <string>   // for string

#include "phase.h"    // for Phase
#include "qtensor.h"  // for QTensor
#include "zxDef.h"    // for VertexType

class QCirGate;
class ZXGraph;

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
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;
    size_t getId() const { return _id; }
    size_t getTime() const { return _time; }
    Phase getPhase() const { return _rotatePhase; }
    const std::vector<BitInfo>& getQubits() const { return _qubits; }
    const BitInfo getQubit(size_t qubit) const;

    void setId(size_t id) { _id = id; }
    void setTime(size_t time) { _time = time; }
    void setTypeStr(std::string type) { _type = type; }
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
    std::string _type;
    size_t _time;
    size_t _nqubit;
    unsigned _DFSCounter;
    std::vector<BitInfo> _qubits;
    Phase _rotatePhase;

    using ZXVertexCombi = std::vector<ZXVertex*>;

    ZXGraph* mapSingleQubitGate(VertexType, Phase);
    void printSingleQubitGate(std::string, bool = false) const;
    void printMultipleQubitsGate(std::string, bool = false, bool = false) const;
    std::vector<ZXVertexCombi> makeCombi(ZXVertexCombi verVec, int k);
    void makeCombiUtil(std::vector<ZXVertexCombi>& comb, ZXVertexCombi& tmp, ZXVertexCombi verVec, int left, int k);
};

class HGate : public QCirGate {
public:
    HGate(size_t id) : QCirGate(id) { _type = "h"; }
    virtual ~HGate() {}
    virtual std::string getTypeStr() const { return "h"; }
    virtual GateType getType() const { return GateType::H; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::H_BOX, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::hbox(2); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("H", st); }
};

class CnPGate : public QCirGate {
public:
    CnPGate(size_t id) : QCirGate(id) { _type = "mcp"; }
    virtual ~CnPGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "cp"; }
    virtual GateType getType() const { return GateType::MCP; }

    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::pzgate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RZ", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class CRZGate : public CnPGate {
public:
    CRZGate(size_t id) : CnPGate(id) { _type = "crz"; }
    virtual ~CRZGate() {}
    virtual std::string getTypeStr() const { return "crz"; }
    virtual GateType getType() const { return GateType::CRZ; }
    virtual ZXGraph* getZXform();

    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rzgate(_rotatePhase), 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RZ", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class CnRXGate : public QCirGate {
public:
    CnRXGate(size_t id) : QCirGate(id) { _type = "mcrx"; }
    virtual ~CnRXGate(){};

    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "crx"; }
    virtual GateType getType() const { return GateType::MCRX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rxgate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RX", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class CnRYGate : public QCirGate {
public:
    CnRYGate(size_t id) : QCirGate(id) { _type = "mcry"; }
    virtual ~CnRYGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "cry"; }
    virtual GateType getType() const { return GateType::MCRY; }
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rygate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RY", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class ZGate : public CnPGate {
public:
    ZGate(size_t id) : CnPGate(id) { _type = "z"; }
    virtual ~ZGate() {}
    virtual std::string getTypeStr() const { return "z"; }
    virtual GateType getType() const { return GateType::Z; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::zgate(); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Z", st); }
};

class SGate : public CnPGate {
public:
    SGate(size_t id) : CnPGate(id) { _type = "s"; }
    virtual ~SGate() {}
    virtual std::string getTypeStr() const { return "s"; }
    virtual GateType getType() const { return GateType::S; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("S", st); }
};

class SDGGate : public CnPGate {
public:
    SDGGate(size_t id) : CnPGate(id) { _type = "sdg"; }
    virtual ~SDGGate() {}
    virtual std::string getTypeStr() const { return "sd"; }
    virtual GateType getType() const { return GateType::SDG; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(-1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(-1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Sdg", st); }
};

class TGate : public CnPGate {
public:
    TGate(size_t id) : CnPGate(id) { _type = "t"; }
    virtual ~TGate() {}
    virtual std::string getTypeStr() const { return "t"; }
    virtual GateType getType() const { return GateType::T; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1, 4)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(1, 4)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("T", st); }
};

class TDGGate : public CnPGate {
public:
    TDGGate(size_t id) : CnPGate(id) { _type = "tdg"; }
    virtual ~TDGGate() {}
    virtual std::string getTypeStr() const { return "td"; }
    virtual GateType getType() const { return GateType::TDG; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(-1, 4)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(-1, 4)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Tdg", st); }
};

class RZGate : public CnPGate {
public:
    RZGate(size_t id) : CnPGate(id) { _type = "rz"; }
    virtual ~RZGate() {}
    virtual std::string getTypeStr() const { return "rz"; }
    virtual GateType getType() const { return GateType::RZ; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(_rotatePhase)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rzgate(_rotatePhase); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RZ", st); }
};

class CZGate : public CnPGate {
public:
    CZGate(size_t id) : CnPGate(id) { _type = "cz"; }
    virtual ~CZGate() {}
    virtual std::string getTypeStr() const { return "cz"; }
    virtual GateType getType() const { return GateType::CZ; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::zgate(), 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

class CCZGate : public CnPGate {
public:
    CCZGate(size_t id) : CnPGate(id) { _type = "ccz"; }
    virtual ~CCZGate() {}
    virtual std::string getTypeStr() const { return "ccz"; }
    virtual GateType getType() const { return GateType::CCZ; }

    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::zgate(), 2); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

class XGate : public CnRXGate {
public:
    XGate(size_t id) : CnRXGate(id) { _type = "x"; }
    virtual ~XGate() {}
    virtual std::string getTypeStr() const { return "x"; }
    virtual GateType getType() const { return GateType::X; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::xgate(); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("X", st); }
};

class SXGate : public CnRXGate {
public:
    SXGate(size_t id) : CnRXGate(id) { _type = "sx"; }
    virtual ~SXGate() {}
    virtual std::string getTypeStr() const { return "sx"; }
    virtual GateType getType() const { return GateType::SX; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pxgate(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SX", st); }
};

class RXGate : public CnRXGate {
public:
    RXGate(size_t id) : CnRXGate(id) { _type = "rx"; }
    virtual ~RXGate() {}
    virtual std::string getTypeStr() const { return "rx"; }
    virtual GateType getType() const { return GateType::RX; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, _rotatePhase); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rxgate(_rotatePhase); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RX", st); }
};

class CXGate : public CnRXGate {
public:
    CXGate(size_t id) : CnRXGate(id) { _type = "cx"; }
    virtual ~CXGate() {}
    virtual std::string getTypeStr() const { return "cx"; }
    virtual GateType getType() const { return GateType::CX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::xgate(), 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};

class CCXGate : public CnRXGate {
public:
    CCXGate(size_t id) : CnRXGate(id) { _type = "ccx"; }
    virtual ~CCXGate() {}
    virtual std::string getTypeStr() const { return "ccx"; }
    virtual GateType getType() const { return GateType::CCX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::xgate(), 2); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};
class YGate : public CnRYGate {
public:
    YGate(size_t id) : CnRYGate(id) { _type = "y"; }
    virtual ~YGate() {}
    virtual std::string getTypeStr() const { return "y"; }
    virtual GateType getType() const { return GateType::Y; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::ygate(); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Y", st); }
};

class SYGate : public CnRYGate {
public:
    SYGate(size_t id) : CnRYGate(id) { _type = "sy"; }
    virtual ~SYGate() {}
    virtual std::string getTypeStr() const { return "sy"; }
    virtual GateType getType() const { return GateType::SY; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::pygate(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SY", st); }
};
#endif  // QCIR_GATE_H