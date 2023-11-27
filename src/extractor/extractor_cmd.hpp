#pragma once

#include "cli/cli.hpp"
#include "qcir/qcir_mgr.hpp"
#include "zx/zxgraph_mgr.hpp"
namespace qsyn::extractor {

bool add_extract_cmds(dvlab::CommandLineInterface& cli, zx::ZXGraphMgr& zxgraph_mgr, qcir::QCirMgr& qcir_mgr);

}
