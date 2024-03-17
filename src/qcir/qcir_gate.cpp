/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_gate.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <ranges>
#include <string>
#include <type_traits>

#include "qcir/gate_type.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {

size_t SINGLE_DELAY   = 1;
size_t DOUBLE_DELAY   = 2;
size_t SWAP_DELAY     = 6;
size_t MULTIPLE_DELAY = 5;

void QCirGate::set_operation(Operation const& op) {
    if (op.get_num_qubits() != _qubits.size()) {
        spdlog::error("Operation {} cannot be set with {} qubits!", op.get_type(), _qubits.size());
        return;
    }

    _operation = op;
}

/**
 * @brief Get delay of gate
 *
 * @return size_t
 */
size_t QCirGate::get_delay() const {
    if (get_operation() == SwapGate{})
        return SWAP_DELAY;
    if (_qubits.size() == 1)
        return SINGLE_DELAY;
    if (_qubits.size() == 2)
        return DOUBLE_DELAY;
    return MULTIPLE_DELAY;
}

void QCirGate::adjoint() {
    _operation = qsyn::qcir::adjoint(_operation);
}

}  // namespace qsyn::qcir
