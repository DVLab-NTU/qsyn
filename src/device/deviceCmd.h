/****************************************************************************
  FileName     [ deviceCmd.h ]
  PackageName  [ device ]
  Synopsis     [ Define device package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef DEVICE_CMD_H
#define DEVICE_CMD_H

#include "cmdParser.h"
#include "deviceMgr.h"  // for DeviceMgr

CmdClass(DeviceGraphPrintCmd);

#define DT_CMD_MGR_NOT_EMPTY_OR_RETURN(str)                                    \
    {                                                                          \
        if (deviceMgr->getDTListItr() == deviceMgr->getDeviceList().end()) {   \
            cerr << "Error: Device list is empty now. Please DTRead first.\n"; \
            return CMD_EXEC_ERROR;                                             \
        }                                                                      \
    }

#endif  // DEVICE_CMD_H