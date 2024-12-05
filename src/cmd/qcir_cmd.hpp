/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./qcir_mgr.hpp"
#include "cli/cli.hpp"

namespace qsyn::qcir {

std::function<bool(size_t const&)> valid_qcir_id(QCirMgr const& qcir_mgr);
std::function<bool(size_t const&)> valid_qcir_gate_id(QCirMgr const& qcir_mgr);
std::function<bool(QubitIdType const&)> valid_qcir_qubit_id(QCirMgr const& qcir_mgr);

bool add_qcir_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr);

}  // namespace qsyn::qcir
