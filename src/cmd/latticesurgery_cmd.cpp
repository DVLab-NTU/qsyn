/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define latticesurgery package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "cmd/latticesurgery_cmd.hpp"

#include <fmt/color.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>


#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "cmd/latticesurgery_mgr.hpp"
#include "latticesurgery/latticesurgery.hpp"
#include "latticesurgery/latticesurgery_gate.hpp"
#include "latticesurgery/latticesurgery_io.hpp"
#include "util/cin_cout_cerr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"
#include "qsyn/qsyn_type.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::latticesurgery {

std::function<bool(size_t const&)> valid_latticesurgery_id(LatticeSurgeryMgr const& ls_mgr) {
    return [&](size_t const& id) {
        if (ls_mgr.get() && ls_mgr.is_id(id))
            return true;
        spdlog::error("LatticeSurgery {} does not exist!!", id);
        return false;
    };
}

std::function<bool(size_t const&)>
valid_latticesurgery_gate_id(LatticeSurgeryMgr const& ls_mgr) {
    return [&](size_t const& id) {
        if (!dvlab::utils::mgr_has_data(ls_mgr))
            return false;
        if (ls_mgr.get() && ls_mgr.get()->get_gate(id) != nullptr)
            return true;
        spdlog::error("Gate ID {} does not exist!!", id);
        return false;
    };
}

std::function<bool(QubitIdType const&)>
valid_latticesurgery_qubit_id(LatticeSurgeryMgr const& ls_mgr) {
    return [&](QubitIdType const& id) {
        if (!dvlab::utils::mgr_has_data(ls_mgr))
            return false;
        if (ls_mgr.get() && id < ls_mgr.get()->get_num_qubits())
            return true;
        spdlog::error("Qubit ID {} does not exist!!", id);
        return false;
    };
}

dvlab::Command latticesurgery_read_cmd(LatticeSurgeryMgr& ls_mgr) {
    return {
        "read",
        [](ArgumentParser& parser) {
            parser.description(
                "read a lattice surgery circuit and construct the corresponding netlist");

            parser.add_argument<std::string>("filepath")
                .constraint(path_readable)
                .constraint(allowed_extension({".ls"}))
                .help(
                    "the filepath to lattice surgery circuit file. Supported extension: .ls");

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help(
                    "if specified, replace the current circuit; otherwise store "
                    "to a new one");
        },
        [&](ArgumentParser const& parser) {
            auto const filepath = parser.get<std::string>("filepath");
            auto replace = parser.get<bool>("--replace");
            auto ls = from_file(filepath);
            if (!ls) {
                fmt::println("Error: the format in \"{}\" has something wrong!!",
                             filepath);
                return CmdExecResult::error;
            }
            if (ls_mgr.empty() || !replace) {
                ls_mgr.add(ls_mgr.get_next_id(),
                             std::make_unique<LatticeSurgery>(std::move(*ls)));
            } else {
                ls_mgr.set(std::make_unique<LatticeSurgery>(std::move(*ls)));
            }
            ls_mgr.get()->set_filename(std::filesystem::path{filepath}.stem());
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_write_cmd(LatticeSurgeryMgr const& ls_mgr) {
    return {
        "write",
        [](ArgumentParser& parser) {
            parser.description("write LatticeSurgery circuit to a file");

            parser.add_argument<std::string>("output_path")
                .nargs(NArgsOption::optional)
                .constraint(path_writable)
                .constraint(allowed_extension({".ls"}))
                .help(
                    "the filepath to output file. Supported extension: .ls. If "
                    "not specified, the result will be dumped to the terminal");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(ls_mgr))
                return CmdExecResult::error;

            auto const output_path = parser.get<std::string>("output_path");
            if (output_path.empty()) {
                ls_mgr.get()->print_ls();
            } else {
                if (!ls_mgr.get()->write_ls(output_path)) {
                    spdlog::error("Failed to write to file: {}", output_path);
                    return CmdExecResult::error;
                }
            }
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_print_cmd(LatticeSurgeryMgr const& ls_mgr) {
    return {
        "print",
        [](ArgumentParser& parser) {
            parser.description("print the LatticeSurgery circuit");

            parser.add_argument<bool>("-v", "--verbose")
                .action(store_true)
                .help("display more information");

            auto mutex = parser.add_mutually_exclusive_group();

            mutex.add_argument<bool>("-n", "--neighbors")
                .action(store_true)
                .help("print gate neighbors");

            mutex.add_argument<size_t>("-g", "--gate")
                .nargs(NArgsOption::zero_or_more)
                .help("print information for the gates with the specified IDs. If the ID is not specified, print all gates");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(ls_mgr))
                return CmdExecResult::error;

            if (parser.parsed("--gate")) {
                auto gate_ids = parser.get<std::vector<size_t>>("--gate");
                ls_mgr.get()->print_gates(parser.parsed("--verbose"), std::span{gate_ids});
            } else {
                ls_mgr.get()->print_ls_info();
            }
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_gate_add_cmd(LatticeSurgeryMgr& ls_mgr) {
    return {
        "add",
        [&](ArgumentParser& parser) {
            parser.description("add a gate to the LatticeSurgery circuit");

            parser.add_argument<std::string>("type")
                .constraint(choices_allow_prefix({"merge", "split"}))
                .help("the type of gate to add");

            parser.add_argument<QubitIdType>("qubits")
                .nargs(NArgsOption::zero_or_more)
                .constraint(valid_latticesurgery_qubit_id(ls_mgr))
                .help("the qubits to merge/split");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(ls_mgr))
                return CmdExecResult::error;

            auto const type = parser.get<std::string>("type");
            auto const qubits = parser.get<QubitIdList>("qubits");

            if (!LatticeSurgeryGate::qubit_id_is_unique(qubits)) {
                spdlog::error("Qubits must be unique!!");
                return CmdExecResult::error;
            }

            LatticeSurgeryOpType op_type;
            if (dvlab::str::is_prefix_of(type, "merge")) {
                op_type = LatticeSurgeryOpType::merge;
            } else if (dvlab::str::is_prefix_of(type, "split")) {
                op_type = LatticeSurgeryOpType::split;
            } else {
                spdlog::error("Invalid gate type: {}", type);
                return CmdExecResult::error;
            }

            LatticeSurgeryGate gate(op_type, qubits);
            ls_mgr.get()->append(gate);
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_gate_delete_cmd(LatticeSurgeryMgr& ls_mgr) {
    return {
        "delete",
        [&](ArgumentParser& parser) {
            parser.description("delete a gate from the LatticeSurgery circuit");

            parser.add_argument<size_t>("id")
                .constraint(valid_latticesurgery_gate_id(ls_mgr))
                .help("the ID of the gate to delete");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(ls_mgr))
                return CmdExecResult::error;

            auto const id = parser.get<size_t>("id");
            if (!ls_mgr.get()->remove_gate(id)) {
                spdlog::error("Failed to delete gate {}", id);
                return CmdExecResult::error;
            }
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_gate_cmd(LatticeSurgeryMgr& ls_mgr) {
    auto cmd = Command{
        "gate",
        [](ArgumentParser& parser) {
            parser.description("gate operations");

            parser.add_subparsers("gate-cmd").required(true);
        },
        [](ArgumentParser const& /*parser*/) { return CmdExecResult::error; }};

    cmd.add_subcommand("gate-cmd", latticesurgery_gate_add_cmd(ls_mgr));
    cmd.add_subcommand("gate-cmd", latticesurgery_gate_delete_cmd(ls_mgr));

    return cmd;
}

dvlab::Command latticesurgery_qubit_add_cmd(LatticeSurgeryMgr& ls_mgr) {
    return {
        "add",
        [](ArgumentParser& parser) {
            parser.description("add qubits to the LatticeSurgery circuit");

            parser.add_argument<size_t>("num")
                .help("the number of qubits to add");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(ls_mgr))
                return CmdExecResult::error;

            auto const num = parser.get<size_t>("num");
            ls_mgr.get()->add_qubits(num);
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_qubit_delete_cmd(LatticeSurgeryMgr& ls_mgr) {
    return {
        "delete",
        [&](ArgumentParser& parser) {
            parser.description("delete a qubit from the LatticeSurgery circuit");

            parser.add_argument<QubitIdType>("id")
                .constraint(valid_latticesurgery_qubit_id(ls_mgr))
                .help("the ID of the qubit to delete");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(ls_mgr))
                return CmdExecResult::error;

            auto const id = parser.get<QubitIdType>("id");
            if (!ls_mgr.get()->remove_qubit(id)) {
                spdlog::error("Failed to delete qubit {}", id);
                return CmdExecResult::error;
            }
            return CmdExecResult::done;
        }};
}

dvlab::Command latticesurgery_qubit_cmd(LatticeSurgeryMgr& ls_mgr) {
    auto cmd = Command{
        "qubit",
        [](ArgumentParser& parser) {
            parser.description("qubit operations");

            parser.add_subparsers("qubit-cmd").required(true);
        },
        [](ArgumentParser const& /*parser*/) { return CmdExecResult::error; }};

    cmd.add_subcommand("qubit-cmd", latticesurgery_qubit_add_cmd(ls_mgr));
    cmd.add_subcommand("qubit-cmd", latticesurgery_qubit_delete_cmd(ls_mgr));

    return cmd;
}

dvlab::Command latticesurgery_cmd(LatticeSurgeryMgr& ls_mgr) {
    auto cmd = Command{
        "ls",
        [](ArgumentParser& parser) {
            parser.description("lattice surgery circuit operations");

            parser.add_subparsers("ls-cmd-group").required(true);
        },
        [](ArgumentParser const& /*parser*/) { return CmdExecResult::error; }};

    cmd.add_subcommand("ls-cmd-group", dvlab::utils::mgr_list_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", dvlab::utils::mgr_checkout_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", dvlab::utils::mgr_new_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", dvlab::utils::mgr_delete_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", dvlab::utils::mgr_copy_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", latticesurgery_read_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", latticesurgery_write_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", latticesurgery_print_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", latticesurgery_gate_cmd(ls_mgr));
    cmd.add_subcommand("ls-cmd-group", latticesurgery_qubit_cmd(ls_mgr));

    return cmd;
}

bool add_latticesurgery_cmds(dvlab::CommandLineInterface& cli, LatticeSurgeryMgr& ls_mgr) {
    if (!cli.add_command(latticesurgery_cmd(ls_mgr))) {
        return false;
    }
    return true;
}

}  // namespace qsyn::latticesurgery 