/****************************************************************************
  PackageName  [ qcir/deancilla ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "qcir/qcir_mgr.hpp"

namespace qsyn::qcir {

dvlab::Command qcir_deancilla_cmd(QCirMgr& qcir_mgr);

}