/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define class ZX-to-Tensor Mapper structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <cstddef>
#include <vector>

#include "tensor/qtensor.hpp"
#include "zx/zx_def.hpp"

namespace qsyn {

namespace zx {

class ZXGraph;
class ZXVertex;

}  // namespace zx

std::optional<tensor::QTensor<double>> to_tensor(zx::ZXGraph const& zxgraph);

tensor::QTensor<double> get_tensor_form(zx::ZXVertex* v);

}  // namespace qsyn