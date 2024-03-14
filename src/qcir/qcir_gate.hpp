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

class QCirGate {
public:
    using QubitIdType = qsyn::QubitIdType;

    QCirGate(size_t id, Operation const& op, QubitIdList qubits)
        : _id(id),
          _operation{op},
          _rotation_category{op.get_underlying<LegacyGateType>().get_rotation_category()},
          _qubits{std::move(qubits)},
          _phase{op.get_underlying<LegacyGateType>().get_phase()} {}

    // Basic access method
    std::string get_type_str() const { return get_operation().get_type(); }
    Operation const& get_operation() const { return _operation; }
    void set_operation(Operation const& op);
    size_t get_id() const { return _id; }
    size_t get_delay() const;
    QubitIdType get_qubit(size_t pin_id) const { return _qubits[pin_id]; }
    QubitIdList get_qubits() const { return _qubits; }
    std::optional<size_t> get_pin_by_qubit(QubitIdType qubit) const {
        auto it = std::find(_qubits.begin(), _qubits.end(), qubit);
        if (it == _qubits.end()) return std::nullopt;
        return std::distance(_qubits.begin(), it);
    }
    void set_qubits(QubitIdList const& qubits) { _qubits = qubits; }

    size_t get_num_qubits() const { return _qubits.size(); }

    // Printing functions

    void adjoint();

    GateRotationCategory get_rotation_category() const { return _rotation_category; }
    dvlab::Phase get_phase() const { return _phase; }

private:
protected:
    size_t _id;
    Operation _operation;
    GateRotationCategory _rotation_category;
    std::vector<QubitIdType> _qubits;
    dvlab::Phase _phase;

    // void _print_single_qubit_or_controlled_gate(std::string gtype, bool show_rotation = false) const;
};

}  // namespace qsyn::qcir
