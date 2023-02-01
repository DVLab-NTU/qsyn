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
#include "topologyMgr.h"  // for DeviceTopoMgr

CmdClass(DeviceTopoCheckoutCmd);
CmdClass(DeviceTopoResetCmd);
CmdClass(DeviceTopoDeleteCmd);
CmdClass(DeviceTopoNewCmd);
CmdClass(DeviceTopoReadCmd);
CmdClass(DeviceTopoPrintCmd);

#define DT_CMD_MGR_NOT_EMPTY_OR_RETURN(str)                                                             \
    {                                                                                                   \
        if (deviceTopoMgr->getDTListItr() == deviceTopoMgr->getDeviceTopoList().end()) {                \
            cerr << "Error: DeviceTopo list is empty now. Please DTNEW/DTRead before " << str << ".\n"; \
            return CMD_EXEC_ERROR;                                                                      \
        }                                                                                               \
    }

#define DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id)                               \
    {                                                                      \
        if (!(deviceTopoMgr->isID(id))) {                                  \
            cerr << "Error: DeviceTopo " << (id) << " does not exist!!\n"; \
            return CMD_EXEC_ERROR;                                         \
        }                                                                  \
    }

#define DT_CMD_ID_VALID_OR_RETURN(option, id, str)         \
    {                                                      \
        if (!myStr2Uns((option), (id))) {                  \
            cerr << "Error: invalid " << str << " ID!!\n"; \
            return errorOption(CMD_OPT_ILLEGAL, (option)); \
        }                                                  \
    }

#define DT_CMD_DTOPO_ID_NOT_EXIST_OR_RETURN(id)                                                                        \
    {                                                                                                                  \
        if (deviceTopoMgr->isID(id)) {                                                                                 \
            cerr << "Error: DeviceTopo " << (id) << " already exists!! Add `-Replace` if you want to overwrite it.\n"; \
            return CMD_EXEC_ERROR;                                                                                     \
        }                                                                                                              \
    }

#endif  // TOPOLOGY_CMD_H