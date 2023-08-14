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
#include "tensor/qtensor.hpp"

std::optional<QTensor<double>> toTensor(QCirGate* gate);
std::optional<QTensor<double>> toTensor(QCir const& qcir);