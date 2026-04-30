/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"

namespace qsyn {

namespace experimental {

dvlab::Command tableau_cmd(TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr);

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr);

}  // namespace experimental

}  // namespace qsyn
