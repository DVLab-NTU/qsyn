/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>

namespace qsyn::qcir {

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 *
 * @param P number of ancilla qubits
 * @param filepath path to the in put dependency graph file
 */
void test_pebble(const size_t P, const std::string& filepath);

}  // namespace qsyn::qcir
