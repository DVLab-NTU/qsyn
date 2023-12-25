/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <vector>

#include "qcir/qcir_mgr.hpp"

namespace qsyn::qcir {

/**
 * @brief change the number of ancilla qubits to target ancilla count by SAT based reversible pebbling game
 *
 * @param qcir_mgr
 * @param target_ancilla_count
 * @param ancilla_qubit_indexes
 */
void pebble(QCirMgr& qcir_mgr, size_t target_ancilla_count, std::vector<QubitIdType> const& ancilla_qubit_indexes);

}  // namespace qsyn::qcir
