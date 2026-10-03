/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define class TensorMgr structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <map>
#include <string>

#include "tensor/qtensor.hpp"
#include "util/data_structure_manager.hpp"
#include "util/phase.hpp"

namespace qsyn::tensor {

template <typename T>
class QTensor;

using TensorMgr = dvlab::utils::DataStructureManager<QTensor<double>>;

}  // namespace qsyn::tensor

template <>
inline std::string dvlab::utils::data_structure_info_string(dvlab::utils::DataStructureManager<qsyn::tensor::QTensor<double>> const& mgr, size_t id) {
    auto* tensor = mgr.find_by_id(id);
    if (!tensor) return {};
    return fmt::format("{:<19} #Dim: {}   {}",
                       mgr.get_filename(id).substr(0, 19),
                       tensor->dimension(),
                       fmt::join(mgr.get_procedures(id), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(dvlab::utils::DataStructureManager<qsyn::tensor::QTensor<double>> const& mgr, size_t id) {
    return mgr.get_filename(id);
}
