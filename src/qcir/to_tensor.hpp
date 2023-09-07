/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define conversion from QCir to Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./qcir.hpp"
#include "./qcir_gate.hpp"
#include "tensor/qtensor.hpp"

std::optional<QTensor<double>> to_tensor(QCirGate* gate);
std::optional<QTensor<double>> to_tensor(QCir const& qcir);