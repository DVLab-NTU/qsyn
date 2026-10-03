/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "./cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "util/data_structure_manager.hpp"

namespace qsyn::qcir {

using QCirMgr = dvlab::utils::DataStructureManager<QCir>;

}  // namespace qsyn::qcir

template <>
inline std::string dvlab::utils::data_structure_info_string(dvlab::utils::DataStructureManager<qsyn::qcir::QCir> const& mgr, size_t id) {
    return fmt::format("{:<19} {}", mgr.get_filename(id).substr(0, 19),
                       fmt::join(mgr.get_procedures(id), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(dvlab::utils::DataStructureManager<qsyn::qcir::QCir> const& mgr, size_t id) {
    return mgr.get_filename(id);
}
