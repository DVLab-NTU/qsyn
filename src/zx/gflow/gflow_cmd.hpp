/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define gflow package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "../zxgraph_mgr.hpp"
#include "cli/cli.hpp"

namespace qsyn::zx {
dvlab::Command zxgraph_gflow_cmd(ZXGraphMgr const& zxgraph_mgr);

}  // namespace qsyn::zx
