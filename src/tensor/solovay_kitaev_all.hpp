/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Apply Solovay-Kitaev decomposition to all rotation gates in QCir ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2025 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "cmd/qcir_mgr.hpp"

namespace qsyn {

namespace tensor {

/**
 * @brief Apply Solovay-Kitaev decomposition to all RX/RY/RZ gates in the given QCirMgr
 *
 * @param mgr The quantum circuit manager
 * @param depth Depth of the gate approximation tree
 * @param recursion Number of recursive refinements (SK recursion level)
 */
void solovay_kitaev_all(qcir::QCirMgr& mgr, size_t depth, size_t recursion);

}  // namespace tensor

}  // namespace qsyn