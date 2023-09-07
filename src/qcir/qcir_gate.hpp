/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "qcir/gate_type.hpp"
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
struct QubitInfo {
    size_t _qubit;
    QCirGate* _parent;
    QCirGate* _child;
    bool _isTarget;
};

extern std::unordered_map<std::string, GateType> STR_TO_GATE_TYPE;
extern std::unordered_map<GateType, std::string> GATE_TYPE_TO_STR;

class QCirGate {
public:
    QCirGate(size_t id, Phase ph = Phase(0)) : _id(id), _phase(ph) {}
    virtual ~QCirGate() {}

    // Basic access method
    virtual std::string get_type_str() const = 0;
    virtual GateType get_type() const = 0;
    size_t get_id() const { return _id; }
    size_t get_time() const { return _time; }
    size_t get_delay() const;
    Phase get_phase() const { return _phase; }
    std::vector<QubitInfo> const& get_qubits() const { return _qubits; }
    void set_qubits(std::vector<QubitInfo> const& qubits) { _qubits = qubits; }
    QubitInfo get_qubit(size_t qubit) const;
    size_t get_num_qubits() { return _qubits.size(); }
    QubitInfo get_targets() const { return _qubits[_qubits.size() - 1]; }
    QubitInfo get_control() const { return _qubits[0]; }

    void set_id(size_t id) { _id = id; }
    void set_time(size_t time) { _time = time; }
    void set_child(size_t qubit, QCirGate* c);
    void set_parent(size_t qubit, QCirGate* p);

    void add_qubit(size_t qubit, bool is_target);
    void set_target_qubit(size_t qubit);
    void set_control_qubit(size_t qubit) {}
    // DFS
    bool is_visited(unsigned global) { return global == _dfs_counter; }
    void set_visited(unsigned global) { _dfs_counter = global; }
    void add_dummy_child(QCirGate* c);

    // Printing functions
    void print_gate() const;

    virtual void set_phase(Phase p) {}
    virtual void print_gate_info(bool) const = 0;

private:
protected:
    size_t _id;
    size_t _time = 0;
    size_t _nqubit = 0;
    unsigned _dfs_counter = 0;
    std::vector<QubitInfo> _qubits;
    Phase _phase;

    void _print_single_qubit_gate(std::string, bool = false) const;
    void _print_multiple_qubits_gate(std::string, bool = false, bool = false) const;
};

class HGate : public QCirGate {
public:
    HGate(size_t id) : QCirGate(id, Phase(1)) {}
    virtual ~HGate() {}
    virtual std::string get_type_str() const { return "h"; }
    virtual GateType get_type() const { return GateType::h; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("H", st); }
};

/**
 * @brief Virtual Class: Gates rotating on Z-axis
 *
 */
class ZAxisGate : public QCirGate {
public:
    ZAxisGate(size_t id, Phase ph = Phase(0)) : QCirGate(id, ph) {}
    virtual ~ZAxisGate(){};
    virtual std::string get_type_str() const = 0;
    virtual GateType get_type() const = 0;
    virtual void print_gate_info(bool st) const = 0;
};

/**
 * @brief Virtual Class: Gates rotating on X-axis
 *
 */
class XAxisGate : public QCirGate {
public:
    XAxisGate(size_t id, Phase ph = Phase(0)) : QCirGate(id, ph) {}
    virtual ~XAxisGate(){};
    virtual std::string get_type_str() const = 0;
    virtual GateType get_type() const = 0;
    virtual void print_gate_info(bool st) const = 0;
};

/**
 * @brief Virtual Class: Gates rotating on Y-axis
 *
 */
class YAxisGate : public QCirGate {
public:
    YAxisGate(size_t id, Phase ph = Phase(0)) : QCirGate(id, ph) {}
    virtual ~YAxisGate(){};
    virtual std::string get_type_str() const = 0;
    virtual GateType get_type() const = 0;
    virtual void print_gate_info(bool st) const = 0;
};

//----------------------------------------------------------------------
//    MCPZXY, MCRZXY
//----------------------------------------------------------------------

class MCPGate : public ZAxisGate {
public:
    MCPGate(size_t id, Phase ph = Phase(0)) : ZAxisGate(id, ph) {}
    virtual ~MCPGate(){};
    virtual std::string get_type_str() const { return _qubits.size() > 2 ? "mcp" : _qubits.size() == 2 ? "cp"
                                                                                                       : "p"; }
    virtual GateType get_type() const { return GateType::mcp; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate("P", true, st); }
    virtual void set_phase(Phase p) { _phase = p; }
};

class MCRZGate : public ZAxisGate {
public:
    MCRZGate(size_t id, Phase ph = Phase(0)) : ZAxisGate(id, ph) {}
    virtual ~MCRZGate(){};
    virtual std::string get_type_str() const { return _qubits.size() > 2 ? "mcrz" : _qubits.size() == 2 ? "crz"
                                                                                                        : "rz"; }
    virtual GateType get_type() const { return GateType::mcrz; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate(_qubits.size() > 1 ? " RZ" : "RZ", true, st); }
    virtual void set_phase(Phase p) { _phase = p; }
};

class MCPXGate : public XAxisGate {
public:
    MCPXGate(size_t id, Phase ph = Phase(0)) : XAxisGate(id, ph) {}
    virtual ~MCPXGate(){};
    virtual std::string get_type_str() const { return _qubits.size() > 2 ? "mcpx" : _qubits.size() == 2 ? "cpx"
                                                                                                        : "px"; }
    virtual GateType get_type() const { return GateType::mcpx; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate(_qubits.size() > 1 ? " PX" : "PX", true, st); }
    virtual void set_phase(Phase p) { _phase = p; }
};

class MCRXGate : public XAxisGate {
public:
    MCRXGate(size_t id, Phase ph = Phase(0)) : XAxisGate(id, ph) {}
    virtual ~MCRXGate(){};
    virtual std::string get_type_str() const { return _qubits.size() > 2 ? "mcrx" : _qubits.size() == 2 ? "crx"
                                                                                                        : "rx"; }
    virtual GateType get_type() const { return GateType::mcrx; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate(_qubits.size() > 1 ? " RX" : "RX", true, st); }
    virtual void set_phase(Phase p) { _phase = p; }
};

class MCPYGate : public YAxisGate {
public:
    MCPYGate(size_t id, Phase ph = Phase(0)) : YAxisGate(id, ph) {}
    virtual ~MCPYGate(){};

    virtual std::string get_type_str() const { return _qubits.size() > 2 ? "mcpy" : _qubits.size() == 2 ? "cpy"
                                                                                                        : "py"; }
    virtual GateType get_type() const { return GateType::mcpy; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate(_qubits.size() > 1 ? " PY" : "PY", true, st); }
    virtual void set_phase(Phase p) { _phase = p; }
};

class MCRYGate : public YAxisGate {
public:
    MCRYGate(size_t id, Phase ph = Phase(0)) : YAxisGate(id, ph) {}
    virtual ~MCRYGate(){};
    virtual std::string get_type_str() const { return _qubits.size() > 2 ? "mcry" : _qubits.size() == 2 ? "cry"
                                                                                                        : "ry"; }
    virtual GateType get_type() const { return GateType::mcry; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate(_qubits.size() > 1 ? " RY" : "RY", true, st); }
    virtual void set_phase(Phase p) { _phase = p; }
};

//----------------------------------------------------------------------
//    Children class of MCP
//----------------------------------------------------------------------

class CCZGate : public MCPGate {
public:
    CCZGate(size_t id) : MCPGate(id, Phase(1)) {}
    virtual ~CCZGate() {}
    virtual std::string get_type_str() const { return "ccz"; }
    virtual GateType get_type() const { return GateType::ccz; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate("Z", false, st); }
};

class CZGate : public MCPGate {
public:
    CZGate(size_t id) : MCPGate(id, Phase(1)) {}
    virtual ~CZGate() {}
    virtual std::string get_type_str() const { return "cz"; }
    virtual GateType get_type() const { return GateType::cz; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate("Z", false, st); }

    QubitInfo get_control() const { return _qubits[0]; }
    void set_control_qubit(size_t q) { _qubits[0]._qubit = q; }
};

class PGate : public MCPGate {
public:
    PGate(size_t id) : MCPGate(id) {}
    virtual ~PGate() {}
    virtual std::string get_type_str() const { return "p"; }
    virtual GateType get_type() const { return GateType::p; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("P", st); }
};

class ZGate : public MCPGate {
public:
    ZGate(size_t id) : MCPGate(id, Phase(1)) {}
    virtual ~ZGate() {}
    virtual std::string get_type_str() const { return "z"; }
    virtual GateType get_type() const { return GateType::z; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("Z", st); }
};

class SGate : public MCPGate {
public:
    SGate(size_t id) : MCPGate(id, Phase(1, 2)) {}
    virtual ~SGate() {}
    virtual std::string get_type_str() const { return "s"; }
    virtual GateType get_type() const { return GateType::s; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("S", st); }
};

class SDGGate : public MCPGate {
public:
    SDGGate(size_t id) : MCPGate(id, Phase(-1, 2)) {}
    virtual ~SDGGate() {}
    virtual std::string get_type_str() const { return "sdg"; }
    virtual GateType get_type() const { return GateType::sdg; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("Sdg", st); }
};

class TGate : public MCPGate {
public:
    TGate(size_t id) : MCPGate(id, Phase(1, 4)) {}
    virtual ~TGate() {}
    virtual std::string get_type_str() const { return "t"; }
    virtual GateType get_type() const { return GateType::t; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("T", st); }
};

class TDGGate : public MCPGate {
public:
    TDGGate(size_t id) : MCPGate(id, Phase(-1, 4)) {}
    virtual ~TDGGate() {}
    virtual std::string get_type_str() const { return "tdg"; }
    virtual GateType get_type() const { return GateType::tdg; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("Tdg", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRZ
//----------------------------------------------------------------------

class RZGate : public MCRZGate {
public:
    RZGate(size_t id) : MCRZGate(id) {}
    virtual ~RZGate() {}
    virtual std::string get_type_str() const { return "rz"; }
    virtual GateType get_type() const { return GateType::rz; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("RZ", st); }
};

//----------------------------------------------------------------------
//    Children class of MCPX
//----------------------------------------------------------------------

class CCXGate : public MCPXGate {
public:
    CCXGate(size_t id) : MCPXGate(id, Phase(1)) {}
    virtual ~CCXGate() {}
    virtual std::string get_type_str() const { return "ccx"; }
    virtual GateType get_type() const { return GateType::ccx; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate("X", false, st); }
};

class CXGate : public MCPXGate {
public:
    CXGate(size_t id) : MCPXGate(id, Phase(1)) {}
    virtual ~CXGate() {}
    virtual std::string get_type_str() const { return "cx"; }
    virtual GateType get_type() const { return GateType::cx; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate("X", false, st); }

    QubitInfo get_control() const { return _qubits[0]; }
    void set_control_qubit(size_t q) { _qubits[0]._qubit = q; }
};

class SWAPGate : public MCPXGate {
public:
    SWAPGate(size_t id) : MCPXGate(id) {}
    virtual ~SWAPGate() {}
    virtual std::string get_type_str() const { return "sw"; }
    virtual GateType get_type() const { return GateType::swap; }
    virtual void print_gate_info(bool st) const { _print_multiple_qubits_gate("SWP", false, st); }
};

class PXGate : public MCPXGate {
public:
    PXGate(size_t id) : MCPXGate(id) {}
    virtual ~PXGate() {}
    virtual std::string get_type_str() const { return "px"; }
    virtual GateType get_type() const { return GateType::px; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("PX", st); }
};

class XGate : public MCPXGate {
public:
    XGate(size_t id) : MCPXGate(id, Phase(1)) {}
    virtual ~XGate() {}
    virtual std::string get_type_str() const { return "x"; }
    virtual GateType get_type() const { return GateType::x; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("X", st); }
};

class SXGate : public MCPXGate {
public:
    SXGate(size_t id) : MCPXGate(id, Phase(1, 2)) {}
    virtual ~SXGate() {}
    virtual std::string get_type_str() const { return "sx"; }
    virtual GateType get_type() const { return GateType::sx; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("SX", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRX
//----------------------------------------------------------------------

class RXGate : public MCRXGate {
public:
    RXGate(size_t id) : MCRXGate(id) {}
    virtual ~RXGate() {}
    virtual std::string get_type_str() const { return "rx"; }
    virtual GateType get_type() const { return GateType::rx; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("RX", st); }
};

//----------------------------------------------------------------------
//    Children class of MCPY
//----------------------------------------------------------------------

class YGate : public MCPYGate {
public:
    YGate(size_t id) : MCPYGate(id, Phase(1)) {}
    virtual ~YGate() {}
    virtual std::string get_type_str() const { return "y"; }
    virtual GateType get_type() const { return GateType::y; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("Y", st); }
};

class PYGate : public MCPYGate {
public:
    PYGate(size_t id) : MCPYGate(id) {}
    virtual ~PYGate() {}
    virtual std::string get_type_str() const { return "py"; }
    virtual GateType get_type() const { return GateType::py; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("PY", st); }
};

class SYGate : public MCPYGate {
public:
    SYGate(size_t id) : MCPYGate(id, Phase(1, 2)) {}
    virtual ~SYGate() {}
    virtual std::string get_type_str() const { return "sy"; }
    virtual GateType get_type() const { return GateType::sy; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("SY", st); }
};

//----------------------------------------------------------------------
//    Children class of MCRY
//----------------------------------------------------------------------

class RYGate : public MCRYGate {
public:
    RYGate(size_t id) : MCRYGate(id) {}
    virtual ~RYGate() {}
    virtual std::string get_type_str() const { return "ry"; }
    virtual GateType get_type() const { return GateType::ry; }
    virtual void print_gate_info(bool st) const { _print_single_qubit_gate("RY", st); }
};
