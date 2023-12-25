/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

namespace qsyn::qcir {

/**
 * @brief test ancilla qubit scheduling with SAT based reversible pebbling game
 */
void test_pebble();

}  // namespace qsyn::qcir
