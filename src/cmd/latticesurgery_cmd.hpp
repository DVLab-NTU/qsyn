/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define latticesurgery package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./latticesurgery_mgr.hpp"
#include "cli/cli.hpp"

namespace qsyn::latticesurgery {

std::function<bool(size_t const&)> valid_latticesurgery_id(LatticeSurgeryMgr const& ls_mgr);
std::function<bool(size_t const&)> valid_latticesurgery_gate_id(LatticeSurgeryMgr const& ls_mgr);
std::function<bool(QubitIdType const&)> valid_latticesurgery_qubit_id(LatticeSurgeryMgr const& ls_mgr);

bool add_latticesurgery_cmds(dvlab::CommandLineInterface& cli, LatticeSurgeryMgr& ls_mgr);

}  // namespace qsyn::latticesurgery