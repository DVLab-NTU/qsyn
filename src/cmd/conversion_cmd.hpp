/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion among QCir, ZXGraph, and Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include "cli/cli.hpp"
#include "cmd/latticesurgery_mgr.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "cmd/tensor_mgr.hpp"
#include "cmd/zxgraph_mgr.hpp"

namespace qsyn {

bool add_conversion_cmds(dvlab::CommandLineInterface& cli, qcir::QCirMgr& qcir_mgr, tensor::TensorMgr& tensor_mgr, zx::ZXGraphMgr& zxgraph_mgr, experimental::TableauMgr& tableau_mgr, latticesurgery::LatticeSurgeryMgr& latticesurgery_mgr);

}
