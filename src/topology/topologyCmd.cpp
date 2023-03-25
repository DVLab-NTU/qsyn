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
#include "topology.h"     // for Device
#include "topologyMgr.h"  // for DeviceMgr

using namespace std;

extern DeviceMgr *deviceMgr;
extern size_t verbose;
extern int effLimit;

bool initDeviceCmd() {
    deviceMgr = new DeviceMgr;
    if (!(cmdMgr->regCmd("DTCHeckout", 4, make_unique<DeviceCheckoutCmd>()) &&
          cmdMgr->regCmd("DTReset", 3, make_unique<DeviceResetCmd>()) &&
          cmdMgr->regCmd("DTDelete", 3, make_unique<DeviceDeleteCmd>()) &&
          cmdMgr->regCmd("DTNew", 3, make_unique<DeviceNewCmd>()) &&
          cmdMgr->regCmd("DTGRead", 4, make_unique<DeviceGraphReadCmd>()) &&
          cmdMgr->regCmd("DTGPrint", 4, make_unique<DeviceGraphPrintCmd>()) &&
          cmdMgr->regCmd("DTPrint", 3, make_unique<DevicePrintCmd>()))) {
        cerr << "Registering \"device topology\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    DTCHeckout <(size_t id)>
//----------------------------------------------------------------------

CmdExecStatus
DeviceCheckoutCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        DT_CMD_ID_VALID_OR_RETURN(token, id, "Device");
        DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id);
        deviceMgr->checkout2Device(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceCheckoutCmd::usage() const {
    cout << "Usage: DTCHeckout <(size_t id)>" << endl;
}

void DeviceCheckoutCmd::summary() const {
    cout << setw(15) << left << "DTCHeckout: "
         << "checkout to Device <id> in DeviceMgr" << endl;
}

//----------------------------------------------------------------------
//    DTReset
//----------------------------------------------------------------------
CmdExecStatus
DeviceResetCmd::exec(const string &option) {
    if (!lexNoOption(option)) return CMD_EXEC_ERROR;
    if (!deviceMgr)
        deviceMgr = new DeviceMgr;
    else
        deviceMgr->reset();
    return CMD_EXEC_DONE;
}

void DeviceResetCmd::usage() const {
    cout << "Usage: DTReset" << endl;
}

void DeviceResetCmd::summary() const {
    cout << setw(15) << left << "DTReset: "
         << "reset DeviceMgr" << endl;
}

//----------------------------------------------------------------------
//    DTDelete <(size_t id)>
//----------------------------------------------------------------------
CmdExecStatus
DeviceDeleteCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, "");
    else {
        unsigned id;
        DT_CMD_ID_VALID_OR_RETURN(token, id, "Device");
        DT_CMD_DTOPO_ID_EXISTS_OR_RETURN(id);
        deviceMgr->removeDevice(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceDeleteCmd::usage() const {
    cout << "Usage: DTDelete <size_t id>" << endl;
}

void DeviceDeleteCmd::summary() const {
    cout << setw(15) << left << "DTDelete: "
         << "remove a Device from DeviceMgr" << endl;
}

//----------------------------------------------------------------------
//    DTNew [(size_t id)]
//----------------------------------------------------------------------
CmdExecStatus
DeviceNewCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

    if (token.empty())
        deviceMgr->addDevice(deviceMgr->getNextID());
    else {
        unsigned id;
        DT_CMD_ID_VALID_OR_RETURN(token, id, "Device");
        deviceMgr->addDevice(id);
    }
    return CMD_EXEC_DONE;
}

void DeviceNewCmd::usage() const {
    cout << "Usage: DTNew [size_t id]" << endl;
}

void DeviceNewCmd::summary() const {
    cout << setw(15) << left << "DTNew: "
         << "create a new Device to DeviceMgr" << endl;
}

//----------------------------------------------------------------------
//    DTGRead <(size_t filename)> [-Replace]
//----------------------------------------------------------------------
CmdExecStatus
DeviceGraphReadCmd::exec(const string &option) {
    vector<string> options;
    if (!CmdExec::lexOptions(option, options))
        return CMD_EXEC_ERROR;
    if (options.empty())
        return CmdExec::errorOption(CMD_OPT_MISSING, "");

    bool doReplace = false;
    string fileName;
    for (size_t i = 0, n = options.size(); i < n; ++i) {
        if (myStrNCmp("-Replace", options[i], 2) == 0) {
            if (doReplace)
                return CmdExec::errorOption(CMD_OPT_EXTRA, options[i]);
            doReplace = true;
        } else {
            if (fileName.size())
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, options[i]);
            fileName = options[i];
        }
    }
    Device bufferTopo = Device(0);
    if (!bufferTopo.readDevice(fileName)) {
        cerr << "Error: The format in \"" << fileName << "\" has something wrong!!" << endl;
        return CMD_EXEC_ERROR;
    }
    if (deviceMgr->getDTListItr() == deviceMgr->getDeviceList().end()) {
        deviceMgr->addDevice(deviceMgr->getNextID());
    } else {
        if (doReplace) {
            if (verbose >= 1) cout << "Note: original Device is replaced..." << endl;
        } else {
            deviceMgr->addDevice(deviceMgr->getNextID());
        }
    }
    deviceMgr->setDevice(bufferTopo);
    return CMD_EXEC_DONE;
}

void DeviceGraphReadCmd::usage() const {
    cout << "Usage: DTGRead <(size_t filename)> [-Replace]" << endl;
}

void DeviceGraphReadCmd::summary() const {
    cout << setw(15) << left << "DTGRead: "
         << "read a device topology" << endl;
}

//-----------------------------------------------------------------------------------------------------------
//    DTGPrint [-Summary | -Edges | -Path | -Qubit]
//-----------------------------------------------------------------------------------------------------------
CmdExecStatus
DeviceGraphPrintCmd::exec(const string &option) {
    // check option
    vector<string> options;
    if (!CmdExec::lexOptions(option, options)) return CMD_EXEC_ERROR;

    DT_CMD_MGR_NOT_EMPTY_OR_RETURN("DTGPrint");

    if (options.empty() || myStrNCmp("-Summary", options[0], 2) == 0)
        deviceMgr->getDevice().printTopology();
    else if (myStrNCmp("-Edges", options[0], 2) == 0) {
        CMD_N_OPTS_AT_MOST_OR_RETURN(options, 3)
        vector<size_t> candidates;
        for (size_t i = 1; i < options.size(); i++) {
            unsigned qid;
            if (myStr2Uns(options[i], qid))
                candidates.push_back(size_t(qid));
            else {
                cout << "Warning: " << options[i] << " is not a valid qubit ID!!" << endl;
            }
        }
        deviceMgr->getDevice().printEdges(candidates);
    } else if (myStrNCmp("-Qubits", options[0], 2) == 0) {
        vector<size_t> candidates;
        for (size_t i = 1; i < options.size(); i++) {
            unsigned qid;
            if (myStr2Uns(options[i], qid))
                candidates.push_back(size_t(qid));
            else {
                cout << "Warning: " << options[i] << " is not a valid qubit ID!!" << endl;
            }
        }
        deviceMgr->getDevice().printQubits(candidates);
    } else if (myStrNCmp("-Path", options[0], 2) == 0) {
        CMD_N_OPTS_EQUAL_OR_RETURN(options, 3)
        unsigned qid0, qid1;
        if (!myStr2Uns(options[1], qid0)) cout << "Warning: " << options[1] << " is not a valid qubit ID!!" << endl;
        if (!myStr2Uns(options[2], qid1)) cout << "Warning: " << options[2] << " is not a valid qubit ID!!" << endl;

        deviceMgr->getDevice().printPath(qid0, qid1);
    } else
        return errorOption(CMD_OPT_ILLEGAL, options[0]);
    return CMD_EXEC_DONE;
}

void DeviceGraphPrintCmd::usage() const {
    cout << "Usage: DTGPrint [-Summary | -Edges | -Path | -Qubit]" << endl;
}

void DeviceGraphPrintCmd::summary() const {
    cout << setw(15) << left << "DTGPrint: "
         << "print info of device topology" << endl;
}

//----------------------------------------------------------------------
//    DTPrint [-Summary | -Focus | -List | -Num]
//----------------------------------------------------------------------
CmdExecStatus
DevicePrintCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if (token.empty() || myStrNCmp("-Summary", token, 2) == 0)
        deviceMgr->printDeviceMgr();
    else if (myStrNCmp("-Focus", token, 2) == 0)
        deviceMgr->printDeviceListItr();
    else if (myStrNCmp("-List", token, 2) == 0)
        deviceMgr->printDeviceList();
    else if (myStrNCmp("-Num", token, 2) == 0)
        deviceMgr->printDeviceListSize();
    else
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    return CMD_EXEC_DONE;
}

void DevicePrintCmd::usage() const {
    cout << "Usage: DTPrint [-Summary | -Focus | -List | -Num]" << endl;
}

void DevicePrintCmd::summary() const {
    cout << setw(15) << left << "DTPrint: "
         << "print info of DeviceMgr" << endl;
}
