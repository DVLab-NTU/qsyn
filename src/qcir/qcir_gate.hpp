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

#include "qcir/operation.hpp"
#include "qsyn/qsyn_type.hpp"
namespace qsyn::qcir {

extern size_t SINGLE_DELAY;
extern size_t DOUBLE_DELAY;
extern size_t SWAP_DELAY;
extern size_t MULTIPLE_DELAY;

class QCirGate {
public:
    QCirGate(size_t id, Operation const& op, QubitIdList qubits);
    QCirGate(Operation const& op, QubitIdList qubits) : QCirGate(0, op, std::move(qubits)) {}

    // Basic access method
    std::string get_type_str() const { return get_operation().get_type(); }
    Operation const& get_operation() const { return _operation; }
    void set_operation(Operation const& op);
    size_t get_id() const { return _id; }
    size_t get_delay() const;
    QubitIdList get_qubits() const { return _qubits; }
    QubitIdType get_qubit(size_t pin_id) const { return _qubits[pin_id]; }
    void set_qubits(QubitIdList qubits);
    std::optional<size_t> get_pin_by_qubit(QubitIdType qubit) const;

    size_t get_num_qubits() const { return _qubits.size(); }

    inline bool operator==(QCirGate const& rhs) const {
        return _operation == rhs._operation && _qubits == rhs._qubits;
    }

    inline bool operator!=(QCirGate const& rhs) const { return !(*this == rhs); }

    static bool qubit_id_is_unique(QubitIdList const& qubits);

protected:
    size_t _id;
    Operation _operation;
    std::vector<QubitIdType> _qubits;
};

}  // namespace qsyn::qcir
