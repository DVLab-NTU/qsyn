/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for boolean oracle synthesis ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "qcir/oracle/xag.hpp"
#include "qcir/qcir.hpp"

namespace qsyn::qcir {

/**
 * @brief synthesize a boolean oracle for the given function truth table
 *
 * @param xag XAG
 * @param n_ancilla number of ancilla qubits
 * @param k maximum cut size for k-LUT partitioning
 */
std::optional<QCir> synthesize_boolean_oracle(XAG xag, size_t n_ancilla, size_t k);

}  // namespace qsyn::qcir
