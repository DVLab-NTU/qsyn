
/****************************************************************************
  PackageName  [ device ]
  Synopsis     [ Define device package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"

namespace qsyn::device {

bool add_device_cmds(dvlab::CommandLineInterface& cli, qsyn::device::DeviceMgr& device_mgr);

}
