/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_cmd.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "argparse/argument.hpp"
#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau_optimization.hpp"
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

            std::ranges::copy(qubits, qubit_array.begin());

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

            mutex.add_argument<bool>("-g", "--gate")
                .action(store_true)
                .help("Print the tableau with extracted gate lists for Clifford segments");
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
            if (parser.parsed("-g")) {
                fmt::println("{:g}", *tableau_mgr.get());
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

dvlab::Command tableau_optimization_cmd(TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr) {
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

            methods.add_parser("gadgetH")
                .description("Minimize the number of Hadamard gates using H gadgets (ancilla qubits and measurements)");

            auto ancillary_parser = methods.add_parser("ancillaryTopt")
                .description("Minimize the number of T gates in the tableau with the help of classical operations & ancillary qubits");
            ancillary_parser.add_argument<bool>("--tie-search")
                .action(store_true)
                .help("Enable recursive FastTODD tied-move exploration with SAT width probing");
            ancillary_parser.add_argument<std::string>("--tie-search-mode", "-tie-search")
                .default_value("target-random")
                .constraint(choices_allow_prefix({
                    "original",
                    "all-random",
                    "target-random",
                    "force-prefix-target-random",
                }))
                .help("Tie-search mode: original, all-random, target-random, or force-prefix-target-random");
            ancillary_parser.add_argument<size_t>("-m", "--merge-rotations")
                .nargs(NArgsOption::optional)
                .help("Override QSYN_TABLEAU_MERGE_ROTATIONS (0/1)");
            ancillary_parser.add_argument<size_t>("-p", "--properize")
                .nargs(NArgsOption::optional)
                .help("Override QSYN_TABLEAU_PROPERIZE (0/1)");
            ancillary_parser.add_argument<size_t>("--cycle", "-cycle")
                .nargs(NArgsOption::optional)
                .help("Override tie-search max trials (M)");
            ancillary_parser.add_argument<size_t>("--early-stop", "--early_stop", "-early_stop")
                .nargs(NArgsOption::optional)
                .help("Override tie-search patience (N)");

            auto unified_parser = methods.add_parser("unified")
                .description("Alias for ancillaryTopt (H-gadgetize + classical-aware phase polynomial optimization)");
            unified_parser.add_argument<bool>("--tie-search")
                .action(store_true)
                .help("Enable recursive FastTODD tied-move exploration with SAT width probing");
            unified_parser.add_argument<std::string>("--tie-search-mode", "-tie-search")
                .default_value("target-random")
                .constraint(choices_allow_prefix({
                    "original",
                    "all-random",
                    "target-random",
                    "force-prefix-target-random",
                }))
                .help("Tie-search mode: original, all-random, target-random, or force-prefix-target-random");
            unified_parser.add_argument<size_t>("-m", "--merge-rotations")
                .nargs(NArgsOption::optional)
                .help("Override QSYN_TABLEAU_MERGE_ROTATIONS (0/1)");
            unified_parser.add_argument<size_t>("-p", "--properize")
                .nargs(NArgsOption::optional)
                .help("Override QSYN_TABLEAU_PROPERIZE (0/1)");
            unified_parser.add_argument<size_t>("--cycle", "-cycle")
                .nargs(NArgsOption::optional)
                .help("Override tie-search max trials (M)");
            unified_parser.add_argument<size_t>("--early-stop", "--early_stop", "-early_stop")
                .nargs(NArgsOption::optional)
                .help("Override tie-search patience (N)");

            auto test_parser = methods.add_parser("test")
                                   .description("Run commute-text validation test and compare simulated PMC with ops section");
            test_parser.add_argument<std::string>("txt-file")
                .help("Path to the commute test text file");

            auto phasepoly_parser = methods.add_parser("phasepoly")
                                        .description("Reduce the number of terms for phase polynomials in the Tableau");

            phasepoly_parser.add_argument<std::string>("strategy")
                .default_value("todd")
                .constraint(choices_allow_prefix({"todd", "fasttodd", "tohpe"}))
                .help("Phase polynomial optimization strategy (todd, fasttodd, tohpe)");

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
            auto const method_str = parser.get<std::string>("method");

            enum struct OptimizationMethod : std::uint8_t {
                full,
                collapse,
                t_merge,
                internal_h_opt,
                internal_h_opt_gadgetize,
                phase_polynomial_optimization,
                matroid_partition,
                ancillary_t_opt,
                commute_test
            };

            auto method = std::invoke([&]() -> std::optional<OptimizationMethod> {
                if (dvlab::str::is_prefix_of(method_str, "full")) {
                    return OptimizationMethod::full;
                } else if (dvlab::str::is_prefix_of(method_str, "collapse")) {
                    return OptimizationMethod::collapse;
                } else if (dvlab::str::is_prefix_of(method_str, "tmerge")) {
                    return OptimizationMethod::t_merge;
                } else if (dvlab::str::is_prefix_of(method_str, "gadgetH")) {
                    return OptimizationMethod::internal_h_opt_gadgetize;
                } else if (dvlab::str::is_prefix_of(method_str, "hopt")) {
                    return OptimizationMethod::internal_h_opt;
                } else if (dvlab::str::is_prefix_of(method_str, "phasepoly")) {
                    return OptimizationMethod::phase_polynomial_optimization;
                } else if (dvlab::str::is_prefix_of(method_str, "matpar")) {
                    return OptimizationMethod::matroid_partition;
                } else if (dvlab::str::is_prefix_of(method_str, "ancillaryTopt") ||
                           dvlab::str::is_prefix_of(method_str, "unified")) {
                    return OptimizationMethod::ancillary_t_opt;
                } else if (dvlab::str::is_prefix_of(method_str, "test")) {
                    return OptimizationMethod::commute_test;
                }
                return std::nullopt;
            });

            if (!method) {
                spdlog::error("Unknown optimization method {}!!", method_str);
                return dvlab::CmdExecResult::error;
            }
            if (*method != OptimizationMethod::commute_test && !dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            auto const do_phase_polynomial_optimization = [&]() {
                auto const phasepoly_strategy_str = parser.get<std::string>("strategy");

                auto const phasepoly_strategy = std::invoke([&]() -> std::unique_ptr<PhasePolynomialOptimizationStrategy> {
                    if (dvlab::str::is_prefix_of(phasepoly_strategy_str, "fasttodd")) {
                        return std::make_unique<FastToddPhasePolynomialOptimizationStrategy>();
                    }
                    if (dvlab::str::is_prefix_of(phasepoly_strategy_str, "tohpe")) {
                        return std::make_unique<TohpePhasePolynomialOptimizationStrategy>();
                    }
                    if (dvlab::str::is_prefix_of(phasepoly_strategy_str, "todd")) {
                        return std::make_unique<ToddPhasePolynomialOptimizationStrategy>();
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
                case OptimizationMethod::internal_h_opt_gadgetize:
                    minimize_internal_hadamards_n_gadgetize(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("InternalHOptGadgetize");
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
                case OptimizationMethod::ancillary_t_opt: {
                    auto const set_env_override = [](char const* key, std::string const& value) {
                        if (setenv(key, value.c_str(), 1) != 0) {
                            spdlog::warn("Failed to set env {}={}", key, value);
                        }
                    };
                    auto const validate_binary_flag = [](char const* name, size_t value) {
                        if (value > 1) {
                            spdlog::error("{} expects 0/1, got {}", name, value);
                            return false;
                        }
                        return true;
                    };
                    auto const scoped_env_restore = [&]() {
                        std::vector<std::pair<std::string, std::optional<std::string>>> saved;
                        auto save_env = [&](char const* key) {
                            if (char const* value = std::getenv(key)) {
                                saved.emplace_back(key, std::string{value});
                            } else {
                                saved.emplace_back(key, std::nullopt);
                            }
                        };
                        save_env("QSYN_TABLEAU_MERGE_ROTATIONS");
                        save_env("QSYN_TABLEAU_PROPERIZE");
                        save_env("QSYN_FASTTODD_TIE_SEARCH");
                        save_env("QSYN_FASTTODD_TIE_SEARCH_MODE");
                        save_env("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS");
                        save_env("QSYN_FASTTODD_TIE_SEARCH_PATIENCE");
                        return saved;
                    };
                    struct EnvRestoreGuard {
                        std::vector<std::pair<std::string, std::optional<std::string>>> saved;
                        ~EnvRestoreGuard() {
                            for (auto const& [key, value] : saved) {
                                if (value.has_value()) {
                                    setenv(key.c_str(), value->c_str(), 1);
                                } else {
                                    unsetenv(key.c_str());
                                }
                            }
                        }
                    };
                    EnvRestoreGuard env_guard{scoped_env_restore()};

                    if (parser.parsed("--merge-rotations")) {
                        auto const value = parser.get<size_t>("--merge-rotations");
                        if (!validate_binary_flag("--merge-rotations", value)) {
                            return dvlab::CmdExecResult::error;
                        }
                        set_env_override("QSYN_TABLEAU_MERGE_ROTATIONS", std::to_string(value));
                    }
                    if (parser.parsed("--properize")) {
                        auto const value = parser.get<size_t>("--properize");
                        if (!validate_binary_flag("--properize", value)) {
                            return dvlab::CmdExecResult::error;
                        }
                        set_env_override("QSYN_TABLEAU_PROPERIZE", std::to_string(value));
                    }
                    if (parser.parsed("--cycle")) {
                        set_env_override("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS", std::to_string(parser.get<size_t>("--cycle")));
                    }
                    if (parser.parsed("--early-stop")) {
                        set_env_override("QSYN_FASTTODD_TIE_SEARCH_PATIENCE", std::to_string(parser.get<size_t>("--early-stop")));
                    }
                    auto const tie_search_mode_str = parser.get<std::string>("--tie-search-mode");
                    auto const tie_search_mode = std::invoke([&]() -> std::optional<FastToddTieSearchMode> {
                        if (dvlab::str::is_prefix_of(tie_search_mode_str, "original")) {
                            return FastToddTieSearchMode::original;
                        }
                        if (dvlab::str::is_prefix_of(tie_search_mode_str, "all-random")) {
                            return FastToddTieSearchMode::all_random;
                        }
                        if (dvlab::str::is_prefix_of(tie_search_mode_str, "target-random")) {
                            return FastToddTieSearchMode::random_target_only;
                        }
                        if (dvlab::str::is_prefix_of(tie_search_mode_str, "force-prefix-target-random")) {
                            return FastToddTieSearchMode::force_prefix_random_target;
                        }
                        return std::nullopt;
                    });
                    if (!tie_search_mode.has_value()) {
                        spdlog::error("Unknown tie-search mode {}!!", tie_search_mode_str);
                        return dvlab::CmdExecResult::error;
                    }
                    bool const enable_tie_search =
                        parser.parsed("--tie-search") || parser.parsed("--tie-search-mode");
                    if (enable_tie_search) {
                        set_env_override("QSYN_FASTTODD_TIE_SEARCH", "1");
                        set_env_override("QSYN_FASTTODD_TIE_SEARCH_MODE", tie_search_mode_str);
                    }
                    minimize_ancillary_t_opt(
                        *tableau_mgr.get(),
                        qcir_mgr.empty()
                            ? std::optional<std::string>{tableau_mgr.get()->get_filename()}
                            : std::optional<std::string>{qcir_mgr.get()->get_filename()},
                        enable_tie_search,
                        tie_search_mode);
                    tableau_mgr.get()->add_procedure("AncillaryTOpt");
                    break;
                }
                case OptimizationMethod::commute_test: {
                    auto const txt_file = parser.get<std::string>("txt-file");
                    if (!run_commute_test_from_file(txt_file)) {
                        return dvlab::CmdExecResult::error;
                    }
                    if (!tableau_mgr.empty()) {
                        tableau_mgr.get()->add_procedure("CommuteTest");
                    }
                    break;
                }
            }

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_minimize_q_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "minimize_q",
        [&](ArgumentParser& parser) {
            parser.description("Minimize qubit usage in gadgetized tableaux");

            auto methods = parser.add_subparsers("method").required(true);
            methods.add_parser("degadgetize")
                .description(
                    "Post-T-opt degadgetization: constraint-graph reorder + degadgetize (shorthand: tableau m d)");
            methods.add_parser("degadgetizationTest")
                .description("Alias for degadgetize (legacy name from former tableau o d)");
            methods.add_parser("reorder")
                .description("SAT-based gadget/PR reordering for ancilla minimization (shorthand: tableau m r)");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) {
                return dvlab::CmdExecResult::error;
            }

            auto const method_str = parser.get<std::string>("method");
            enum struct QubitMinimizationMethod : std::uint8_t {
                degadgetize,
                reorder
            };

            auto const method = std::invoke([&]() -> std::optional<QubitMinimizationMethod> {
                if (dvlab::str::is_prefix_of(method_str, "degadgetizationTest") ||
                    dvlab::str::is_prefix_of(method_str, "degadgetize")) {
                    return QubitMinimizationMethod::degadgetize;
                } else if (dvlab::str::is_prefix_of(method_str, "reorder")) {
                    return QubitMinimizationMethod::reorder;
                }
                return std::nullopt;
            });

            if (!method) {
                spdlog::error("Unknown qubit minimization method {}!!", method_str);
                return dvlab::CmdExecResult::error;
            }

            switch (*method) {
                case QubitMinimizationMethod::degadgetize:
                    reorder_n_degadgetize(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("MinimizeQDegadgetize");
                    break;
                case QubitMinimizationMethod::reorder:
                    sat_reorder(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("MinimizeQReorder");
                    break;
            }
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_cmd(TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(tableau_mgr);

    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_list_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_new_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_delete_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_checkout_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_copy_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_append_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_adjoint_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_print_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_optimization_cmd(tableau_mgr, qcir_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_minimize_q_cmd(tableau_mgr));

    return cmd;
}

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr) {
    return cli.add_command(tableau_cmd(tableau_mgr, qcir_mgr));
}

}  // namespace qsyn::experimental
