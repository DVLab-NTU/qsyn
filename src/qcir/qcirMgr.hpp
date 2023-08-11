/****************************************************************************
  FileName     [ qcirMgr.h ]
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

using QCirMgr = dvlab_utils::DataStructureManager<QCir>;

extern QCirMgr qcirMgr;

bool qcirMgrNotEmpty(std::string const& command);


