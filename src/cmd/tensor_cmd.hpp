/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include "cli/cli.hpp"
#include "cmd/tensor_mgr.hpp"

namespace qsyn::tensor {

bool add_tensor_cmds(dvlab::CommandLineInterface& cli, TensorMgr& tensor_mgr);

}
