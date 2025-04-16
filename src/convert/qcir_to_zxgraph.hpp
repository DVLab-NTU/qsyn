/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QCir to ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/operation.hpp"  // clangd might gives unused include warning,
                               // but this header is actually necessary for
                               // the to_zxgraph function
#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn {

std::optional<zx::ZXGraph> to_zxgraph(qcir::QCirGate const& gate);
template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::QCir const& qcir);

}  // namespace qsyn
