/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./qcir.hpp"
#include "./qcir_gate.hpp"
#include "zx/zxgraph.hpp"

std::optional<ZXGraph> to_zxgraph(QCirGate* gate, size_t decomposition_mode = 0);
std::optional<ZXGraph> to_zxgraph(QCir const& qcir, size_t decomposition_mode = 0);