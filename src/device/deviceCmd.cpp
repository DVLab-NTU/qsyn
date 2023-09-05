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
#include "fmt/core.h"

using namespace std;
using namespace ArgParse;

DeviceMgr deviceMgr{"Device"};

Command dtCheckOutCmd();
Command dtResetCmd();
Command dtDeleteCmd();
Command dtGraphReadCmd();
Command dtGraphPrintCmd();
Command dtPrintCmd();

bool deviceMgrNotEmpty() {
    return dvlab::utils::expect(!deviceMgr.empty(), "Device list is empty now. Please DTRead first.");
}

bool initDeviceCmd() {
    if (!(cli.registerCommand("dtcheckout", 4, dtCheckOutCmd()) &&
          cli.registerCommand("dtreset", 3, dtResetCmd()) &&
          cli.registerCommand("dtdelete", 3, dtDeleteCmd()) &&
          cli.registerCommand("dtgread", 4, dtGraphReadCmd()) &&
          cli.registerCommand("dtgprint", 4, dtGraphPrintCmd()) &&
          cli.registerCommand("dtprint", 3, dtPrintCmd()))) {
        logger.fatal("Registering \"device topology\" commands fails... exiting");
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType const validDeviceId =
    [](size_t const& id) {
        if (deviceMgr.isID(id)) return true;
        logger.error("Device {} does not exist!!", id);
        return false;
    };

Command dtCheckOutCmd() {
    return {"dtcheckout",
            [](ArgumentParser& parser) {
                parser.description("checkout to Device <id> in DeviceMgr");

                parser.addArgument<size_t>("id")
                    .constraint(validDeviceId)
                    .help("the ID of the device");
            },
            [](ArgumentParser const& parser) {
                deviceMgr.checkout(parser.get<size_t>("id"));
                return CmdExecResult::DONE;
            }};
}

Command dtResetCmd() {
    return {"dtreset",
            [](ArgumentParser& parser) {
                parser.description("reset DeviceMgr");
            },
            [](ArgumentParser const& parser) {
                deviceMgr.reset();
                return CmdExecResult::DONE;
            }};
    auto cmd = make_unique<Command>("DTReset");
}

Command dtDeleteCmd() {
    return {"dtdelete",
            [](ArgumentParser& parser) {
                parser.description("remove a Device from DeviceMgr");

                parser.addArgument<size_t>("id")
                    .constraint(validDeviceId)
                    .help("the ID of the device");
            },
            [](ArgumentParser const& parser) {
                deviceMgr.remove(parser.get<size_t>("id"));
                return CmdExecResult::DONE;
            }};
}

Command dtGraphReadCmd() {
    return {"dtgread",
            [](ArgumentParser& parser) {
                parser.description("read a device topology");

                parser.addArgument<string>("filepath")
                    .help("the filepath to device file");

                parser.addArgument<bool>("-replace")
                    .action(storeTrue)
                    .help("if specified, replace the current device; otherwise store to a new one");
            },
            [](ArgumentParser const& parser) {
                Device bufferDevice;
                auto filepath = parser.get<string>("filepath");
                auto replace = parser.get<bool>("-replace");

                if (!bufferDevice.readDevice(filepath)) {
                    logger.error("the format in \"{}\" has something wrong!!", filepath);
                    return CmdExecResult::ERROR;
                }

                if (deviceMgr.empty() || !replace) {
                    deviceMgr.add(deviceMgr.getNextID(), std::make_unique<Device>(std::move(bufferDevice)));
                } else {
                    deviceMgr.set(std::make_unique<Device>(std::move(bufferDevice)));
                }

                return CmdExecResult::DONE;
            }};
}

Command dtPrintCmd() {
    return {"dtprint",
            [](ArgumentParser& parser) {
                parser.description("print info about Devices");

                auto mutex = parser.addMutuallyExclusiveGroup();

                mutex.addArgument<bool>("-focus")
                    .action(storeTrue)
                    .help("print the info of device in focus");
                mutex.addArgument<bool>("-list")
                    .action(storeTrue)
                    .help("print a list of devices");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("-focus"))
                    deviceMgr.printFocus();
                else if (parser.parsed("-list"))
                    deviceMgr.printList();
                else
                    deviceMgr.printManager();

                return CmdExecResult::DONE;
            }};
}

//-----------------------------------------------------------------------------------------------------------
//    DTGPrint [-Summary | -Edges | -Path | -Qubit]
//-----------------------------------------------------------------------------------------------------------

Command dtGraphPrintCmd() {
    return {"dtgprint",
            [](ArgumentParser& parser) {
                parser.description("print info of device topology");

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
            },
            [](ArgumentParser const& parser) {
                if (!deviceMgrNotEmpty()) return CmdExecResult::ERROR;

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
            }};
}
