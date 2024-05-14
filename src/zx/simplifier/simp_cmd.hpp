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

dvlab::Command zxgraph_optimize_cmd(ZXGraphMgr& zxgraph_mgr);
dvlab::Command zxgraph_rule_cmd(ZXGraphMgr& zxgraph_mgr);
dvlab::Command zxgraph_manual_apply_cmd(ZXGraphMgr& zxgraph_mgr);

}  // namespace qsyn::zx
