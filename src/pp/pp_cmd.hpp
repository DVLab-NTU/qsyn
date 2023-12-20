/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define pp package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "qcir/qcir_mgr.hpp"

namespace qsyn::pp {
bool add_pp_cmds(dvlab::CommandLineInterface& cli, qcir::QCirMgr& qcir_mgr);

}
