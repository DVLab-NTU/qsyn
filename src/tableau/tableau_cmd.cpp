/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_cmd.hpp"

#include "./tableau.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "argparse/argument.hpp"
#include "cli/cli.hpp"
#include "tableau/tableau_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/text_format.hpp"

using namespace dvlab::argparse;

namespace qsyn::experimental {

ArgType<size_t>::ConstraintType valid_tableau_qubit_id(TableauMgr const& tableau_mgr) {
    return [&tableau_mgr](size_t const& id) -> bool {
        if (id < tableau_mgr.get()->n_qubits()) return true;
        spdlog::error("Qubit {} does not exist in Tableau {}!!", id, tableau_mgr.focused_id());
        return false;
    };
}

dvlab::Command tableau_new_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "new",
        [&](ArgumentParser& parser) {
            parser.description("Create a new tableau");

            parser.add_argument<size_t>("n_qubits")
                .help("Number of qubits");

            parser.add_argument<size_t>("id")
                .nargs(NArgsOption::optional)
                .help(fmt::format("the ID of the Tableau"));

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help(fmt::format("if specified, replace the current Tableau; otherwise create a new one"));
        },
        [&](ArgumentParser const& parser) {
            auto const n_qubits = parser.get<size_t>("n_qubits");

            auto id = parser.parsed("id") ? parser.get<size_t>("id") : tableau_mgr.get_next_id();

            if (tableau_mgr.is_id(id)) {
                if (!parser.parsed("--replace")) {
                    spdlog::error("Tableau {} already exists!! Please specify `--replace` to replace if needed", id);
                    return dvlab::CmdExecResult::error;
                }
                tableau_mgr.set_by_id(id, std::make_unique<StabilizerTableau>(n_qubits));
                return dvlab::CmdExecResult::done;
            }

            tableau_mgr.add(id, std::make_unique<StabilizerTableau>(n_qubits));

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_equivalence_cmd(TableauMgr& tableau_mgr) {
    return {
        "equiv",
        [&](ArgumentParser& parser) {
            parser.description("check the equivalency of two stored tensors");

            parser.add_argument<size_t>("ids")
                .nargs(1, 2)
                .constraint(dvlab::utils::valid_mgr_id(tableau_mgr))
                .help("Compare the two Tableaus. If only one is specified, compare with the Tableau on focus");
        },
        [&](ArgumentParser const& parser) {
            auto const ids = parser.get<std::vector<size_t>>("ids");

            bool is_equiv = std::invoke([&]() {
                if (ids.size() == 1) {
                    return *tableau_mgr.get() == *tableau_mgr.find_by_id(ids[0]);
                } else {
                    return *tableau_mgr.find_by_id(ids[0]) == *tableau_mgr.find_by_id(ids[1]);
                }
            });

            if (is_equiv) {
                fmt::println("{}", dvlab::fmt_ext::styled_if_ansi_supported("Equivalent", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            } else {
                fmt::println("{}", dvlab::fmt_ext::styled_if_ansi_supported("Not Equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            }
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_apply_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "apply",
        [&](ArgumentParser& parser) {
            parser.description("Apply a gate to a tableau");

            auto mutex = parser.add_mutually_exclusive_group().required(true);

            mutex.add_argument<size_t>("-H")
                .metavar("q-id")
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply H gate to the qubit");

            mutex.add_argument<size_t>("-s")
                .metavar("q-id")
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply S gate to the qubit");

            mutex.add_argument<size_t>("-sdg")
                .metavar("q-id")
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply S† gate to the qubit");

            mutex.add_argument<size_t>("-cx")
                .metavar("q-id")
                .nargs(2)
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply CX gate to the qubit");

            mutex.add_argument<size_t>("-cz")
                .metavar("q-id")
                .nargs(2)
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply CZ gate to the qubit");

            mutex.add_argument<size_t>("-x")
                .metavar("q-id")
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply X gate to the qubit");

            mutex.add_argument<size_t>("-y")
                .metavar("q-id")
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply Y gate to the qubit");

            mutex.add_argument<size_t>("-z")
                .metavar("q-id")
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("Apply Z gate to the qubit");
        },
        [&](ArgumentParser const& parser) {
            if (parser.parsed("-H")) {
                tableau_mgr.get()->h(parser.get<size_t>("-H"));
            } else if (parser.parsed("-s")) {
                tableau_mgr.get()->s(parser.get<size_t>("-s"));
            } else if (parser.parsed("-sdg")) {
                tableau_mgr.get()->sdg(parser.get<size_t>("-sdg"));
            } else if (parser.parsed("-cx")) {
                auto const qubits = parser.get<std::vector<size_t>>("-cx");
                if (qubits[0] == qubits[1]) {
                    spdlog::error("Control and target qubits cannot be the same!!");
                    return dvlab::CmdExecResult::error;
                }
                tableau_mgr.get()->cx(qubits[0], qubits[1]);
            } else if (parser.parsed("-cz")) {
                auto const qubits = parser.get<std::vector<size_t>>("-cz");
                if (qubits[0] == qubits[1]) {
                    spdlog::error("Control and target qubits cannot be the same!!");
                    return dvlab::CmdExecResult::error;
                }
                tableau_mgr.get()->cz(qubits[0], qubits[1]);
            } else if (parser.parsed("-x")) {
                tableau_mgr.get()->x(parser.get<size_t>("-x"));
            } else if (parser.parsed("-y")) {
                tableau_mgr.get()->y(parser.get<size_t>("-y"));
            } else if (parser.parsed("-z")) {
                tableau_mgr.get()->z(parser.get<size_t>("-z"));
            } else {
                spdlog::error("Illegal gate type!!");
                return dvlab::CmdExecResult::error;
            }
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_print_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "print",
        [&](ArgumentParser& parser) {
            parser.description("Print the tableau");
        },
        [&](ArgumentParser const& /*parser*/) {
            fmt::println("{}", tableau_mgr.get()->to_string());
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_cmd(TableauMgr& tableau_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(tableau_mgr);

    cmd.add_subcommand(dvlab::utils::mgr_list_cmd(tableau_mgr));
    cmd.add_subcommand(tableau_new_cmd(tableau_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_delete_cmd(tableau_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_checkout_cmd(tableau_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_copy_cmd(tableau_mgr));
    cmd.add_subcommand(tableau_apply_cmd(tableau_mgr));
    cmd.add_subcommand(tableau_print_cmd(tableau_mgr));
    cmd.add_subcommand(tableau_equivalence_cmd(tableau_mgr));

    return cmd;
}

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr) {
    return cli.add_command(tableau_cmd(tableau_mgr));
}

}  // namespace qsyn::experimental
