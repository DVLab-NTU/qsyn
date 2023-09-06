/****************************************************************************
  FileName     [ qcirMgr.hpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "./qcir.hpp"
#include "util/dataStructureManager.hpp"

template <>
inline std::string dvlab::utils::dataInfoString(QCir* qc) {
    return fmt::format("{:<19} {}", qc->getFileName().substr(0, 19),
                       fmt::join(qc->getProcedures(), " ➔ "));
}

template <>
inline std::string dvlab::utils::dataName(QCir* qc) {
    return qc->getFileName();
}

using QCirMgr = dvlab::utils::DataStructureManager<QCir>;

extern QCirMgr qcirMgr;

bool qcirMgrNotEmpty();
