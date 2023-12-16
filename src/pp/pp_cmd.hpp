/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define pp package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "argparse/argparse.hpp"
#include "qcir/qcir_mgr.hpp"

namespace qsyn::Phase_Polynomial{
bool add_pp_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr);

}