/****************************************************************************
  FileName     [ qcirMgr.h ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir manager structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QCIR_MGR_H
#define QCIR_MGR_H

#include <cstddef>
#include <vector>

#include "./qcir.h"
#include "util/dataStructureManager.h"

using QCirMgr = dvlab_utils::DataStructureManager<QCir>;

extern QCirMgr qcirMgr;

bool qcirMgrNotEmpty(std::string const& command);

#endif  // QCIR_MGR_H
