/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "cli/cli.hpp"
#include "device/device_mgr.hpp"
#include "qcir/qcir_cmd.hpp"

namespace qsyn::duostra {

bool add_duostra_cmds(dvlab::CommandLineInterface& cli, qcir::QCirMgr& qcir_mgr, qsyn::device::DeviceMgr& device_mgr);

}
