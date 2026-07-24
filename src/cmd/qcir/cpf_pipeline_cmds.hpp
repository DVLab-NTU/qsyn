/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ CPF pipeline CLI registration. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"

namespace qsyn::qcir {

dvlab::Command qcir_to_u3cx_cmd(QCirMgr& qcir_mgr, qsyn::device::DeviceMgr& device_mgr);
dvlab::Command qcir_to_zyz_cmd(QCirMgr& qcir_mgr);
dvlab::Command qcir_to_tableau_cmd(QCirMgr& qcir_mgr, experimental::TableauMgr& tableau_mgr);
dvlab::Command qcir_from_tableau_cmd(QCirMgr& qcir_mgr, experimental::TableauMgr& tableau_mgr);
dvlab::Command qcir_cpf_pipeline_cmd(QCirMgr& qcir_mgr, qsyn::device::DeviceMgr& device_mgr);

bool add_qcpfq_cmd(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr,
                   qsyn::device::DeviceMgr& device_mgr);

}  // namespace qsyn::qcir
