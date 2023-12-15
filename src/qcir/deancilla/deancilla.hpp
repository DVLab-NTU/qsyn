/****************************************************************************
  PackageName  [ qcir/deancilla ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <vector>

#include "qcir/qcir_mgr.hpp"

namespace qsyn::qcir {

void deancilla(QCirMgr& qcir_mgr, size_t target_ancilla_count, std::vector<QubitIdType> const& ancilla_qubit_indexes);

}  // namespace qsyn::qcir
