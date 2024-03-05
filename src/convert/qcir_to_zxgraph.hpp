/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QCir to ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn {

std::optional<zx::ZXGraph> to_zxgraph(qcir::QCirGate* gate, size_t decomposition_mode = 3);
std::optional<zx::ZXGraph> to_zxgraph(qcir::QCir const& qcir, size_t decomposition_mode = 3);

}  // namespace qsyn
