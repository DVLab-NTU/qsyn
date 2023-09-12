/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZX-to-Tensor Mapper structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <cstddef>
#include <vector>

#include "tensor/qtensor.hpp"
#include "zx/zx_def.hpp"

class ZXGraph;
class ZXVertex;

std::optional<QTensor<double>> to_tensor(ZXGraph const& zxgraph);

QTensor<double> get_tensor_form(ZXVertex* v);
