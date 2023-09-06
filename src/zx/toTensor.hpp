/****************************************************************************
  FileName     [ zx2tsMapper.hpp ]
  PackageName  [ zx ]
  Synopsis     [ Define class ZX-to-Tensor Mapper structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <cstddef>
#include <vector>

#include "tensor/qtensor.hpp"
#include "zx/zxDef.hpp"

class ZXGraph;
class ZXVertex;

std::optional<QTensor<double>> toTensor(ZXGraph const& zxgraph);

QTensor<double> getTSform(ZXVertex* v);
