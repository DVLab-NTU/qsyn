/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "./qcir.hpp"
#include "util/data_structure_manager.hpp"

template <>
inline std::string dvlab::utils::data_structure_info_string(QCir* t) {
    return fmt::format("{:<19} {}", t->get_filename().substr(0, 19),
                       fmt::join(t->get_procedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::data_structure_name(QCir* t) {
    return t->get_filename();
}

using QCirMgr = dvlab::utils::DataStructureManager<QCir>;

extern QCirMgr QCIR_MGR;

bool qcir_mgr_not_empty();
