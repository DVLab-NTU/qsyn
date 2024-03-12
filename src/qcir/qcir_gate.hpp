/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <tl/to.hpp>
#include <tuple>
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
    QCirGate* _prev;
    QCirGate* _next;
};

class QCirGate {
public:
    using QubitIdType = qsyn::QubitIdType;
    QCirGate(size_t id, GateRotationCategory type, QubitIdList qubits, dvlab::Phase ph)
        : _id(id),
          _operation{LegacyGateType{std::make_tuple(type, qubits.size(), ph)}},
          _rotation_category{type},
          _qubits{std::vector<QubitInfo>(qubits.size(), {nullptr, nullptr})},
          _operands{std::move(qubits)},
          _phase{ph} {}

    // Basic access method
    std::string get_type_str() const { return get_operation().get_type(); }
    Operation const& get_operation() const { return _operation; }
    void set_operation(Operation const& op);
    size_t get_id() const { return _id; }
    size_t get_delay() const;
    QubitIdType get_qubit(size_t pin_id) const { return _operands[pin_id]; }
    QubitIdList get_qubits() const { return _operands; }
    std::optional<size_t> get_pin_by_qubit(QubitIdType qubit) const {
        auto it = std::find(_operands.begin(), _operands.end(), qubit);
        if (it == _operands.end()) return std::nullopt;
        return std::distance(_operands.begin(), it);
    }
    void set_qubits(QubitIdList const& operands) {
        _operands = operands;
        _qubits   = std::vector<QubitInfo>(operands.size(), {nullptr, nullptr});
    }

    size_t get_num_qubits() const { return _qubits.size(); }

    // Printing functions
    void print_gate(std::optional<size_t> time) const;
    // void print_gate_info() const;

    void adjoint();

    /* to be removed in the future */
    std::vector<QubitInfo> const& legacy_get_qubits() const { return _qubits; }
    std::vector<QubitInfo>& legacy_get_qubits() { return _qubits; }

    GateRotationCategory get_rotation_category() const { return _rotation_category; }
    dvlab::Phase get_phase() const { return _phase; }

private:
protected:
    size_t _id;
    Operation _operation;
    GateRotationCategory _rotation_category;
    std::vector<QubitInfo> _qubits = {};
    std::vector<QubitIdType> _operands;
    dvlab::Phase _phase;

    // void _print_single_qubit_or_controlled_gate(std::string gtype, bool show_rotation = false) const;
};

}  // namespace qsyn::qcir
