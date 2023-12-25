/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

namespace qsyn::qcir {

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 *
 * @param P number of ancilla qubits
 */
void test_pebble(const size_t P);

}  // namespace qsyn::qcir
