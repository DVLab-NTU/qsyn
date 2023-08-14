/****************************************************************************
  FileName     [ qcirGate.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class qcirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>

#include "qcir/gateType.hpp"
#include "util/phase.hpp"

extern size_t SINGLE_DELAY;
extern size_t DOUBLE_DELAY;
extern size_t SWAP_DELAY;
extern size_t MULTIPLE_DELAY;

class QCirGate;

// ┌────────────────────────────────────────────────────────────────────────┐
// │                                                                        │
// │                        Hierarchy of QCirGates                          │
// │                                                                        │
// │                                   QCirGate                             │
// │         ┌──────────────────┬─────────┴─────────┬──────────────────┐    │
// │       Z-axis             X-axis              Y-axis               H    │
// │    ┌────┴────┐        ┌────┴────┐         ┌────┴────┐                  │
// │   MCP      MCRZ      MCPX     MCRX       MCPY     MCRY                 │
// │  ╌╌╌╌╌╌   ╌╌╌╌╌╌    ╌╌╌╌╌╌   ╌╌╌╌╌╌     ╌╌╌╌╌╌   ╌╌╌╌╌╌                │
// │  CCZ (2)            CCX (2)             (CCY)                          │
// │  CZ  (1)            CX  (1)             (CY)                           │
// │  P        RZ        PX       RX         PY       RY                    │
// │  Z                  X                   Y                              │
// │  S, SDG             SX                  SY                             │
// │  T, TDG             SWAP                                               │
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

extern std::unordered_map<std::string, GateType> str2GateType;
extern std::unordered_map<GateType, std::string> gateType2Str;

class QCirGate {
public:
    QCirGate(size_t id, Phase ph = Phase(0)) : _id(id), _rotatePhase(ph) {}
    virtual ~QCirGate() {}

    // Basic access method
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;
    size_t getId() const { return _id; }
    size_t getTime() const { return _time; }
    size_t getDelay() const;
    Phase getPhase() const { return _rotatePhase; }
    const std::vector<BitInfo>& getQubits() const { return _qubits; }
    const BitInfo getQubit(size_t qubit) const;
    size_t getNQubit() { return _qubits.size(); }
    const BitInfo getTarget() const { return _qubits[_qubits.size() - 1]; }
    const BitInfo getControl() const { return _qubits[0]; }

    void setId(size_t id) { _id = id; }
    void setTime(size_t time) { _time = time; }
    void setChild(size_t qubit, QCirGate* c);
    void setParent(size_t qubit, QCirGate* p);

    void addQubit(size_t qubit, bool isTarget);
    void setTargetBit(size_t qubit);
    void setControlBit(size_t qubit) {}
    // DFS
    bool isVisited(unsigned global) { return global == _DFSCounter; }
    void setVisited(unsigned global) { _DFSCounter = global; }
    void addDummyChild(QCirGate* c);

    // Printing functions
    void printGate() const;

    virtual void setRotatePhase(Phase p) {}
    virtual void printGateInfo(bool) const = 0;

private:
protected:
    size_t _id;
    size_t _time = 0;
    size_t _nqubit;
    unsigned _DFSCounter = 0;
    std::vector<BitInfo> _qubits;
    Phase _rotatePhase;

    void printSingleQubitGate(std::string, bool = false) const;
    void printMultipleQubitsGate(std::string, bool = false, bool = false) const;
};

class HGate : public QCirGate {
public:
    HGate(size_t id) : QCirGate(id, Phase(1)) {}
    virtual ~HGate() {}
    virtual std::string getTypeStr() const { return "h"; }
    virtual GateType getType() const { return GateType::H; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("H", st); }
};

/**
 * @brief Virtual Class: Gates rotating on Z-axis
 *
 */
class ZAxisGate : public QCirGate {
public:
    ZAxisGate(size_t id, Phase ph = Phase(0)) : QCirGate(id, ph) {}
    virtual ~ZAxisGate(){};
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;
    virtual void printGateInfo(bool st) const = 0;
};

/**
 * @brief Virtual Class: Gates rotating on X-axis
 *
 */
class XAxisGate : public QCirGate {
public:
    XAxisGate(size_t id, Phase ph = Phase(0)) : QCirGate(id, ph) {}
    virtual ~XAxisGate(){};
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;
    virtual void printGateInfo(bool st) const = 0;
};

/**
 * @brief Virtual Class: Gates rotating on Y-axis
 *
 */
class YAxisGate : public QCirGate {
public:
    YAxisGate(size_t id, Phase ph = Phase(0)) : QCirGate(id, ph) {}
    virtual ~YAxisGate(){};
    virtual std::string getTypeStr() const = 0;
    virtual GateType getType() const = 0;
    virtual void printGateInfo(bool st) const = 0;
};

//----------------------------------------------------------------------
//    MCPZXY, MCRZXY
//----------------------------------------------------------------------

class MCPGate : public ZAxisGate {
public:
    MCPGate(size_t id, Phase ph = Phase(0)) : ZAxisGate(id, ph) {}
    virtual ~MCPGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? "mcp" : _qubits.size() == 2 ? "cp"
                                                                                                     : "p"; }
    virtual GateType getType() const { return GateType::MCP; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("P", true, st); }
    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class MCRZGate : public ZAxisGate {
public:
    MCRZGate(size_t id, Phase ph = Phase(0)) : ZAxisGate(id, ph) {}
    virtual ~MCRZGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? "mcrz" : _qubits.size() == 2 ? "crz"
                                                                                                      : "rz"; }
    virtual GateType getType() const { return GateType::MCRZ; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(_qubits.size() > 1 ? " RZ" : "RZ", true, st); }
    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class MCPXGate : public XAxisGate {
public:
    MCPXGate(size_t id, Phase ph = Phase(0)) : XAxisGate(id, ph) {}
    virtual ~MCPXGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? "mcpx" : _qubits.size() == 2 ? "cpx"
                                                                                                      : "px"; }
    virtual GateType getType() const { return GateType::MCPX; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(_qubits.size() > 1 ? " PX" : "PX", true, st); }
    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class MCRXGate : public XAxisGate {
public:
    MCRXGate(size_t id, Phase ph = Phase(0)) : XAxisGate(id, ph) {}
    virtual ~MCRXGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? "mcrx" : _qubits.size() == 2 ? "crx"
                                                                                                      : "rx"; }
    virtual GateType getType() const { return GateType::MCRX; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(_qubits.size() > 1 ? " RX" : "RX", true, st); }
    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class MCPYGate : public YAxisGate {
public:
    MCPYGate(size_t id, Phase ph = Phase(0)) : YAxisGate(id, ph) {}
    virtual ~MCPYGate(){};

    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? "mcpy" : _qubits.size() == 2 ? "cpy"
                                                                                                      : "py"; }
    virtual GateType getType() const { return GateType::MCPY; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(_qubits.size() > 1 ? " PY" : "PY", true, st); }
    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

class MCRYGate : public YAxisGate {
public:
    MCRYGate(size_t id, Phase ph = Phase(0)) : YAxisGate(id, ph) {}
    virtual ~MCRYGate(){};
    virtual std::string getTypeStr() const { return _qubits.size() > 2 ? "mcry" : _qubits.size() == 2 ? "cry"
                                                                                                      : "ry"; }
    virtual GateType getType() const { return GateType::MCRY; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate(_qubits.size() > 1 ? " RY" : "RY", true, st); }
    virtual void setRotatePhase(Phase p) { _rotatePhase = p; }
};

//----------------------------------------------------------------------
//    Children class of MCP
//----------------------------------------------------------------------

class CCZGate : public MCPGate {
public:
    CCZGate(size_t id) : MCPGate(id, Phase(1)) {}
    virtual ~CCZGate() {}
    virtual std::string getTypeStr() const { return "ccz"; }
    virtual GateType getType() const { return GateType::CCZ; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }
};

class CZGate : public MCPGate {
public:
    CZGate(size_t id) : MCPGate(id, Phase(1)) {}
    virtual ~CZGate() {}
    virtual std::string getTypeStr() const { return "cz"; }
    virtual GateType getType() const { return GateType::CZ; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("Z", false, st); }

    const BitInfo getControl() const { return _qubits[0]; }
    void setControlBit(size_t q) { _qubits[0]._qubit = q; }
};

class PGate : public MCPGate {
public:
    PGate(size_t id) : MCPGate(id) {}
    virtual ~PGate() {}
    virtual std::string getTypeStr() const { return "p"; }
    virtual GateType getType() const { return GateType::P; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("P", st); }
};

class ZGate : public MCPGate {
public:
    ZGate(size_t id) : MCPGate(id, Phase(1)) {}
    virtual ~ZGate() {}
    virtual std::string getTypeStr() const { return "z"; }
    virtual GateType getType() const { return GateType::Z; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Z", st); }
};

class SGate : public MCPGate {
public:
    SGate(size_t id) : MCPGate(id, Phase(1, 2)) {}
    virtual ~SGate() {}
    virtual std::string getTypeStr() const { return "s"; }
    virtual GateType getType() const { return GateType::S; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("S", st); }
};

class SDGGate : public MCPGate {
public:
    SDGGate(size_t id) : MCPGate(id, Phase(-1, 2)) {}
    virtual ~SDGGate() {}
    virtual std::string getTypeStr() const { return "sdg"; }
    virtual GateType getType() const { return GateType::SDG; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Sdg", st); }
};

class TGate : public MCPGate {
public:
    TGate(size_t id) : MCPGate(id, Phase(1, 4)) {}
    virtual ~TGate() {}
    virtual std::string getTypeStr() const { return "t"; }
    virtual GateType getType() const { return GateType::T; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("T", st); }
};

class TDGGate : public MCPGate {
public:
    TDGGate(size_t id) : MCPGate(id, Phase(-1, 4)) {}
    virtual ~TDGGate() {}
    virtual std::string getTypeStr() const { return "tdg"; }
    virtual GateType getType() const { return GateType::TDG; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Tdg", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRZ
//----------------------------------------------------------------------

class RZGate : public MCRZGate {
public:
    RZGate(size_t id) : MCRZGate(id) {}
    virtual ~RZGate() {}
    virtual std::string getTypeStr() const { return "rz"; }
    virtual GateType getType() const { return GateType::RZ; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RZ", st); }
};

//----------------------------------------------------------------------
//    Children class of MCPX
//----------------------------------------------------------------------

class CCXGate : public MCPXGate {
public:
    CCXGate(size_t id) : MCPXGate(id, Phase(1)) {}
    virtual ~CCXGate() {}
    virtual std::string getTypeStr() const { return "ccx"; }
    virtual GateType getType() const { return GateType::CCX; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }
};

class CXGate : public MCPXGate {
public:
    CXGate(size_t id) : MCPXGate(id, Phase(1)) {}
    virtual ~CXGate() {}
    virtual std::string getTypeStr() const { return "cx"; }
    virtual GateType getType() const { return GateType::CX; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("X", false, st); }

    const BitInfo getControl() const { return _qubits[0]; }
    void setControlBit(size_t q) { _qubits[0]._qubit = q; }
};

class SWAPGate : public MCPXGate {
public:
    SWAPGate(size_t id) : MCPXGate(id) {}
    virtual ~SWAPGate() {}
    virtual std::string getTypeStr() const { return "sw"; }
    virtual GateType getType() const { return GateType::SWAP; }
    virtual void printGateInfo(bool st) const { printMultipleQubitsGate("SWP", false, st); }
};

class PXGate : public MCPXGate {
public:
    PXGate(size_t id) : MCPXGate(id) {}
    virtual ~PXGate() {}
    virtual std::string getTypeStr() const { return "px"; }
    virtual GateType getType() const { return GateType::PX; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("PX", st); }
};

class XGate : public MCPXGate {
public:
    XGate(size_t id) : MCPXGate(id, Phase(1)) {}
    virtual ~XGate() {}
    virtual std::string getTypeStr() const { return "x"; }
    virtual GateType getType() const { return GateType::X; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("X", st); }
};

class SXGate : public MCPXGate {
public:
    SXGate(size_t id) : MCPXGate(id, Phase(1, 2)) {}
    virtual ~SXGate() {}
    virtual std::string getTypeStr() const { return "sx"; }
    virtual GateType getType() const { return GateType::SX; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SX", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRX
//----------------------------------------------------------------------

class RXGate : public MCRXGate {
public:
    RXGate(size_t id) : MCRXGate(id) {}
    virtual ~RXGate() {}
    virtual std::string getTypeStr() const { return "rx"; }
    virtual GateType getType() const { return GateType::RX; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RX", st); }
};

//----------------------------------------------------------------------
//    Children class of MCPY
//----------------------------------------------------------------------

class YGate : public MCPYGate {
public:
    YGate(size_t id) : MCPYGate(id, Phase(1)) {}
    virtual ~YGate() {}
    virtual std::string getTypeStr() const { return "y"; }
    virtual GateType getType() const { return GateType::Y; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("Y", st); }
};

class PYGate : public MCPYGate {
public:
    PYGate(size_t id) : MCPYGate(id) {}
    virtual ~PYGate() {}
    virtual std::string getTypeStr() const { return "py"; }
    virtual GateType getType() const { return GateType::PY; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("PY", st); }
};

class SYGate : public MCPYGate {
public:
    SYGate(size_t id) : MCPYGate(id, Phase(1, 2)) {}
    virtual ~SYGate() {}
    virtual std::string getTypeStr() const { return "sy"; }
    virtual GateType getType() const { return GateType::SY; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("SY", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRY
//----------------------------------------------------------------------

class RYGate : public MCRYGate {
public:
    RYGate(size_t id) : MCRYGate(id) {}
    virtual ~RYGate() {}
    virtual std::string getTypeStr() const { return "ry"; }
    virtual GateType getType() const { return GateType::RY; }
    virtual void printGateInfo(bool st) const { printSingleQubitGate("RY", st); }
};
