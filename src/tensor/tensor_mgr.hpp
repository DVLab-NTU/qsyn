/****************************************************************************
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
#include "util/data_structure_manager.hpp"
#include "util/phase.hpp"

template <typename T>
class QTensor;

template <>
inline std::string dvlab::utils::data_structure_info_string(QTensor<double>* tensor) {
    return fmt::format("{:<19} #Dim: {}   {}",
                       tensor->get_filename().substr(0, 19),
                       tensor->dimension(),
                       fmt::join(tensor->get_procedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(QTensor<double>* tensor) {
    return tensor->get_filename();
}

using TensorMgr = dvlab::utils::DataStructureManager<QTensor<double>>;
