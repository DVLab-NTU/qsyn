/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "qcir/qcir_mgr.hpp"

namespace qsyn::qcir {

dvlab::Command qcir_pebble_cmd();
dvlab::Command qcir_oracle_cmd(QCirMgr& qcir_mgr);

}  // namespace qsyn::qcir
