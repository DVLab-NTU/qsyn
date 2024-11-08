/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_gate.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <set>

#include "./operation.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {

QCirGate::QCirGate(size_t id, Operation const& op, QubitIdList qubits)
    : _id(id),
      // Check operation.hpp for the reason of using raw pointers.
      _operation{new Operation(op)},
      _qubits{std::move(qubits)} {
    DVLAB_ASSERT(qubit_id_is_unique(_qubits), "Qubits must be unique!");
}

QCirGate::~QCirGate() {
    // Check operation.hpp for the reason of using raw pointers.
    delete _operation;
}

QCirGate::QCirGate(QCirGate const& other)
    : _id(other._id),
      // Check operation.hpp for the reason of using raw pointers.
      _operation{new Operation(*other._operation)},
      _qubits{other._qubits} {}

QCirGate::QCirGate(QCirGate&& other) noexcept
    : _id(other._id), _operation(other._operation),
      _qubits(std::move(other._qubits)) {
    // avoids double deletion.
    // Check operation.hpp for the reason of using raw pointers.
    other._operation = nullptr;
}

void QCirGate::swap(QCirGate& other) noexcept {
    using std::swap;
    swap(_id, other._id);
    swap(_operation, other._operation);
    swap(_qubits, other._qubits);
}

std::string QCirGate::get_type_str() const {
    return get_operation().get_type();
}

Operation const& QCirGate::get_operation() const { return *_operation; }

void QCirGate::set_operation(Operation const& op) {
    DVLAB_ASSERT(op.get_num_qubits() == _qubits.size(),
                 fmt::format("Operation {} cannot be set with {} qubits!",
                             op.get_type(), _qubits.size()));
    // Check operation.hpp for the reason of using raw pointers.
    auto temp  = _operation;
    _operation = new Operation(op);
    delete temp;
}

std::optional<size_t> QCirGate::get_pin_by_qubit(QubitIdType qubit) const {
    auto it = std::find(_qubits.begin(), _qubits.end(), qubit);
    if (it == _qubits.end()) return std::nullopt;
    return std::distance(_qubits.begin(), it);
}

void QCirGate::set_qubits(QubitIdList qubits) {
    if (qubits.size() != _qubits.size()) {
        spdlog::error("Qubits cannot be set with different size!");
        return;
    }
    if (!qubit_id_is_unique(qubits)) {
        spdlog::error("Qubits cannot be set with duplicate qubits!");
        return;
    }

    _qubits = std::move(qubits);
}

bool QCirGate::operator==(QCirGate const& rhs) const {
    return *_operation == *rhs._operation && _qubits == rhs._qubits;
}

bool QCirGate::qubit_id_is_unique(QubitIdList const& qubits) {
    return std::set(qubits.begin(), qubits.end()).size() == qubits.size();
}

}  // namespace qsyn::qcir
