/****************************************************************************
  FileName     [ qcirGate.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class qcirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_GATE_H
#define QCIR_GATE_H

#include <cstddef>    // for size_t, NULL
#include <string>     // for string

#include "phase.h"    // for Phase
#include "qtensor.h"  // for QTensor
#include "zxDef.h"    // for VertexType

class QCirGate;
class ZXGraph;

// ┌────────────────────────────────────────────────────────────────────────┐
// │                                                                        │
// │                        Hierarchy of QCirGates                          │
// │                                                                        │
// │                               QCirGate                                 │
// │         ┌──────────────────┬─────────┴─────────┬──────────────────┐    │
// │       Z-axis             X-axis              Y-axis               H    │
// │    ┌────┴────┐        ┌────┴────┐         ┌────┴────┐                  │
// │   MCP      MCRZ      MCPX     MCRX       MCPY     MCRY                 │
// │  ╌╌╌╌╌╌   ╌╌╌╌╌╌    ╌╌╌╌╌╌   ╌╌╌╌╌╌     ╌╌╌╌╌╌   ╌╌╌╌╌╌                │
// │  (MCZ n)            (MCX n)                                            │
// │  CCZ (2)            CCX (2)             CCY (2)                        │
// │  CZ  (1)            CX  (1)             CY  (1)                        │
// │  Z                  X                   Y                              │
// │  S, SDG             SX                  SY                             │
// │  T, TDG             (SWAP)                                             │
// │  ╌╌╌╌╌╌   ╌╌╌╌╌╌    ╌╌╌╌╌╌   ╌╌╌╌╌╌     ╌╌╌╌╌╌   ╌╌╌╌╌╌                │
// │  **********  Will be merged into MCP/R X/Y/Z  **********               │
// │  (CP,P)  (CRZ,RZ)  (CPX,PX) (CRX,RX)   (CPY,PY) (CRY,RY)               │
// └────────────────────────────────────────────────────────────────────────┘

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
    P,
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
    MCPX,
    CX,
    CCX,
    MCRY,
    MCPY,
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

/**
 * @brief Virtual Class: Gates rotating on Z-axis
 *
 */
class ZAxisGate : public QCirGate {
public:
    ZAxisGate(size_t id) : QCirGate(id) {}
    virtual ~ZAxisGate(){};
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;

    virtual ZXGraph* getZXform() = 0;
    virtual QTensor<double> getTSform() const = 0;
    virtual void printGateInfo(bool st) const = 0;
};

/**
 * @brief Virtual Class: Gates rotating on X-axis
 *
 */
class XAxisGate : public QCirGate {
public:
    XAxisGate(size_t id) : QCirGate(id) {}
    virtual ~XAxisGate(){};
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;

    virtual ZXGraph* getZXform() = 0;
    virtual QTensor<double> getTSform() const = 0;
    virtual void printGateInfo(bool st) const = 0;
};

/**
 * @brief Virtual Class: Gates rotating on Y-axis
 *
 */
class YAxisGate : public QCirGate {
public:
    YAxisGate(size_t id) : QCirGate(id) {}
    virtual ~YAxisGate(){};
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;

    virtual ZXGraph* getZXform() = 0;
    virtual QTensor<double> getTSform() const = 0;
    virtual void printGateInfo(bool st) const = 0;
};

//----------------------------------------------------------------------
//    MCPZXY, MCRZXY
//----------------------------------------------------------------------

class MCPGate : public ZAxisGate {
public:
    MCPGate(size_t id) : ZAxisGate(id) { _type = "mcp"; }
    virtual ~MCPGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : _qubits.size() == 2 ? "cp"
                                                                                                     : "p"; }
    virtual GateType getType() const { return GateType::MCP; }

    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::pzgate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("P", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

// FIXME - No ZX form
class MCRZGate : public ZAxisGate {
public:
    MCRZGate(size_t id) : ZAxisGate(id) { _type = "mcrz"; }
    virtual ~MCRZGate(){};

    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "crz"; }
    virtual GateType getType() const { return GateType::MCRX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rzgate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RZ", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class MCPXGate : public XAxisGate {
public:
    MCPXGate(size_t id) : XAxisGate(id) { _type = "mcpx"; }
    virtual ~MCPXGate(){};

    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : _qubits.size() == 2 ? "cpx"
                                                                                                     : "px"; }
    virtual GateType getType() const { return GateType::MCPX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::pxgate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" PX", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

// FIXME - No ZX form
class MCRXGate : public XAxisGate {
public:
    MCRXGate(size_t id) : XAxisGate(id) { _type = "mcrx"; }
    virtual ~MCRXGate(){};

    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "crx"; }
    virtual GateType getType() const { return GateType::MCRX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rxgate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RX", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

// TODO - No ZX form
class MCPYGate : public YAxisGate {
public:
    MCPYGate(size_t id) : YAxisGate(id) { _type = "mcpy"; }
    virtual ~MCPYGate(){};

    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "cpy"; }
    virtual GateType getType() const { return GateType::MCPX; }
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::pygate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" PY", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

// TODO - No ZX form
class MCRYGate : public YAxisGate {
public:
    MCRYGate(size_t id) : YAxisGate(id) { _type = "mcry"; }
    virtual ~MCRYGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? _type : "cry"; }
    virtual GateType getType() const { return GateType::MCRY; }
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rygate(_rotatePhase), _qubits.size() - 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RY", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

//----------------------------------------------------------------------
//    Children class of MCP
//----------------------------------------------------------------------
// NOTE - CP and P gates are merged into MCPGate with number of control = 1 or 0

class CCZGate : public MCPGate {
public:
    CCZGate(size_t id) : MCPGate(id) { _type = "ccz"; }
    virtual ~CCZGate() {}
    virtual std::string getTypeStr() const { return "ccz"; }
    virtual GateType getType() const { return GateType::CCZ; }
    virtual ZXGraph* getZXform() { return MCPGate::getZXform(); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::zgate(), 2); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

class CZGate : public MCPGate {
public:
    CZGate(size_t id) : MCPGate(id) { _type = "cz"; }
    virtual ~CZGate() {}
    virtual std::string getTypeStr() const { return "cz"; }
    virtual GateType getType() const { return GateType::CZ; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::zgate(), 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

// REVIEW - Check if it can be merged into MCP
//  class PGate : public MCPGate {
//  public:
//      PGate(size_t id) : MCPGate(id) { _type = "p"; }
//      virtual ~PGate() {}
//      virtual std::string getTypeStr() const { return "p"; }
//      virtual GateType getType() const { return GateType::P; }
//      virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(_rotatePhase)); }
//      virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(_rotatePhase); }
//      virtual void printGateInfo(bool st) const { printSingleQubitGate("P", st); }
//  };

class ZGate : public MCPGate {
public:
    ZGate(size_t id) : MCPGate(id) { _type = "z"; }
    virtual ~ZGate() {}
    virtual std::string getTypeStr() const { return "z"; }
    virtual GateType getType() const { return GateType::Z; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::zgate(); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Z", st); }
};

class SGate : public MCPGate {
public:
    SGate(size_t id) : MCPGate(id) { _type = "s"; }
    virtual ~SGate() {}
    virtual std::string getTypeStr() const { return "s"; }
    virtual GateType getType() const { return GateType::S; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("S", st); }
};

class SDGGate : public MCPGate {
public:
    SDGGate(size_t id) : MCPGate(id) { _type = "sdg"; }
    virtual ~SDGGate() {}
    virtual std::string getTypeStr() const { return "sd"; }
    virtual GateType getType() const { return GateType::SDG; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(-1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(-1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Sdg", st); }
};

class TGate : public MCPGate {
public:
    TGate(size_t id) : MCPGate(id) { _type = "t"; }
    virtual ~TGate() {}
    virtual std::string getTypeStr() const { return "t"; }
    virtual GateType getType() const { return GateType::T; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(1, 4)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(1, 4)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("T", st); }
};

class TDGGate : public MCPGate {
public:
    TDGGate(size_t id) : MCPGate(id) { _type = "tdg"; }
    virtual ~TDGGate() {}
    virtual std::string getTypeStr() const { return "td"; }
    virtual GateType getType() const { return GateType::TDG; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(-1, 4)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pzgate(Phase(-1, 4)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Tdg", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRZ
//----------------------------------------------------------------------

// REVIEW - Check if it can be merged into MCRZ after the implementation of MCRZ ZX-form
class CRZGate : public MCRZGate {
public:
    CRZGate(size_t id) : MCRZGate(id) { _type = "crz"; }
    virtual ~CRZGate() {}
    virtual std::string getTypeStr() const { return "crz"; }
    virtual GateType getType() const { return GateType::CRZ; }
    virtual ZXGraph* getZXform();

    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::rzgate(_rotatePhase), 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(" RZ", true, st); }

    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

// REVIEW - Check if it can be merged into MCRZ after the implementation of MCRZ ZX-form
class RZGate : public MCRZGate {
public:
    RZGate(size_t id) : MCRZGate(id) { _type = "rz"; }
    virtual ~RZGate() {}
    virtual std::string getTypeStr() const { return "rz"; }
    virtual GateType getType() const { return GateType::RZ; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::Z, Phase(_rotatePhase)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rzgate(_rotatePhase); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RZ", st); }
};

//----------------------------------------------------------------------
//    Children class of MCPX
//----------------------------------------------------------------------
// NOTE - CPX and PX gates are merged into MCPXGate with number of control = 1 or 0

class CCXGate : public MCPXGate {
public:
    CCXGate(size_t id) : MCPXGate(id) { _type = "ccx"; }
    virtual ~CCXGate() {}
    virtual std::string getTypeStr() const { return "ccx"; }
    virtual GateType getType() const { return GateType::CCX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::xgate(), 2); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};

class CXGate : public MCPXGate {
public:
    CXGate(size_t id) : MCPXGate(id) { _type = "cx"; }
    virtual ~CXGate() {}
    virtual std::string getTypeStr() const { return "cx"; }
    virtual GateType getType() const { return GateType::CX; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::control(QTensor<double>::xgate(), 1); }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};

// REVIEW - Check if it can be merged into MCPX
//  class PXGate : public MCPXGate {
//  public:
//      PXGate(size_t id) : MCPXGate(id) { _type = "px"; }
//      virtual ~PXGate() {}
//      virtual std::string getTypeStr() const { return "px"; }
//      virtual GateType getType() const { return GateType::PX; }
//      virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(_rotatePhase)); }
//      virtual QTensor<double> getTSform() const { return QTensor<double>::pxgate(_rotatePhase); }
//      virtual void printGateInfo(bool st) const { printSingleQubitGate(" PX", st); }
//  };

class XGate : public MCPXGate {
public:
    XGate(size_t id) : MCPXGate(id) { _type = "x"; }
    virtual ~XGate() {}
    virtual std::string getTypeStr() const { return "x"; }
    virtual GateType getType() const { return GateType::X; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(1)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::xgate(); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("X", st); }
};

class SXGate : public MCPXGate {
public:
    SXGate(size_t id) : MCPXGate(id) { _type = "sx"; }
    virtual ~SXGate() {}
    virtual std::string getTypeStr() const { return "sx"; }
    virtual GateType getType() const { return GateType::SX; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, Phase(1, 2)); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::pxgate(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SX", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRX
//----------------------------------------------------------------------

// REVIEW - Check if it can be merged into MCRX after the implementation of MCRX ZX-form
class RXGate : public MCRXGate {
public:
    RXGate(size_t id) : MCRXGate(id) { _type = "rx"; }
    virtual ~RXGate() {}
    virtual std::string getTypeStr() const { return "rx"; }
    virtual GateType getType() const { return GateType::RX; }
    virtual ZXGraph* getZXform() { return mapSingleQubitGate(VertexType::X, _rotatePhase); }
    virtual QTensor<double> getTSform() const { return QTensor<double>::rxgate(_rotatePhase); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RX", st); }
};

// REVIEW - Check if CRX can be merged into MCRZ after the implementation of MCRZ ZX-form

//----------------------------------------------------------------------
//    Children class of MCPY
//----------------------------------------------------------------------
// REVIEW - Check if PY, CPY can be merged into MCPY after the implementation of MCPY ZX-form

class YGate : public MCPYGate {
public:
    YGate(size_t id) : MCPYGate(id) { _type = "y"; }
    virtual ~YGate() {}
    virtual std::string getTypeStr() const { return "y"; }
    virtual GateType getType() const { return GateType::Y; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::ygate(); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Y", st); }
};

class SYGate : public MCPYGate {
public:
    SYGate(size_t id) : MCPYGate(id) { _type = "sy"; }
    virtual ~SYGate() {}
    virtual std::string getTypeStr() const { return "sy"; }
    virtual GateType getType() const { return GateType::SY; }
    virtual ZXGraph* getZXform();
    virtual QTensor<double> getTSform() const { return QTensor<double>::pygate(Phase(1, 2)); }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SY", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRY
//----------------------------------------------------------------------
// REVIEW - Check if RY, CRY can be merged into MCPY after the implementation of MCPY ZX-form

#endif  // QCIR_GATE_H