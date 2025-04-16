/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion from QCir to Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/operation.hpp"  // clangd might gives unused include warning,
                               // but this header is actually necessary for
                               // the to_tensor function
#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "tensor/qtensor.hpp"

using namespace qsyn::qcir;

namespace qsyn {

std::optional<qsyn::tensor::QTensor<double>> to_tensor(QCirGate const& gate);
template <>
std::optional<qsyn::tensor::QTensor<double>> to_tensor(QCir const& qcir);

}  // namespace qsyn
