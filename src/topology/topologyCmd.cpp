/****************************************************************************
  FileName     [ topologyCmd.cpp ]
  PackageName  [ topology ]
  Synopsis     [ Define topology package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "topologyCmd.h"

#include <cstddef>        // for size_t, NULL
#include <iomanip>        // for ostream
#include <iostream>       // for ostream
#include <string>         // for string

#include "cmdMacros.h"    // for CMD_N_OPTS_AT_MOST_OR_RETURN
#include "topology.h"     // for DeviceTopo
#include "topologyMgr.h"  // for DeviceTopoMgr

using namespace std;

extern DeviceTopoMgr *deviceTopoMgr;
extern size_t verbose;
extern int effLimit;

bool initDeviceTopoCmd() {
    deviceTopoMgr = new DeviceTopoMgr;
    if (!(cmdMgr->regCmd("DTCHeckout", 4, new DeviceTopoCheckoutCmd)  //&&
                                                                      // cmdMgr->regCmd("DTReset", 3, new DeviceTopoResetCmd) &&
                                                                      // cmdMgr->regCmd("DTDelete", 3, new DeviceTopoDeleteCmd) &&
                                                                      // cmdMgr->regCmd("DTNew", 3, new DeviceTopoNewCmd) &&
                                                                      // cmdMgr->regCmd("DTRead", 3, new DeviceTopoReadCmd) &&
                                                                      // cmdMgr->regCmd("DTPrint", 3, new DeviceTopoPrintCmd)
          )) {
        cerr << "Registering \"device topology\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    DTCHeckout <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
DeviceTopoCheckoutCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        DT_CMD_ID_VALID_OR_RETURN(token, id, "QCir");
        DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id);
        // deviceTopoMgr->checkout2DeviceTopo(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceTopoCheckoutCmd::usage() const {
    cout << "Usage: QCCHeckout <(size_t id)>" << endl;
}

void DeviceTopoCheckoutCmd::help() const {
    cout << setw(15) << left << "QCCHeckout: "
         << "checkout to QCir <id> in QCirMgr" << endl;
}