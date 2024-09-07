/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_cmd.hpp"

#include <cstdint>

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

            auto const id = parser.parsed("id") ? parser.get<size_t>("id") : tableau_mgr.get_next_id();

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

dvlab::Command tableau_append_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "append",
        [&](ArgumentParser& parser) {
            parser.description("Append a gate to a tableau");

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
                type == CliffordOperatorType::swap ||
                type == CliffordOperatorType::ecr) {
                if (qubits.size() != 2) {
                    spdlog::error("The gate {} requires specifying exactly 2 qubit indices!!", to_string(*type));
                    return dvlab::CmdExecResult::error;
                }

                if (qubits[0] == qubits[1]) {
                    spdlog::error("The two qubits cannot be the same!!");
                    return dvlab::CmdExecResult::error;
                }
            } else {
                if (qubits.size() != 1) {
                    spdlog::error("The gate {} requires specifying exactly 1 qubit index!!", to_string(*type));
                    return dvlab::CmdExecResult::error;
                }
            }

            auto qubit_array = std::array<size_t, 2>{};

            std::copy(qubits.begin(), qubits.end(), qubit_array.begin());

            tableau_mgr.get()->apply(CliffordOperator{*type, qubit_array});

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
                return dvlab::CmdExecResult::done;
            }
            if (parser.parsed("-c")) {
                fmt::println("{:c}", *tableau_mgr.get());
                return dvlab::CmdExecResult::done;
            }

            fmt::println("Tableau ({} qubits, {} Clifford segments, {} Pauli rotations)", tableau_mgr.get()->n_qubits(), tableau_mgr.get()->n_cliffords(), tableau_mgr.get()->n_pauli_rotations());
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

            auto methods = parser.add_subparsers("method").required(true);

            methods.add_parser("full")
                .description("Perform tmerge, hopt, phasepoly until the T-count stops decreasing");

            methods.add_parser("collapse")
                .description("Collapse the tableau into a canonical form");

            methods.add_parser("tmerge")
                .description("Merge rotations of the same rotation plane");

            methods.add_parser("hopt")
                .description("Minimize the number of Hadamard gates and internal Hadamard gates in the tableau");

            auto phasepoly_parser = methods.add_parser("phasepoly")
                                        .description("Reduce the number of terms for phase polynomials in the Tableau");

            phasepoly_parser.add_argument<std::string>("strategy")
                .default_value("todd")
                .constraint(choices_allow_prefix({"todd", "tohpe"}))
                .help("Phase polynomial optimization strategy");

            auto matpar_parser = methods.add_parser("matpar")
                                     .description("partition the Pauli rotations into simultaneously-implementable tableaux. This option requires all Pauli rotations to be diagonal");

            matpar_parser.add_argument<size_t>("-a", "--ancillae")
                .default_value(0)
                .help("The number of ancillae to be used in the partitioning");

            matpar_parser.add_argument<std::string>("strategy")
                .default_value("naive")
                .constraint(choices_allow_prefix({"naive"}))
                .help("Matroid partitioning strategy");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            auto const method_str = parser.get<std::string>("method");

            enum struct OptimizationMethod : std::uint8_t {
                full,
                collapse,
                t_merge,
                internal_h_opt,
                phase_polynomial_optimization,
                matroid_partition
            };

            auto method = std::invoke([&]() -> std::optional<OptimizationMethod> {
                if (dvlab::str::is_prefix_of(method_str, "full")) {
                    return OptimizationMethod::full;
                } else if (dvlab::str::is_prefix_of(method_str, "collapse")) {
                    return OptimizationMethod::collapse;
                } else if (dvlab::str::is_prefix_of(method_str, "tmerge")) {
                    return OptimizationMethod::t_merge;
                } else if (dvlab::str::is_prefix_of(method_str, "hopt")) {
                    return OptimizationMethod::internal_h_opt;
                } else if (dvlab::str::is_prefix_of(method_str, "phasepoly")) {
                    return OptimizationMethod::phase_polynomial_optimization;
                } else if (dvlab::str::is_prefix_of(method_str, "matpar")) {
                    return OptimizationMethod::matroid_partition;
                }
                return std::nullopt;
            });

            if (!method) {
                spdlog::error("Unknown optimization method {}!!", method_str);
                return dvlab::CmdExecResult::error;
            }

            auto const do_phase_polynomial_optimization = [&]() {
                auto const phasepoly_strategy_str = parser.get<std::string>("strategy");

                auto const phasepoly_strategy = std::invoke([&]() -> std::unique_ptr<PhasePolynomialOptimizationStrategy> {
                    if (dvlab::str::is_prefix_of(phasepoly_strategy_str, "todd")) {
                        return std::make_unique<ToddPhasePolynomialOptimizationStrategy>();
                    }
                    else if (dvlab::str::is_prefix_of(phasepoly_strategy_str, "tohpe")) {
                        return std::make_unique<TohpePhasePolynomialOptimizationStrategy>();
                    }
                    return nullptr;
                });
                optimize_phase_polynomial(*tableau_mgr.get(), *phasepoly_strategy);
            };

            auto const do_matroid_partition = [&]() {
                auto const ancillae            = parser.get<size_t>("--ancillae");
                auto const matpar_strategy_str = parser.get<std::string>("strategy");

                auto const matpar_strategy = std::invoke([&]() -> std::unique_ptr<MatroidPartitionStrategy> {
                    if (dvlab::str::is_prefix_of(matpar_strategy_str, "naive")) {
                        return std::make_unique<NaiveMatroidPartitionStrategy>();
                    }
                    return nullptr;
                });
                auto const matpar_result   = matroid_partition(*tableau_mgr.get(), *matpar_strategy, ancillae);
                if (!matpar_result) {
                    spdlog::error("Matroid partitioning failed!!");
                    return false;
                }
                *tableau_mgr.get() = *matpar_result;

                return true;
            };

            switch (*method) {
                case OptimizationMethod::full:
                    full_optimize(*tableau_mgr.get());
                    break;
                case OptimizationMethod::collapse:
                    collapse(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("collapse");
                    break;
                case OptimizationMethod::t_merge:
                    merge_rotations(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("MergeT");
                    break;
                case OptimizationMethod::internal_h_opt:
                    minimize_internal_hadamards(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("InternalHOpt");
                    break;
                case OptimizationMethod::phase_polynomial_optimization:
                    do_phase_polynomial_optimization();
                    tableau_mgr.get()->add_procedure("PhasePolyOpt");
                    break;
                case OptimizationMethod::matroid_partition:
                    if (!do_matroid_partition()) {
                        return dvlab::CmdExecResult::error;
                    }
                    tableau_mgr.get()->add_procedure("MatroidPartition");
                    break;
            }

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_cmd(TableauMgr& tableau_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(tableau_mgr);

    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_list_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_new_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_delete_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_checkout_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_copy_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_append_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_adjoint_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_print_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_optimization_cmd(tableau_mgr));

    return cmd;
}

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr) {
    return cli.add_command(tableau_cmd(tableau_mgr));
}

}  // namespace qsyn::experimental
