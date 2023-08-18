/****************************************************************************
  FileName     [ deviceCmd.cpp ]
  PackageName  [ device ]
  Synopsis     [ Define device package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iomanip>
#include <memory>
#include <string>

#include "cli/cli.hpp"
#include "device/device.hpp"
#include "device/deviceMgr.hpp"

using namespace std;
using namespace ArgParse;

DeviceMgr deviceMgr{"Device"};
extern size_t verbose;
extern int effLimit;

unique_ptr<ArgParseCmdType> dtCheckOutCmd();
unique_ptr<ArgParseCmdType> dtResetCmd();
unique_ptr<ArgParseCmdType> dtDeleteCmd();
unique_ptr<ArgParseCmdType> dtGraphReadCmd();
unique_ptr<ArgParseCmdType> dtGraphPrintCmd();  // requires subparsers
unique_ptr<ArgParseCmdType> dtPrintCmd();

bool deviceMgrNotEmpty() {
    return dvlab::utils::expect(!deviceMgr.empty(), "Device list is empty now. Please DTRead first.");
}

bool initDeviceCmd() {
    if (!(cli.regCmd("DTCHeckout", 4, dtCheckOutCmd()) &&
          cli.regCmd("DTReset", 3, dtResetCmd()) &&
          cli.regCmd("DTDelete", 3, dtDeleteCmd()) &&
          cli.regCmd("DTGRead", 4, dtGraphReadCmd()) &&
          cli.regCmd("DTGPrint", 4, dtGraphPrintCmd()) &&
          cli.regCmd("DTPrint", 3, dtPrintCmd()))) {
        cerr << "Registering \"device topology\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validDeviceId = {
    [](size_t const& id) {
        return deviceMgr.isID(id);
    },
    [](size_t const& id) {
        cerr << "Error: Device " << id << " does not exist!!\n";
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
        deviceMgr.checkout(parser.get<size_t>("id"));
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> dtResetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTReset");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("reset DeviceMgr");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        deviceMgr.reset();
        return CmdExecResult::DONE;
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
        deviceMgr.remove(parser.get<size_t>("id"));
        return CmdExecResult::DONE;
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
        Device bufferDevice = Device(0);
        auto filepath = parser.get<string>("filepath");
        auto replace = parser.get<bool>("-replace");

        if (!bufferDevice.readDevice(filepath)) {
            cerr << "Error: the format in \"" << filepath << "\" has something wrong!!" << endl;
            return CmdExecResult::ERROR;
        }

        if (deviceMgr.empty()) {
            deviceMgr.add(deviceMgr.getNextID());
        } else {
            if (replace) {
                if (verbose >= 1) cout << "Note: original Device is replaced..." << endl;
            } else {
                deviceMgr.add(deviceMgr.getNextID());
            }
        }

        deviceMgr.set(std::make_unique<Device>(std::move(bufferDevice)));
        return CmdExecResult::DONE;
    };

    return cmd;
}

// unique_ptr<ArgParseCmdType> dtGraphPrintCmd() {
//     auto cmd = make_unique<ArgParseCmdType>("DTGPrint");

//     cmd->parserDefinition = [](ArgumentParser & parser) {
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

//         return CmdExecResult::DONE;
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
        if (parser.parsed("-focus"))
            deviceMgr.printFocus();
        else if (parser.parsed("-list"))
            deviceMgr.printList();
        else if (parser.parsed("-number"))
            deviceMgr.printListSize();
        else
            deviceMgr.printMgr();

        return CmdExecResult::DONE;
    };

    return cmd;
}

//-----------------------------------------------------------------------------------------------------------
//    DTGPrint [-Summary | -Edges | -Path | -Qubit]
//-----------------------------------------------------------------------------------------------------------

unique_ptr<ArgParseCmdType> dtGraphPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DTGPrint");

    cmd->precondition = deviceMgrNotEmpty;

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info of device topology");
        auto mutex = parser.addMutuallyExclusiveGroup().required(false);

        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print basic information of the topology");

        mutex.addArgument<size_t>("-edges")
            .nargs(0, 2)
            .help(
                "print information of edges. "
                "If no qubit ID is specified, print for all edges; "
                "if one qubit ID specified, list the adjacent edges to the qubit; "
                "if two qubit IDs are specified, list the edge between them");

        mutex.addArgument<size_t>("-qubits")
            .nargs(NArgsOption::ZERO_OR_MORE)
            .help(
                "print information of qubits. "
                "If no qubit ID is specified, print for all qubits;"
                "otherwise, print information of the specified qubit IDs");

        mutex.addArgument<size_t>("-path")
            .nargs(2)
            .metavar("(q1, q2)")
            .help(
                "print routing paths between q1 and q2");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.parsed("-edges")) {
            deviceMgr.get()->printEdges(parser.get<vector<size_t>>("-edges"));
            return CmdExecResult::DONE;
        }
        if (parser.parsed("-qubits")) {
            deviceMgr.get()->printQubits(parser.get<vector<size_t>>("-qubits"));
            return CmdExecResult::DONE;
        }
        if (parser.parsed("-path")) {
            auto qids = parser.get<vector<size_t>>("-path");
            deviceMgr.get()->printPath(qids[0], qids[1]);
            return CmdExecResult::DONE;
        }

        deviceMgr.get()->printTopology();
        return CmdExecResult::DONE;
    };

    return cmd;
}
