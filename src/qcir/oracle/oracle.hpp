/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for boolean oracle synthesis ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"

namespace qsyn::qcir {

/**
 * @brief synthesize a boolean oracle for the given function truth table
 *
 * @param qcir
 * @param n_ancilla
 * @param n_outputs
 * @param truth_table i-th element is the output value when input is i
 */
void synthesize_boolean_oracle(QCir* qcir, size_t n_ancilla, size_t n_outputs, std::vector<size_t> const& truth_table);

}  // namespace qsyn::qcir
