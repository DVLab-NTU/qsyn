/****************************************************************************
  FileName     [ tensorMgr.hpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define class TensorMgr structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <iosfwd>
#include <map>
#include <string>

#include "./qtensor.hpp"
#include "util/dataStructureManager.hpp"
#include "util/phase.hpp"

template <typename T>
class QTensor;

template <>
inline std::string dvlab_utils::dataInfoString(QTensor<double>* tensor) {
    return fmt::format("{:<19} #Dim: {}   {}",
                       tensor->getFileName().substr(0, 19),
                       tensor->dimension(),
                       fmt::join(tensor->getProcedures(), " ➔ "));
}

template <>
inline std::string dvlab_utils::dataName(QTensor<double>* tensor) {
    return tensor->getFileName();
}

using TensorMgr = dvlab_utils::DataStructureManager<QTensor<double>>;
