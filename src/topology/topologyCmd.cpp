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
    if (!(cmdMgr->regCmd("DTCHeckout", 4, new DeviceTopoCheckoutCmd) &&
          cmdMgr->regCmd("DTReset", 3, new DeviceTopoResetCmd) &&
          cmdMgr->regCmd("DTDelete", 3, new DeviceTopoDeleteCmd) &&
          cmdMgr->regCmd("DTNew", 3, new DeviceTopoNewCmd) &&
          cmdMgr->regCmd("DTGRead", 4, new DeviceTopoGraphReadCmd) &&
          cmdMgr->regCmd("DTGPrint", 4, new DeviceTopoGraphPrintCmd) &&
          cmdMgr->regCmd("DTPrint", 3, new DeviceTopoPrintCmd))) {
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
        DT_CMD_ID_VALID_OR_RETURN(token, id, "DeviceTopo");
        DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id);
        deviceTopoMgr->checkout2DeviceTopo(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceTopoCheckoutCmd::usage() const {
    cout << "Usage: DTCHeckout <(size_t id)>" << endl;
}

void DeviceTopoCheckoutCmd::help() const {
    cout << setw(15) << left << "DTCHeckout: "
         << "checkout to DeviceTopo <id> in DeviceTopoMgr" << endl;
}

//----------------------------------------------------------------------
//    DTReset
//----------------------------------------------------------------------
CmdExecStatus
DeviceTopoResetCmd::exec(const string &option) {
    if (!lexNoOption(option)) return CMD_EXEC_ERROR;
    if (!deviceTopoMgr)
        deviceTopoMgr = new DeviceTopoMgr;
    else
        deviceTopoMgr->reset();
    return CMD_EXEC_DONE;
}

void DeviceTopoResetCmd::usage() const {
    cout << "Usage: DTReset" << endl;
}

void DeviceTopoResetCmd::help() const {
    cout << setw(15) << left << "DTReset: "
         << "reset DeviceTopoMgr" << endl;
}

//----------------------------------------------------------------------
//    DTDelete <(size_t id)>
//----------------------------------------------------------------------
CmdExecStatus
DeviceTopoDeleteCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        DT_CMD_ID_VALID_OR_RETURN(token, id, "DeviceTopo");
        DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id);
        deviceTopoMgr->removeDeviceTopo(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceTopoDeleteCmd::usage() const {
    cout << "Usage: DTDelete <size_t id>" << endl;
}

void DeviceTopoDeleteCmd::help() const {
    cout << setw(15) << left << "DTDelete: "
         << "remove a DeviceTopo from DeviceTopoMgr" << endl;
}

//----------------------------------------------------------------------
//    DTNew [(size_t id)]
//----------------------------------------------------------------------
CmdExecStatus
DeviceTopoNewCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        deviceTopoMgr->addDeviceTopo(deviceTopoMgr->getNextID());
    else {
        unsigned id;
        DT_CMD_ID_VALID_OR_RETURN(token, id, "DeviceTopo");
        deviceTopoMgr->addDeviceTopo(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceTopoNewCmd::usage() const {
    cout << "Usage: DTNew [size_t id]" << endl;
}

void DeviceTopoNewCmd::help() const {
    cout << setw(15) << left << "DTNew: "
         << "create a new DeviceTopo to DeviceTopoMgr" << endl;
}

//----------------------------------------------------------------------
//    DTGRead <(size_t filename)>
//----------------------------------------------------------------------
CmdExecStatus
DeviceTopoGraphReadCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    // FIXME - Temporary version
    deviceTopoMgr->addDeviceTopo(deviceTopoMgr->getNextID());
    deviceTopoMgr->getDeviceTopo()->readTopo(token);
    return CMD_EXEC_DONE;
}

void DeviceTopoGraphReadCmd::usage() const {
    cout << "Usage: DTGRead <(size_t filename)>" << endl;
}

void DeviceTopoGraphReadCmd::help() const {
    cout << setw(15) << left << "DTGRead: "
         << "read a device topology" << endl;
}

//-----------------------------------------------------------------------------------------------------------
//    DTGPrint [-Summary | -Edges | -Qubits]
//-----------------------------------------------------------------------------------------------------------
CmdExecStatus
DeviceTopoGraphPrintCmd::exec(const string &option) {
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;

    DT_CMD_MGR_NOT_EMPTY_OR_RETURN("DTGPrint");

    if (options.empty() || myStrNCmp("-Summary", options[0], 2) == 0)
        deviceTopoMgr->getDeviceTopo()->printTopo();
    else if (myStrNCmp("-Edges", options[0], 2) == 0) {
        CMD_N_OPTS_AT_MOST_OR_RETURN(options, 3)
        vector<size_t> candidates;
        for (size_t i = 1; i < options.size(); i++) {
            unsigned qid;
            if (myStr2Uns(options[i], qid))
                candidates.push_back(size_t(qid));
            else {
                cout << "Error: " << options[i] << " is not a valid qubit ID!!" << endl;
            }
        }
        deviceTopoMgr->getDeviceTopo()->printEdges(candidates);
    } else if (myStrNCmp("-Qubits", options[0], 2) == 0) {
        vector<size_t> candidates;
        for (size_t i = 1; i < options.size(); i++) {
            unsigned qid;
            if (myStr2Uns(options[i], qid))
                candidates.push_back(size_t(qid));
            else {
                cout << "Error: " << options[i] << " is not a valid qubit ID!!" << endl;
            }
        }
        deviceTopoMgr->getDeviceTopo()->printQubits(candidates);
    } else
        return errorOption(CMD_OPT_ILLEGAL, options[0]);
    return CMD_EXEC_DONE;
}

void DeviceTopoGraphPrintCmd::usage() const {
    cout << "Usage: DTGPrint [-Summary | -Edges | -Qubits]" << endl;
}

void DeviceTopoGraphPrintCmd::help() const {
    cout << setw(15) << left << "DTGPrint: "
         << "print info of device topology" << endl;
}

//----------------------------------------------------------------------
//    DTPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
CmdExecStatus
DeviceTopoPrintCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if (token.empty() || myStrNCmp("-Summary", token, 2) == 0) {
        deviceTopoMgr->printDeviceTopoMgr();
    } else if (myStrNCmp("-Focus", token, 2) == 0)
        deviceTopoMgr->printTopoListItr();
    else if (myStrNCmp("-Num", token, 2) == 0)
        deviceTopoMgr->printDeviceTopoListSize();
    else
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    return CMD_EXEC_DONE;
}

void DeviceTopoPrintCmd::usage() const {
    cout << "Usage: DTPrint [-Summary | -Focus | -Num]" << endl;
}

void DeviceTopoPrintCmd::help() const {
    cout << setw(15) << left << "DTPrint: "
         << "print info of DeviceTopoMgr" << endl;
}
