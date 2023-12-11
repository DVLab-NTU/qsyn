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
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"

namespace qsyn::qcir {

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
    qsyn::QubitIdType _qubit;
    QCirGate* _prev;
    QCirGate* _next;
    bool _isTarget;
};

class QCirGate {
public:
    using QubitIdType = qsyn::QubitIdType;
    QCirGate(size_t id, GateRotationCategory type, dvlab::Phase ph) : _id(id), _rotation_category{type}, _phase{ph} {}

    // Basic access method
    std::string get_type_str() const;
    GateType get_type() const { return std::make_tuple(_rotation_category, _qubits.size(), _phase); }
    GateRotationCategory get_rotation_category() const { return _rotation_category; }
    size_t get_id() const { return _id; }
    size_t get_time() const { return _time; }
    size_t get_delay() const;
    dvlab::Phase get_phase() const { return _phase; }
    std::vector<QubitInfo> const& get_qubits() const { return _qubits; }
    void set_qubits(std::vector<QubitInfo> const& qubits) { _qubits = qubits; }
    QubitInfo get_qubit(QubitIdType qubit) const;
    size_t get_num_qubits() const { return _qubits.size(); }
    QubitInfo get_targets() const { return _qubits[_qubits.size() - 1]; }
    QubitInfo get_control() const { return _qubits[0]; }

    void set_id(size_t id) { _id = id; }
    void set_time(size_t time) { _time = time; }
    void set_child(QubitIdType qubit, QCirGate* c);
    void set_parent(QubitIdType qubit, QCirGate* p);

    void add_qubit(QubitIdType qubit, bool is_target);
    void set_target_qubit(QubitIdType qubit);
    void set_control_qubit(QubitIdType qubit) { _qubits[0]._qubit = qubit; }
    // DFS
    bool is_visited(unsigned global) { return global == _dfs_counter; }
    void set_visited(unsigned global) { _dfs_counter = global; }
    void add_dummy_child(QCirGate* c);

    // Printing functions
    void print_gate() const;
    void set_rotation_category(GateRotationCategory type);
    void set_phase(dvlab::Phase p);
    void print_gate_info(bool) const;

    bool is_h() const { return _rotation_category == GateRotationCategory::h; }
    bool is_x() const { return _rotation_category == GateRotationCategory::px && _phase == dvlab::Phase(1) && _qubits.size() == 1; }
    bool is_y() const { return _rotation_category == GateRotationCategory::py && _phase == dvlab::Phase(1) && _qubits.size() == 1; }
    bool is_z() const { return _rotation_category == GateRotationCategory::pz && _phase == dvlab::Phase(1) && _qubits.size() == 1; }
    bool is_cx() const { return _rotation_category == GateRotationCategory::px && _phase == dvlab::Phase(1) && _qubits.size() == 2; }
    bool is_cy() const { return _rotation_category == GateRotationCategory::py && _phase == dvlab::Phase(1) && _qubits.size() == 2; }
    bool is_cz() const { return _rotation_category == GateRotationCategory::pz && _phase == dvlab::Phase(1) && _qubits.size() == 2; }
    bool is_swap() const { return _rotation_category == GateRotationCategory::swap; }

private:
protected:
    size_t _id;
    GateRotationCategory _rotation_category;
    size_t _time                   = 0;
    unsigned _dfs_counter          = 0;
    std::vector<QubitInfo> _qubits = {};
    dvlab::Phase _phase;

    // void _print_single_qubit_gate(std::string const& gtype, bool show_rotation = false, bool show_time = false) const;
    void _print_single_qubit_or_controlled_gate(std::string gtype, bool show_rotation = false, bool show_time = false) const;
};

}  // namespace qsyn::qcir
