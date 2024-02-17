/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_cmd.hpp"

#include "./stabilizer_tableau.hpp"
#include "./tableau_optimization.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "argparse/argument.hpp"
#include "cli/cli.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_mgr.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/tableau_mgr.hpp"
#include "tensor/qtensor.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"
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
                tableau_mgr.set_by_id(id, std::make_unique<Tableau>(n_qubits));
                return dvlab::CmdExecResult::done;
            }

            tableau_mgr.add(id, std::make_unique<Tableau>(n_qubits));

            return dvlab::CmdExecResult::done;
        }};
}

// dvlab::Command tableau_equivalence_cmd(TableauMgr& tableau_mgr) {
//     return {
//         "equiv",
//         [&](ArgumentParser& parser) {
//             parser.description("check the equivalency of two stored tensors");

//             parser.add_argument<size_t>("ids")
//                 .nargs(1, 2)
//                 .constraint(dvlab::utils::valid_mgr_id(tableau_mgr))
//                 .help("Compare the two Tableaus. If only one is specified, compare with the Tableau on focus");
//         },
//         [&](ArgumentParser const& parser) {
//             if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
//             return dvlab::CmdExecResult::error;
//             }
//             auto const ids = parser.get<std::vector<size_t>>("ids");

//             bool const is_equiv = std::invoke([&]() {
//                 if (ids.size() == 1) {
//                     return *tableau_mgr.get() == *tableau_mgr.find_by_id(ids[0]);
//                 } else {
//                     return *tableau_mgr.find_by_id(ids[0]) == *tableau_mgr.find_by_id(ids[1]);
//                 }
//             });

//             if (is_equiv) {
//                 fmt::println("{}", dvlab::fmt_ext::styled_if_ansi_supported("Equivalent", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
//             } else {
//                 fmt::println("{}", dvlab::fmt_ext::styled_if_ansi_supported("Not Equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
//             }
//             return dvlab::CmdExecResult::done;
//         }};
// }

dvlab::Command tableau_apply_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "apply",
        [&](ArgumentParser& parser) {
            parser.description("Apply a gate to a tableau");

            parser.add_argument<std::string>("gate-type")
                .help("The gate type to be applied");

            parser.add_argument<size_t>("qubits")
                .nargs(1, 2)
                .constraint(valid_tableau_qubit_id(tableau_mgr))
                .help("The qubits to apply the gate to");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            auto const type   = to_clifford_operator_type(parser.get<std::string>("gate-type"));
            auto const qubits = parser.get<std::vector<size_t>>("qubits");

            if (!type) {
                spdlog::error("Unknown gate type {}!!", parser.get<std::string>("gate-type"));
                return dvlab::CmdExecResult::error;
            }

            if (type == CliffordOperatorType::cx ||
                type == CliffordOperatorType::cz ||
                type == CliffordOperatorType::swap) {
                if (qubits.size() != 2) {
                    spdlog::error("The gate {} requires specifying exactly 2 qubit indices!!", to_string(type.value()));
                    return dvlab::CmdExecResult::error;
                }

                if (qubits[0] == qubits[1]) {
                    spdlog::error("The two qubits cannot be the same!!");
                    return dvlab::CmdExecResult::error;
                }
            } else {
                if (qubits.size() != 1) {
                    spdlog::error("The gate {} requires specifying exactly 1 qubit index!!", to_string(type.value()));
                    return dvlab::CmdExecResult::error;
                }
            }

            auto qubit_array = std::array<size_t, 2>{};

            std::copy(qubits.begin(), qubits.end(), qubit_array.begin());

            tableau_mgr.get()->apply(CliffordOperator{type.value(), qubit_array});

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_print_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "print",
        [&](ArgumentParser& parser) {
            parser.description("Print the tableau");

            auto mutex = parser.add_mutually_exclusive_group().required(false);

            mutex.add_argument<bool>("-b", "--bit")
                .action(store_true)
                .help("Print the tableau in bit string format");

            mutex.add_argument<bool>("-c", "--char")
                .action(store_true)
                .help("Print the tableau in character format");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }
            if (parser.parsed("-b")) {
                fmt::println("{:b}", *tableau_mgr.get());
            } else {
                fmt::println("{:c}", *tableau_mgr.get());
            }
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_adjoint_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "adjoint",
        [&](ArgumentParser& parser) {
            parser.description("transform the tableau to its adjoint");
        },
        [&](ArgumentParser const& /* parser */) {
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }
            adjoint_inplace(*tableau_mgr.get());
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_optimization_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "optimize",
        [&](ArgumentParser& parser) {
            parser.description("Optimize the tableau");

            parser.add_argument<std::string>("method")
                .constraint(choices_allow_prefix({"collapse", "merge-t", "internal-h-opt"}))
                .help("The optimization method to be used");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            enum struct OptimizationMethod {
                collapse,
                merge_t,
                internal_h_opt
            };

            auto const method_str = parser.get<std::string>("method");

            auto method = std::invoke([&]() -> std::optional<OptimizationMethod> {
                if (dvlab::str::is_prefix_of(method_str, "collapse")) {
                    return OptimizationMethod::collapse;
                } else if (dvlab::str::is_prefix_of(method_str, "merge-t")) {
                    return OptimizationMethod::merge_t;
                } else if (dvlab::str::is_prefix_of(method_str, "internal-h-opt")) {
                    return OptimizationMethod::internal_h_opt;
                }
                return std::nullopt;
            });

            if (!method) {
                spdlog::error("Unknown optimization method {}!!", method_str);
                return dvlab::CmdExecResult::error;
            }

            switch (method.value()) {
                case OptimizationMethod::collapse:
                    collapse(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("collapse");
                    break;
                case OptimizationMethod::merge_t:
                    merge_rotations(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("MergeT");
                    break;
                case OptimizationMethod::internal_h_opt:
                    *tableau_mgr.get() = minimize_internal_hadamards(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("InternalHOpt");
                    break;
            }

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
    cmd.add_subcommand(tableau_adjoint_cmd(tableau_mgr));
    cmd.add_subcommand(tableau_print_cmd(tableau_mgr));
    cmd.add_subcommand(tableau_optimization_cmd(tableau_mgr));

    return cmd;
}

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr) {
    return cli.add_command(tableau_cmd(tableau_mgr));
}

}  // namespace qsyn::experimental
