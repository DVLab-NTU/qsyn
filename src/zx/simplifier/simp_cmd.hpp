/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "../zxgraph_mgr.hpp"
#include "cli/cli.hpp"

namespace qsyn::zx {

bool add_zx_simplifier_cmds(dvlab::CommandLineInterface& cli, zx::ZXGraphMgr& zxgraph_mgr);

}  // namespace qsyn::zx
