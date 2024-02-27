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

using dvlab::Command;

Command qcir_oracle_cmd(QCirMgr& qcir_mgr);
Command qcir_k_lut_cmd();
Command qcir_pebble_cmd();

}  // namespace qsyn::qcir
