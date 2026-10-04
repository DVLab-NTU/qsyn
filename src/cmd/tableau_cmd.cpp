/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_cmd.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

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

namespace qsyn::tableau {

std::optional<qcir::QCir> tableau_to_qcir_hopt_naive(Tableau const& tableau);

ArgType<size_t>::ConstraintType valid_tableau_qubit_id(TableauMgr const& tableau_mgr) {
    return [&tableau_mgr](size_t const& id) -> bool {
        if (id < tableau_mgr.get()->n_qubits()) return true;
        spdlog::error("Qubit {} does not exist in Tableau {}!!", id, tableau_mgr.focused_id());
        return false;
    };
}

dvlab::Command tableau_new_cmd(TableauMgr& tableau_mgr) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
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
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
                tableau_mgr.set_by_id(id, std::make_unique<Tableau>(n_qubits));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
                return dvlab::CmdExecResult::done;
            }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
            tableau_mgr.add(id, std::make_unique<Tableau>(n_qubits));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

            return dvlab::CmdExecResult::done;
        }};
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
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
                .description("Perform tmerge, hopt, FastTODD until the T-count stops decreasing");

            methods.add_parser("collapse")
                .description("Collapse the tableau into a canonical form");

            methods.add_parser("tmerge")
                .description("Merge rotations of the same rotation plane");

            methods.add_parser("hopt")
                .description("Minimize the number of Hadamard gates and internal Hadamard gates in the tableau");

            methods.add_parser("unify")
                .description("T-opt with H-gadgetize: gadgetize then FastTODD (no SMT reorder)");

            methods.add_parser("reorder")
                .description("SMT-based gadget/PR reordering for ancilla minimization; runs unify first if needed");

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
                unify_t_opt,
                reorder,
                phase_polynomial_optimization,
                matroid_partition,
                commute_test
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
                } else if (dvlab::str::is_prefix_of(method_str, "unify")) {
                    return OptimizationMethod::unify_t_opt;
                } else if (dvlab::str::is_prefix_of(method_str, "reorder")) {
                    return OptimizationMethod::reorder;
                } else if (dvlab::str::is_prefix_of(method_str, "phasepoly")) {
                    return OptimizationMethod::phase_polynomial_optimization;
                } else if (dvlab::str::is_prefix_of(method_str, "matpar")) {
                    return OptimizationMethod::matroid_partition;
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

            auto const apply_unify = [&]() {
                auto& tableau          = *tableau_mgr.get();
                auto const export_name = !qcir_mgr.get_filename().empty()
                                             ? qcir_mgr.get_filename()
                                             : tableau_mgr.get_filename();
                minimize_ancillary_t_opt(
                    tableau,
                    export_name.empty() ? std::nullopt : std::optional<std::string>{export_name});
                tableau_mgr.add_procedure("UnifyTOpt");
            };

            auto const unify_already_applied = [&]() {
                auto const& procedures = tableau_mgr.get_procedures();
                return std::ranges::any_of(procedures, [](std::string const& procedure) {
                    return procedure == "UnifyTOpt" || procedure == "UnifiedTOpt";
                });
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
                    tableau_mgr.add_procedure("collapse");
                    break;
                case OptimizationMethod::t_merge:
                    merge_rotations(*tableau_mgr.get());
                    tableau_mgr.add_procedure("MergeT");
                    break;
                case OptimizationMethod::internal_h_opt:
                    minimize_internal_hadamards(*tableau_mgr.get());
                    tableau_mgr.add_procedure("InternalHOpt");
                    break;
                case OptimizationMethod::unify_t_opt:
                    apply_unify();
                    break;
                case OptimizationMethod::reorder: {
                    auto& tableau = *tableau_mgr.get();
                    if (!unify_already_applied()) {
                        apply_unify();
                    }
                    size_t const t0 = tableau.n_pauli_rotations();
                    size_t const a0 = tableau.n_ancilla();
                    sat_reorder(tableau);
                    tableau_mgr.add_procedure("SatReorder");
                    log_topt_stage("reorder", "SMT width search",
                                   t0, tableau.n_pauli_rotations(), a0, tableau.n_ancilla(),
                                   /*with_t=*/false);
                    break;
                }
                case OptimizationMethod::phase_polynomial_optimization:
                    do_phase_polynomial_optimization();
                    tableau_mgr.add_procedure("PhasePolyOpt");
                    break;
                case OptimizationMethod::matroid_partition:
                    if (!do_matroid_partition()) {
                        return dvlab::CmdExecResult::error;
                    }
                    tableau_mgr.add_procedure("MatroidPartition");
                    break;
                case OptimizationMethod::commute_test: {
                    auto const txt_file = parser.get<std::string>("txt-file");
                    if (!run_commute_test_from_file(txt_file)) {
                        return dvlab::CmdExecResult::error;
                    }
                    if (!tableau_mgr.empty()) {
                        tableau_mgr.add_procedure("CommuteTest");
                    }
                    break;
                }
            }

            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_tie_search_cmd(TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr, std::string_view name) {
    return dvlab::Command{
        name,
        [&](ArgumentParser& parser) {
            parser.description(
                "runs the tie-search algorithm to co-optimize T and ancilla count. "
                "Measurement gates as well as ancilla will be added for optimization.");
            parser.add_argument<size_t>("--cycle", "-cycle")
                .nargs(NArgsOption::optional)
                .help("Optional safety cap on total phase-2 shots (default: unlimited)");
            parser.add_argument<size_t>("--early-stop", "--early_stop", "-early_stop")
                .nargs(NArgsOption::optional)
                .help("Fixed tie-search patience override; all_random uses N/2 (default: dynamic max(60, ceil(2.5*A_min)) for the current best)");
            auto t_only_cmds = parser.add_subparsers("t-only-mode").required(false);
            auto add_t_only  = [&](std::string_view name) {
                auto t_only_parser = t_only_cmds.add_parser(name);
                t_only_parser.description("Minimize T-count only (skip SMT/ancilla). Tracks last T-reduction step.");
                t_only_parser.add_argument<size_t>("--repeats")
                    .nargs(NArgsOption::optional)
                    .help("Independent repeats (default 1). Logs avg last T-reduction step.");
            };
            add_t_only("-t-only");
            add_t_only("--t-only");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) {
                spdlog::error("tie-search requires QCir; run qc read first");
                return dvlab::CmdExecResult::error;
            }

            auto const set_env_override = [](char const* key, std::string const& value) {
                if (setenv(key, value.c_str(), 1) != 0) {
                    spdlog::warn("Failed to set env {}={}", key, value);
                }
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

            if (parser.parsed("--cycle")) {
                set_env_override("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS", std::to_string(parser.get<size_t>("--cycle")));
            }
            if (parser.parsed("--early-stop")) {
                set_env_override("QSYN_FASTTODD_TIE_SEARCH_PATIENCE", std::to_string(parser.get<size_t>("--early-stop")));
            }

            bool const t_only = parser.get_activated_subparser().has_value();
            size_t const repeats =
                t_only && parser.parsed("--repeats") ? parser.get<size_t>("--repeats") : size_t{1};
            if (t_only && !parser.parsed("--cycle")) {
                set_env_override("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS", "1000");
            }
            if (t_only && !parser.parsed("--early-stop")) {
                set_env_override("QSYN_FASTTODD_TIE_SEARCH_PATIENCE", "100");
            }

            Tableau optimized{0};
            bool ok = false;
            if (t_only) {
                TOnlyTieSearchAggregate agg;
                ok = minimize_t_opt_tie_search_from_qcir(*qcir_mgr.get(), optimized, repeats, &agg);
                if (ok) {
                    spdlog::info(
                        "t-only tie search csv: repeats={} min_T={} max_T={} avg_T={:.6f} "
                        "avg_last_t_reduce_step={:.6f} avg_last_t_reduce_step_at_min_T={:.6f} hit_min_T={}",
                        agg.repeats,
                        agg.min_t,
                        agg.max_t,
                        agg.avg_final_t,
                        agg.avg_last_t_reduce_step,
                        agg.avg_last_t_reduce_step_at_min_t,
                        agg.n_hit_min_t);
                }
            } else {
                ok = minimize_ancillary_t_opt_from_qcir(*qcir_mgr.get(), optimized);
            }
            if (!ok) {
                return dvlab::CmdExecResult::error;
            }

            if (tableau_mgr.empty()) {
                tableau_mgr.add(tableau_mgr.get_next_id(), std::make_unique<Tableau>(std::move(optimized)));
            } else {
                *tableau_mgr.get() = std::move(optimized);
            }
            if (!qcir_mgr.get_filename().empty()) {
                tableau_mgr.set_filename(qcir_mgr.get_filename());
            }
            tableau_mgr.add_procedure(t_only ? "TOnlyTieSearch" : "TieSearch");

            spdlog::debug("Converting Tableau {} to QCir {}...", tableau_mgr.focused_id(), qcir_mgr.get_next_id());
            auto qcir = tableau_to_qcir_hopt_naive(*tableau_mgr.get());
            if (!qcir.has_value()) {
                spdlog::error("tie-search: tableau-to-qcir conversion failed");
                return dvlab::CmdExecResult::error;
            }
            qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(qcir.value())));
            qcir_mgr.set_filename(tableau_mgr.get_filename());
            qcir_mgr.add_procedures(tableau_mgr.get_procedures());
            qcir_mgr.add_procedure("TABL2QC");
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

    return cmd;
}

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr, qsyn::qcir::QCirMgr& qcir_mgr) {
    return cli.add_command(tableau_cmd(tableau_mgr, qcir_mgr)) &&
           cli.add_command(tableau_tie_search_cmd(tableau_mgr, qcir_mgr, "tie-search"));
}

}  // namespace qsyn::tableau
