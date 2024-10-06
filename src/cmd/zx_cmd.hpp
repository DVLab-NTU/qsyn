/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "cli/cli.hpp"
#include "cmd/zxgraph_mgr.hpp"

namespace qsyn::zx {

std::function<bool(size_t const&)> valid_zxvertex_id(ZXGraphMgr const& zxgraph_mgr);
std::function<bool(size_t const&)> zxgraph_id_not_exist(ZXGraphMgr const& zxgraph_mgr);
std::function<bool(int const&)> zxgraph_input_qubit_not_exist(ZXGraphMgr const& zxgraph_mgr);
std::function<bool(int const&)> zxgraph_output_qubit_not_exist(ZXGraphMgr const& zxgraph_mgr);
bool add_zx_cmds(dvlab::CommandLineInterface& cli, qsyn::zx::ZXGraphMgr& zxgraph_mgr);

dvlab::Command zxgraph_test_cmd(ZXGraphMgr& zxgraph_mgr);

}  // namespace qsyn::zx
