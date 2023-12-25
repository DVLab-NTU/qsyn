/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define functions for boolean oracle synthesis ]
  Author       [ Design Verification Lab ]
  Paper        [ https://arxiv.org/abs/1904.02121 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/oracle/oracle.hpp"

namespace qsyn::qcir {

void synthesize_boolean_oracle(QCir* /*qcir*/, size_t n_ancilla, size_t n_outputs, std::vector<size_t> const& truth_table) {
    spdlog::debug("synthesize_boolean_oracle: n_ancilla = {}, n_outputs = {}", n_ancilla, n_outputs);
    for (auto const& tt : truth_table) {
        spdlog::debug("synthesize_boolean_oracle: truth_table = {}", tt);
    }
}

}  // namespace qsyn::qcir
