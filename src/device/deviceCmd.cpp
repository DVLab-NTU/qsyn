/****************************************************************************
  FileName     [ deviceCmd.cpp ]
  PackageName  [ device ]
  Synopsis     [ Define device package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "deviceCmd.h"

#include <cstddef>   // for size_t, NULL
#include <iomanip>   // for ostream
#include <iostream>  // for ostream
#include <string>    // for string

#include "apCmd.h"
#include "cmdMacros.h"  // for CMD_N_OPTS_AT_MOST_OR_RETURN
#include "cmdParser.h"
#include "device.h"     // for Device
#include "deviceMgr.h"  // for DeviceMgr

using namespace std;
using namespace ArgParse;

extern DeviceMgr* deviceMgr;
extern size_t verbose;
extern int effLimit;

unique_ptr<ArgParseCmdType> dtCheckOutCmd();
unique_ptr<ArgParseCmdType> dtResetCmd();
unique_ptr<ArgParseCmdType> dtDeleteCmd();
unique_ptr<ArgParseCmdType> dtGraphReadCmd();
// unique_ptr<ArgParseCmdType> dtGraphPrintCmd(); // requires subparsers
unique_ptr<ArgParseCmdType> dtPrintCmd();

bool initDeviceCmd() {
    deviceMgr = new DeviceMgr;
    if (!(cmdMgr->regCmd("DTCHeckout", 4, dtCheckOutCmd()) &&
          cmdMgr->regCmd("DTReset", 3, dtResetCmd()) &&
          cmdMgr->regCmd("DTDelete", 3, dtDeleteCmd()) &&
          cmdMgr->regCmd("DTGRead", 4, dtGraphReadCmd()) &&
          cmdMgr->regCmd("DTGPrint", 4, make_unique<DeviceGraphPrintCmd>()) &&
          cmdMgr->regCmd("DTPrint", 3, dtPrintCmd()))) {
        cerr << "Registering \"device topology\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validDeviceId = {
    [](ArgType<size_t>& arg) {
        return [&arg]() {
            return deviceMgr->isID(arg.getValue());
        };
    },
    [](ArgType<size_t> const& arg) {
        return [&arg]() {
            cerr << "Error: Device " << arg.getValue() << " does not exist!!\n";
        };
    }};

unique_ptr<ArgParseCmdType> dtCheckOutCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTCHeckout");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("checkout to Device <id> in DeviceMgr");

        parser.addArgument<size_t>("id")
            .constraint(validDeviceId)
            .help("the ID of the device");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        deviceMgr->checkout2Device(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> dtResetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTReset");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("reset DeviceMgr");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        deviceMgr->reset();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> dtDeleteCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTDelete");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("remove a Device from DeviceMgr");

        parser.addArgument<size_t>("id")
            .constraint(validDeviceId)
            .help("the ID of the device");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        deviceMgr->removeDevice(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> dtGraphReadCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTGRead");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("read a device topology");

        parser.addArgument<string>("filepath")
            .help("the filepath to device file");

        parser.addArgument<bool>("-replace")
            .action(storeTrue)
            .help("if specified, replace the current device; otherwise store to a new one");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        Device bufferTopo = Device(0);
        string filepath = parser["filepath"];
        bool replace = parser["-replace"];

        if (!bufferTopo.readDevice(filepath)) {
            cerr << "Error: the format in \"" << filepath << "\" has something wrong!!" << endl;
            return CMD_EXEC_ERROR;
        }

        if (deviceMgr->getDTListItr() == deviceMgr->getDeviceList().end()) {
            deviceMgr->addDevice(deviceMgr->getNextID());
        } else {
            if (replace) {
                if (verbose >= 1) cout << "Note: original Device is replaced..." << endl;
            } else {
                deviceMgr->addDevice(deviceMgr->getNextID());
            }
        }

        deviceMgr->setDevice(bufferTopo);
        return CMD_EXEC_DONE;
    };

    return cmd;
}

// unique_ptr<ArgParseCmdType> dtGraphPrintCmd() {
//     auto cmd = make_unique<ArgParseCmdType>("DTGPrint");

//     cmd->parserDefinition = [](ArgumentParser& parser) {
//         parser.help("print info of device topology");

//         auto mutex = parser.addMutuallyExclusiveGroup();

//         mutex.addArgument<bool>("-summary")
//             .action(storeTrue)
//             .help("summary of the device graph");

//         mutex.addArgument<bool>("-edges")
//             .action(storeTrue)
//             .help("edges of the device graph");

//         mutex.addArgument<bool>("-path")
//             .action(storeTrue)
//             .help("path of the device graph");

//         mutex.addArgument<bool>("-qubit")
//             .action(storeTrue)
//             .help("qubit of the device graph");
//     };

//     cmd->onParseSuccess = [](ArgumentParser const& parser) {

//         return CMD_EXEC_DONE;
//     };

//     return cmd;
// }

unique_ptr<ArgParseCmdType> dtPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTPrint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info of DeviceMgr");

        auto mutex = parser.addMutuallyExclusiveGroup();

        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print summary of all devices");
        mutex.addArgument<bool>("-focus")
            .action(storeTrue)
            .help("print the info of device in focus");
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of devices");
        mutex.addArgument<bool>("-number")
            .action(storeTrue)
            .help("print number of devices");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["-focus"].isParsed())
            deviceMgr->printDeviceListItr();
        else if (parser["-list"].isParsed())
            deviceMgr->printDeviceList();
        else if (parser["-number"].isParsed())
            deviceMgr->printDeviceListSize();
        else
            deviceMgr->printDeviceMgr();

        return CMD_EXEC_DONE;
    };

    return cmd;
}

//-----------------------------------------------------------------------------------------------------------
//    DTGPrint [-Summary | -Edges | -Path | -Qubit]
//-----------------------------------------------------------------------------------------------------------
CmdExecStatus
DeviceGraphPrintCmd::exec(const string& option) {
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
