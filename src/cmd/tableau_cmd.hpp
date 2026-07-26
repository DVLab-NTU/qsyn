/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./tableau_cmd.hpp"
#include "cli/cli.hpp"
#include "cmd/ncf_mgr.hpp"
#include "cmd/tableau_mgr.hpp"

namespace qsyn {

namespace experimental {

dvlab::Command tableau_cmd();

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr, NcfMgr& ncf_mgr);

// Command to exponentiate Pauli strings / Hamiltonian terms into a tableau
dvlab::Command tableau_from_pauli_cmd(TableauMgr& tableau_mgr);

}  // namespace experimental

}  // namespace qsyn
