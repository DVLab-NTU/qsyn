/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QCir to Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "tensor/qtensor.hpp"

using namespace qsyn::qcir;

namespace qsyn {

std::optional<qsyn::tensor::QTensor<double>> to_tensor(QCirGate* gate);
std::optional<qsyn::tensor::QTensor<double>> to_tensor(QCir const& qcir);

}  // namespace qsyn
