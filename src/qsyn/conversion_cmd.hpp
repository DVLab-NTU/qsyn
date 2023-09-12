/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion among QCir, ZXGraph, and Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include "cli/cli.hpp"
#include "qcir/qcir_mgr.hpp"
#include "tensor/tensor_mgr.hpp"
#include "zx/zxgraph_mgr.hpp"

namespace qsyn {

bool add_conversion_cmds(dvlab::CommandLineInterface& cli, qcir::QCirMgr& qcir_mgr, tensor::TensorMgr& tensor_mgr, zx::ZXGraphMgr& zxgraph_mgr);

}