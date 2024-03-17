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

#include "qcir/gate_type.hpp"
#include "qsyn/qsyn_type.hpp"

namespace qsyn::qcir {

extern size_t SINGLE_DELAY;
extern size_t DOUBLE_DELAY;
extern size_t SWAP_DELAY;
extern size_t MULTIPLE_DELAY;

class QCirGate {
public:
    using QubitIdType = qsyn::QubitIdType;

    QCirGate(size_t id, Operation const& op, QubitIdList qubits)
        : _id(id),
          _operation{op},
          _qubits{std::move(qubits)} {}

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

private:
protected:
    size_t _id;
    Operation _operation;
    std::vector<QubitIdType> _qubits;
};

}  // namespace qsyn::qcir
