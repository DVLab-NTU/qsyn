/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for boolean oracle synthesis ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "qcir/qcir.hpp"

namespace qsyn::qcir {

/**
 * @brief synthesize a boolean oracle for the given function truth table
 *
 * @param xag_input input stream of the XAG file
 * @param qcir the QCIR object to be synthesized
 * @param n_ancilla number of ancilla qubits
 * @param k maximum cut size for k-LUT partitioning
 */
void synthesize_boolean_oracle(std::istream& xag_input, QCir* /*qcir*/, size_t n_ancilla, size_t k);

}  // namespace qsyn::qcir
