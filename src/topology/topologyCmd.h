/****************************************************************************
  FileName     [ topologyCmd.h ]
  PackageName  [ topology ]
  Synopsis     [ Define topology package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef TOPOLOGY_CMD_H
#define TOPOLOGY_CMD_H

#include "cmdParser.h"
#include "topologyMgr.h"  // for DeviceMgr

CmdClass(DeviceGraphPrintCmd);

#define DT_CMD_MGR_NOT_EMPTY_OR_RETURN(str)                                                         \
    {                                                                                               \
        if (deviceMgr->getDTListItr() == deviceMgr->getDeviceList().end()) {                        \
            cerr << "Error: Device list is empty now. Please DTNEW/DTRead before " << str << ".\n"; \
            return CMD_EXEC_ERROR;                                                                  \
        }                                                                                           \
    }

#endif  // TOPOLOGY_CMD_H