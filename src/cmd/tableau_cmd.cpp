/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_cmd.hpp"

#include <cstdint>
#include <filesystem>

#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "argparse/argument.hpp"
#include "cli/cli.hpp"
#include "cmd/tableau_mgr.hpp"
#include "cmd/ncf_mgr.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "convert/tableau_to_qcir.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/pauli_terms_io.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"
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

dvlab::Command tableau_from_pauli_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "from-pauli",
        [&](ArgumentParser& parser) {
            parser.description("Pauli String Exponentiation (PSE) or Hamiltonian Term Exponentiation (HTE) to tableau");

            parser.add_argument<std::string>("phase")
                .nargs(NArgsOption::optional)
                .default_value("0")
                .help("Common rotation phase for Pauli String Exponentiation (e.g. pi/4, pi/2, 1/4*pi). Default: 0");

            parser.add_argument<std::string>("paulis")
                .nargs(NArgsOption::zero_or_more)
                .help("Pauli strings such as ZIII, IIZX, etc. Use with <phase> for Pauli String Exponentiation; all must have the same length.");

            parser.add_argument<std::string>("--terms")
                .nargs(NArgsOption::one_or_more)
                .help("Hamiltonian terms with individual phases, formatted as <pauli>:<phase> (e.g. ZIII:pi/4 XXII:-pi/8)");

            parser.add_argument<size_t>("id")
                .nargs(NArgsOption::optional)
                .help("ID of the Tableau to create/replace");

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help("If specified, replace the existing Tableau with the same ID");
        },
        [&](ArgumentParser const& parser) {
            using dvlab::Phase;

            auto const paulis      = parser.get<std::vector<std::string>>("paulis");
            auto const terms_input = parser.parsed("--terms") ? parser.get<std::vector<std::string>>("--terms") : std::vector<std::string>{};

            auto const has_terms         = !terms_input.empty();
            auto const has_pauli_strings = !paulis.empty();

            if (!has_terms && !has_pauli_strings) {
                spdlog::error("Specify either --terms for Hamiltonian Term Exponentiation or <pauli strings> for Pauli String Exponentiation!!");
                return dvlab::CmdExecResult::error;
            }

            if (has_terms && parser.parsed("phase")) spdlog::warn("Ignoring common phase because --terms (HTE) is provided.");
            if (has_terms && has_pauli_strings) spdlog::warn("Ignoring positional Pauli strings because --terms (HTE) is provided.");

            auto const store_tableau = [&](Tableau&& tableau) {
            auto const id = parser.parsed("id") ? parser.get<size_t>("id") : tableau_mgr.get_next_id();

            if (tableau_mgr.is_id(id)) {
                if (!parser.parsed("--replace")) {
                    spdlog::error("Tableau {} already exists!! Please specify `--replace` to replace if needed", id);
                    return dvlab::CmdExecResult::error;
                }
                tableau_mgr.set_by_id(id, std::make_unique<Tableau>(std::move(tableau)));
            } else {
                tableau_mgr.add(id, std::make_unique<Tableau>(std::move(tableau)));
            }

            return dvlab::CmdExecResult::done;
            };

            if (has_terms) {
                std::vector<std::pair<std::string, Phase>> parsed_terms;
                parsed_terms.reserve(terms_input.size());

                for (auto const& term : terms_input) {
                    auto const pos = term.find(':');
                    if (pos == std::string::npos || pos == 0 || pos == term.size() - 1) {
                        spdlog::error("Cannot parse term \"{}\"!! Expected format <pauli>:<phase> (e.g. ZIII:pi/4)", term);
                        return dvlab::CmdExecResult::error;
                    }
                    auto const pauli_str = term.substr(0, pos);
                    auto const phase_str = term.substr(pos + 1);

                    auto const phase_opt = Phase::from_string(phase_str);
                    if (!phase_opt) {
                        spdlog::error("Cannot parse phase string \"{}\" in term \"{}\"!!", phase_str, term);
                        return dvlab::CmdExecResult::error;
                    }

                    parsed_terms.emplace_back(pauli_str, *phase_opt);
                }

                if (parsed_terms.empty()) {
                    spdlog::error("No Hamiltonian terms provided!!");
                    return dvlab::CmdExecResult::error;
                }

                auto const n_qubits = parsed_terms.front().first.size();
                if (!std::ranges::all_of(parsed_terms, [n_qubits](auto const& term) { return term.first.size() == n_qubits; })) {
                    spdlog::error("All Pauli strings must have the same length!!");
                    return dvlab::CmdExecResult::error;
                }

                auto tableau = make_tableau_from_pauli_terms(parsed_terms);
                tableau.set_filename("hamiltonian_term_exp");
                tableau.add_procedure("hamiltonian-term-exp");

                return store_tableau(std::move(tableau));
            }

            auto const phase_str = parser.get<std::string>("phase");
            auto const phase_opt = Phase::from_string(phase_str);
            if (!phase_opt) {
                spdlog::error("Cannot parse phase string \"{}\"!!", phase_str);
                return dvlab::CmdExecResult::error;
            }

            auto const n_qubits = paulis.front().size();
            if (!std::ranges::all_of(paulis, [n_qubits](std::string const& s) { return s.size() == n_qubits; })) {
                spdlog::error("All Pauli strings must have the same length!!");
                return dvlab::CmdExecResult::error;
            }

            auto tableau = make_tableau_from_pauli_strings(paulis, *phase_opt);
            tableau.set_filename("pauli_string_exp");
            tableau.add_procedure("pauli-string-exp");

            return store_tableau(std::move(tableau));
        }};
}

dvlab::Command tableau_from_terms_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "from-terms",
        [&](ArgumentParser& parser) {
            parser.description("Load numbered Pauli Hamiltonian terms from a .json or .terms file (PySCF export)");

            parser.add_argument<std::string>("filepath")
                .help("Path to terms file (.json or .terms)");

            parser.add_argument<size_t>("id")
                .nargs(NArgsOption::optional)
                .help("ID of the Tableau to create/replace");

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help("If specified, replace the existing Tableau with the same ID");

            parser.add_argument<bool>("--use-coeff")
                .action(store_true)
                .help("Use coeff field as rotation angle (multiply by --scale)");

            parser.add_argument<double>("--scale")
                .default_value(1.0)
                .help("Scale factor applied to coeff/angle values");
        },
        [&](ArgumentParser const& parser) {
            auto const path = parser.get<std::string>("filepath");
            auto file_opt   = load_pauli_terms_file(path);
            if (!file_opt) return dvlab::CmdExecResult::error;

            PauliTermsLoadOptions options;
            options.use_coeff = parser.get<bool>("--use-coeff");
            options.scale     = parser.get<double>("--scale");

            auto pairs_opt = to_pauli_phase_pairs(*file_opt, options);
            if (!pairs_opt) return dvlab::CmdExecResult::error;

            auto tableau = make_tableau_from_pauli_terms(*pairs_opt);
            tableau.set_filename(std::filesystem::path{path}.filename().string());
            tableau.add_procedure("from-terms");

            auto const id = parser.parsed("id") ? parser.get<size_t>("id") : tableau_mgr.get_next_id();
            if (tableau_mgr.is_id(id)) {
                if (!parser.parsed("--replace")) {
                    spdlog::error("Tableau {} already exists!! Please specify `--replace` to replace if needed", id);
                    return dvlab::CmdExecResult::error;
                }
                tableau_mgr.set_by_id(id, std::make_unique<Tableau>(std::move(tableau)));
            } else {
                tableau_mgr.add(id, std::make_unique<Tableau>(std::move(tableau)));
            }
            print_pauli_terms_loaded(*file_opt, id);
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

dvlab::Command tableau_equiv_cmd(TableauMgr& tableau_mgr) {
    return dvlab::Command{
        "equiv",
        [&](ArgumentParser& parser) {
            parser.description(
                "Check if two tableaux are equivalent. Converts each to a circuit, then to a "
                "tensor, and compares the two unitaries (allows global phase; uses tolerance 1e-5).");

            parser.add_argument<size_t>("ids")
                .nargs(2)
                .constraint(dvlab::utils::valid_mgr_id(tableau_mgr))
                .help("Two tableau IDs to compare (e.g. 0 1)");
        },
        [&](ArgumentParser const& parser) {
            auto const ids = parser.get<std::vector<size_t>>("ids");
            auto const* t0 = tableau_mgr.find_by_id(ids[0]);
            auto const* t1 = tableau_mgr.find_by_id(ids[1]);
            if (!t0 || !t1) {
                return dvlab::CmdExecResult::error;
            }

            auto const qcir0 = qsyn::experimental::to_qcir(
                *t0,
                qsyn::experimental::HOptSynthesisStrategy{},
                qsyn::experimental::NaivePauliRotationsSynthesisStrategy{});
            auto const qcir1 = qsyn::experimental::to_qcir(
                *t1,
                qsyn::experimental::HOptSynthesisStrategy{},
                qsyn::experimental::NaivePauliRotationsSynthesisStrategy{});

            if (!qcir0) {
                spdlog::error("Failed to convert tableau {} to circuit.", ids[0]);
                return dvlab::CmdExecResult::error;
            }
            if (!qcir1) {
                spdlog::error("Failed to convert tableau {} to circuit.", ids[1]);
                return dvlab::CmdExecResult::error;
            }

            if (t0->n_qubits() > 7) {
                spdlog::error("Tableau equiv via tensor only supports up to 7 qubits (got {}).", t0->n_qubits());
                return dvlab::CmdExecResult::error;
            }

            auto const tensor0 = qsyn::to_tensor(*qcir0);
            auto const tensor1 = qsyn::to_tensor(*qcir1);
            if (!tensor0 || !tensor1) {
                spdlog::error("Failed to convert circuit to tensor.");
                return dvlab::CmdExecResult::error;
            }

            // Direct comparison of the two unitaries (tolerance 1e-5 to allow for floating-point accumulation)
            bool const equiv = qsyn::tensor::is_equivalent(*tensor0, *tensor1, 1e-5);
            if (equiv) {
                fmt::println(
                    "{}",
                    dvlab::fmt_ext::styled_if_ansi_supported(
                        "The two tableaux are equivalent!!",
                        fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            } else {
                fmt::println(
                    "{}",
                    dvlab::fmt_ext::styled_if_ansi_supported(
                        "The two tableaux are not equivalent!!",
                        fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            }
            return dvlab::CmdExecResult::done;
        }};
}

dvlab::Command tableau_optimization_cmd(TableauMgr& tableau_mgr, NcfMgr& ncf_mgr) {
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

            auto ncf_parser = methods.add_parser("ncf")
                                  .description("Non-Clifford Fusion: partition Pauli rotations into groups that conjugate to 1-qubit (default) or 2-qubit (--two-qubit), then emit [C†][R'][C] Clifford+RZ blocks (see arXiv:2510.13573). Does not synthesize fused unitaries into Clifford+T.");
            ncf_parser.add_argument<bool>("--all-merges")
                .action(store_true)
                .help("Enumerate all valid partial-merge cases (merge all / none / subset-by-group) and store each case as a separate tableau ID");
            ncf_parser.add_argument<size_t>("--max-cases")
                .default_value(0)
                .help("Maximum number of enumerated NCF cases (0 = no limit)");
            ncf_parser.add_argument<bool>("--overlap-priority")
                .action(store_true)
                .help("Pick anti-commuting NCF pairs with maximum Paulihedral Pauli-string overlap first (ASPLOS'22 metric; 1-qubit mode)");
            ncf_parser.add_argument<bool>("--two-qubit")
                .action(store_true)
                .help("Paper §IV-A2 two-qubit grouping: grading system + Table III + sliding window (default w=128)");
            ncf_parser.add_argument<size_t>("--window")
                .default_value(0)
                .help("Sliding-window size w (0 = paper default: 4 for 1q, 128 for 2q)");

            methods.add_parser("equiv")
                .description("Lightweight optimization for equivalence checking (tmerge + hopt only, no phase polynomial / TODD); safe for arbitrary phases");

            auto phasepoly_parser = methods.add_parser("phasepoly")
                                        .description("Reduce the number of terms for phase polynomials in the Tableau");

            phasepoly_parser.add_argument<std::string>("strategy")
                .default_value("todd")
                .constraint(choices_allow_prefix({"todd"}))
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
                ncf_fusion,
                equiv,
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
                } else if (dvlab::str::is_prefix_of(method_str, "ncf")) {
                    return OptimizationMethod::ncf_fusion;
                } else if (dvlab::str::is_prefix_of(method_str, "equiv")) {
                    return OptimizationMethod::equiv;
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
                case OptimizationMethod::ncf_fusion: {
                    auto pre_ncf = std::make_unique<Tableau>(*tableau_mgr.get());
                    NcfFusionOptions ncf_options{};
                    ncf_options.overlap_priority = parser.parsed("--overlap-priority");
                    ncf_options.two_qubit        = parser.parsed("--two-qubit");
                    ncf_options.window_w         = parser.get<size_t>("--window");
                    if (parser.parsed("--all-merges")) {
                        ncf_options.all_merges = true;
                        ncf_options.max_cases  = parser.get<size_t>("--max-cases");
                        auto cases             = ncf_fusion_all(*tableau_mgr.get(), ncf_options);
                        if (cases.empty()) {
                            spdlog::error("NCF enumerate produced no candidates.");
                            return dvlab::CmdExecResult::error;
                        }
                        *tableau_mgr.get() = std::move(cases.front());
                        tableau_mgr.get()->add_procedure("NCF-all-merges-case0");
                        for (size_t i = 1; i < cases.size(); ++i) {
                            auto id = tableau_mgr.get_next_id();
                            cases[i].add_procedure("NCF-all-merges-case" + std::to_string(i));
                            tableau_mgr.add(id, std::make_unique<Tableau>(std::move(cases[i])));
                            spdlog::info("NCF enumerate: case {} stored as Tableau ID {}", i, id);
                        }
                        spdlog::info("NCF enumerate: case 0 kept in current Tableau ID {}", tableau_mgr.focused_id());
                    } else {
                        ncf_fusion(*tableau_mgr.get(), ncf_options);
                        std::string proc = "NCF";
                        if (ncf_options.two_qubit) {
                            proc = ncf_options.window_w == 0
                                       ? "NCF-2q-w128"
                                       : fmt::format("NCF-2q-w{}", ncf_options.window_w);
                        } else if (ncf_options.overlap_priority) {
                            proc = "NCF-overlap-priority";
                        }
                        tableau_mgr.get()->add_procedure(proc);
                    }
                    register_ncf_from_tableau(ncf_mgr, tableau_mgr, pre_ncf.get());
                    break;
                }
                case OptimizationMethod::equiv:
                    optimize_for_equiv(*tableau_mgr.get());
                    tableau_mgr.get()->add_procedure("OptimizeForEquiv");
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

dvlab::Command tableau_cmd(TableauMgr& tableau_mgr, NcfMgr& ncf_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(tableau_mgr);

    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_list_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_new_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_from_pauli_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_from_terms_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_delete_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_checkout_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", dvlab::utils::mgr_copy_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_append_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_adjoint_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_print_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_equiv_cmd(tableau_mgr));
    cmd.add_subcommand("tableau-cmd-group", tableau_optimization_cmd(tableau_mgr, ncf_mgr));

    return cmd;
}

bool add_tableau_command(dvlab::CommandLineInterface& cli, TableauMgr& tableau_mgr, NcfMgr& ncf_mgr) {
    return cli.add_command(tableau_cmd(tableau_mgr, ncf_mgr));
}

}  // namespace qsyn::experimental
