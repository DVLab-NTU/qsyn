/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "./zxgraph_mgr.hpp"
#include "cli/cli.hpp"

namespace qsyn::zx {

bool add_zx_cmds(dvlab::CommandLineInterface& cli, qsyn::zx::ZXGraphMgr& zxgraph_mgr);

}  // namespace qsyn::zx
