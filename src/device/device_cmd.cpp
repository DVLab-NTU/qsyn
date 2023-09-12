/****************************************************************************
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
#include "device/device_mgr.hpp"
#include "fmt/core.h"

using namespace std;
using namespace argparse;

DeviceMgr DEVICE_MGR{"Device"};

Command device_checkout_cmd();
Command device_mgr_reset_cmd();
Command device_delete_cmd();
Command device_graph_read_cmd();
Command device_graph_print_cmd();
Command device_mgr_print_cmd();

bool device_mgr_not_empty() {
    return dvlab::utils::expect(!DEVICE_MGR.empty(), "Device list is empty now. Please DTRead first.");
}

bool add_device_cmds() {
    if (!(CLI.add_command(device_checkout_cmd()) &&
          CLI.add_command(device_mgr_reset_cmd()) &&
          CLI.add_command(device_delete_cmd()) &&
          CLI.add_command(device_graph_read_cmd()) &&
          CLI.add_command(device_graph_print_cmd()) &&
          CLI.add_command(device_mgr_print_cmd()))) {
        LOGGER.fatal("Registering \"device topology\" commands fails... exiting");
        return false;
    }
    return true;
}

bool valid_device_id(size_t const& id) {
    if (DEVICE_MGR.is_id(id)) return true;
    LOGGER.error("Device {} does not exist!!", id);
    return false;
};

Command device_checkout_cmd() {
    return {"dtcheckout",
            [](ArgumentParser& parser) {
                parser.description("checkout to Device <id> in DeviceMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_device_id)
                    .help("the ID of the device");
            },
            [](ArgumentParser const& parser) {
                DEVICE_MGR.checkout(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

Command device_mgr_reset_cmd() {
    return {"dtreset",
            [](ArgumentParser& parser) {
                parser.description("reset DeviceMgr");
            },
            [](ArgumentParser const& /*parser*/) {
                DEVICE_MGR.reset();
                return CmdExecResult::done;
            }};
    auto cmd = make_unique<Command>("DTReset");
}

Command device_delete_cmd() {
    return {"dtdelete",
            [](ArgumentParser& parser) {
                parser.description("remove a Device from DeviceMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_device_id)
                    .help("the ID of the device");
            },
            [](ArgumentParser const& parser) {
                DEVICE_MGR.remove(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

Command device_graph_read_cmd() {
    return {"dtgread",
            [](ArgumentParser& parser) {
                parser.description("read a device topology");

                parser.add_argument<string>("filepath")
                    .help("the filepath to device file");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .help("if specified, replace the current device; otherwise store to a new one");
            },
            [](ArgumentParser const& parser) {
                Device buffer_device;
                auto filepath = parser.get<string>("filepath");
                auto replace = parser.get<bool>("-replace");

                if (!buffer_device.read_device(filepath)) {
                    LOGGER.error("the format in \"{}\" has something wrong!!", filepath);
                    return CmdExecResult::error;
                }

                if (DEVICE_MGR.empty() || !replace) {
                    DEVICE_MGR.add(DEVICE_MGR.get_next_id(), std::make_unique<Device>(std::move(buffer_device)));
                } else {
                    DEVICE_MGR.set(std::make_unique<Device>(std::move(buffer_device)));
                }

                return CmdExecResult::done;
            }};
}

Command device_mgr_print_cmd() {
    return {"dtprint",
            [](ArgumentParser& parser) {
                parser.description("print info about Devices");

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<bool>("-focus")
                    .action(store_true)
                    .help("print the info of device in focus");
                mutex.add_argument<bool>("-list")
                    .action(store_true)
                    .help("print a list of devices");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("-focus"))
                    DEVICE_MGR.print_focus();
                else if (parser.parsed("-list"))
                    DEVICE_MGR.print_list();
                else
                    DEVICE_MGR.print_manager();

                return CmdExecResult::done;
            }};
}

//-----------------------------------------------------------------------------------------------------------
//    DTGPrint [-Summary | -Edges | -Path | -Qubit]
//-----------------------------------------------------------------------------------------------------------

Command device_graph_print_cmd() {
    return {"dtgprint",
            [](ArgumentParser& parser) {
                parser.description("print info of device topology");

                auto mutex = parser.add_mutually_exclusive_group().required(false);

                mutex.add_argument<bool>("-summary")
                    .action(store_true)
                    .help("print basic information of the topology");

                mutex.add_argument<size_t>("-edges")
                    .nargs(0, 2)
                    .help(
                        "print information of edges. "
                        "If no qubit ID is specified, print for all edges; "
                        "if one qubit ID specified, list the adjacent edges to the qubit; "
                        "if two qubit IDs are specified, list the edge between them");

                mutex.add_argument<size_t>("-qubits")
                    .nargs(NArgsOption::zero_or_more)
                    .help(
                        "print information of qubits. "
                        "If no qubit ID is specified, print for all qubits;"
                        "otherwise, print information of the specified qubit IDs");

                mutex.add_argument<size_t>("-path")
                    .nargs(2)
                    .metavar("(q1, q2)")
                    .help(
                        "print routing paths between q1 and q2");
            },
            [](ArgumentParser const& parser) {
                if (!device_mgr_not_empty()) return CmdExecResult::error;

                if (parser.parsed("-edges")) {
                    DEVICE_MGR.get()->print_edges(parser.get<vector<size_t>>("-edges"));
                    return CmdExecResult::done;
                }
                if (parser.parsed("-qubits")) {
                    DEVICE_MGR.get()->print_qubits(parser.get<vector<size_t>>("-qubits"));
                    return CmdExecResult::done;
                }
                if (parser.parsed("-path")) {
                    auto qids = parser.get<vector<size_t>>("-path");
                    DEVICE_MGR.get()->print_path(qids[0], qids[1]);
                    return CmdExecResult::done;
                }

                DEVICE_MGR.get()->print_topology();
                return CmdExecResult::done;
            }};
}
