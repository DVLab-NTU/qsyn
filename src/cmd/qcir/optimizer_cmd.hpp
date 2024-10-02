/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"

namespace qsyn::qcir {

dvlab::Command qcir_optimize_cmd(QCirMgr& qcir_mgr);

}
