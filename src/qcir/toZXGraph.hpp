/****************************************************************************
  FileName     [ qcirGate2ZX.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirGate Mapping functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./qcir.hpp"
#include "./qcirGate.hpp"
#include "zx/zxGraph.hpp"

std::optional<ZXGraph> toZXGraph(QCirGate* gate);
std::optional<ZXGraph> toZXGraph(QCir const& qcir);