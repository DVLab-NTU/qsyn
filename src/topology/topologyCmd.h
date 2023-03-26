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

CmdClass(DeviceCheckoutCmd);
CmdClass(DeviceResetCmd);
CmdClass(DeviceDeleteCmd);
CmdClass(DeviceNewCmd);
CmdClass(DeviceGraphReadCmd);
CmdClass(DevicePrintCmd);
CmdClass(DeviceGraphPrintCmd);

#define DT_CMD_MGR_NOT_EMPTY_OR_RETURN(str)                                                         \
    {                                                                                               \
        if (deviceMgr->getDTListItr() == deviceMgr->getDeviceList().end()) {                        \
            cerr << "Error: Device list is empty now. Please DTNEW/DTRead before " << str << ".\n"; \
            return CMD_EXEC_ERROR;                                                                  \
        }                                                                                           \
    }

#define DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id)                           \
    {                                                                  \
        if (!(deviceMgr->isID(id))) {                                  \
            cerr << "Error: Device " << (id) << " does not exist!!\n"; \
            return CMD_EXEC_ERROR;                                     \
        }                                                              \
    }

#define DT_CMD_ID_VALID_OR_RETURN(option, id, str)         \
    {                                                      \
        if (!myStr2Uns((option), (id))) {                  \
            cerr << "Error: invalid " << str << " ID!!\n"; \
            return errorOption(CMD_OPT_ILLEGAL, (option)); \
        }                                                  \
    }

#define DT_CMD_DTOPO_ID_NOT_EXIST_OR_RETURN(id)                                                                    \
    {                                                                                                              \
        if (deviceMgr->isID(id)) {                                                                                 \
            cerr << "Error: Device " << (id) << " already exists!! Add `-Replace` if you want to overwrite it.\n"; \
            return CMD_EXEC_ERROR;                                                                                 \
        }                                                                                                          \
    }

#endif  // TOPOLOGY_CMD_H