/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ CPF product-pipeline step commands and top-level `qcpfq`. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "cmd/device_mgr.hpp"
#include "qcir/coupling_constraints.hpp"
#include "qcir/cpf_pipeline.hpp"
#include "qcir/qcir.hpp"
#include "util/data_structure_manager.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

namespace {

void apply_cpf_fold_strategy_arg(ArgumentParser const& parser, CpfPipelineOptions& opt) {
    opt.use_dag_fold = !parser.parsed("--no-dag-fold");
    if (parser.parsed("--fold-strategy")) {
        opt.fold_strategy = parse_cpf_fold_strategy(parser.get<std::string>("--fold-strategy"));
        if (opt.fold_strategy != CpfFoldStrategy::global_only) {
            opt.use_dag_fold = true;
        }
    }
}

void apply_pauli_compress_arg(ArgumentParser const& parser, CpfPipelineOptions& opt) {
    if (parser.parsed("--no-pauli-compress")) {
        opt.run_lossless_pauli_compress = false;
    }
}

void log_pipeline_stats(CpfPipelineStats const& stats) {
    if (stats.rotations_after_trace_replay > 0 || stats.ran_cpf_fold) {
        spdlog::info(
            "cpf-pipeline summary: {} -> {} after fold, {} after collapse, {} after pauli-compress ({} rounds)",
            stats.rotations_after_trace_replay, stats.rotations_after_fold,
            stats.rotations_after_collapse, stats.rotations_after_pauli_compress, stats.n_rounds);
    }
    if (stats.used_dag_fold && stats.dag_fold_stats.n_passes > 0) {
        auto const& s = stats.dag_fold_stats.staq;
        auto const& g = stats.dag_fold_stats.graph;
        auto const& p = stats.dag_fold_stats.global;
        spdlog::info("cpf-pipeline: staq_fold {} merges (rots {} -> {})",
                     s.n_merges, s.n_rotations_before, s.n_rotations_after);
        spdlog::info("cpf-pipeline: pauli_dag {} nodes, {} merge edges, {} edge-folds",
                     g.n_nodes, g.n_merge_edges, g.n_component_merges);
        spdlog::info("cpf-pipeline: global prune {} rotations ({} -> {} Pauli classes)",
                     p.n_rotations_pruned, p.n_terms_before, p.n_terms_after);
    }
    if (stats.fold_stats.n_passes > 0) {
        spdlog::info("cpf-pipeline: {} {} local + {} prop merges in {} passes",
                     stats.used_dag_fold ? "dag_fold" : "global_fold",
                     stats.fold_stats.n_local_merges, stats.fold_stats.n_propagation_merges,
                     stats.fold_stats.n_passes);
    }
    if (stats.used_graysynth_per_block) {
        spdlog::info("cpf-pipeline: used Gray-synth for at least one rotation block.");
    }
    if (stats.used_naive_fallback) {
        spdlog::warn("cpf-pipeline: non-diagonal block(s) synthesised with naive (Gray-synth is diagonal-only).");
    }
    if (stats.ran_lossless_pauli_compress && stats.pauli_compress_stats.n_before > 0) {
        auto const& pc = stats.pauli_compress_stats;
        spdlog::info("cpf-pipeline: pauli-compress {} -> {} ({} merged, {} cancelled)",
                     pc.n_before, pc.n_after, pc.n_merged, pc.n_cancelled);
    }
}

void store_qcir(QCirMgr& mgr, QCir&& qcir, bool replace) {
    if (replace) {
        *mgr.get() = std::move(qcir);
    } else {
        mgr.add(mgr.get_next_id(), std::make_unique<QCir>(std::move(qcir)));
    }
}

void add_u3cx_compile_args(dvlab::argparse::ArgumentParser& parser) {
    parser.add_argument<bool>("--u3syn")
        .action(dvlab::argparse::store_true)
        .help("U3+CX via external BQSKit compile() (partition, QSearch/LEAP, instantiate).");
    parser.add_argument<bool>("--bqskit")
        .action(dvlab::argparse::store_true)
        .help("Alias for --u3syn (deprecated name).");
    parser.add_argument<bool>("--qsearch")
        .action(dvlab::argparse::store_true)
        .help("Per-block QSearch via scripts/bqskit_block_synth.py (needs bqskit).");
    parser.add_argument<bool>("--leap")
        .action(dvlab::argparse::store_true)
        .help("Per-block LEAP synthesis (bqskit_block_synth.py).");
    parser.add_argument<bool>("--qsearch-native")
        .action(dvlab::argparse::store_true)
        .help("(PR-C) Per-block QSearch implemented natively in qsyn "
              "(no external bqskit subprocess).");
    parser.add_argument<bool>("--leap-native")
        .action(dvlab::argparse::store_true)
        .help("(PR-C) Per-block LEAP implemented natively in qsyn.");
    parser.add_argument<bool>("--qfast")
        .action(dvlab::argparse::store_true)
        .help("QFAST for blocks with >=4 qubits (bqskit_block_synth.py).");
    parser.add_argument<bool>("--qpredict")
        .action(dvlab::argparse::store_true)
        .help("QPredict for large blocks (bqskit_block_synth.py).");
    parser.add_argument<bool>("--prefer-qsearch")
        .action(dvlab::argparse::store_true)
        .help("Try per-block QSearch before native synthesis when bqskit is available.");
    parser.add_argument<bool>("--device-topology")
        .action(dvlab::argparse::store_true)
        .help("Restrict CX gate deletion/alignment to focused device coupling edges.");
    parser.add_argument<bool>("--monolithic")
        .action(dvlab::argparse::store_true)
        .help("Force one-shot 2^n unitary synthesis (legacy; may trigger KAK fallback).");
    parser.add_argument<int>("--opt-level")
        .default_value(1)
        .help("1=qfactor polish, 2=+gate removal, 3=+resynthesis (native). Passed to --u3syn.");
    parser.add_argument<bool>("--three-cnot-kak")
        .action(dvlab::argparse::store_true)
        .help("(Experimental) try the 3-CNOT QFactor-instantiated ansatz for 2-qubit "
              "blocks; slow under coordinate descent, fast after PR-B's LBFGS lands.");
    parser.add_argument<int>("--three-cnot-restarts")
        .default_value(1)
        .help("Number of random restarts for the 3-CNOT QFactor ansatz.");
    parser.add_argument<bool>("--three-cnot-lbfgs")
        .action(dvlab::argparse::store_true)
        .help("Deprecated alias for --lbfgs (kept for backwards compat).");
    parser.add_argument<bool>("--lbfgs")
        .action(dvlab::argparse::store_true)
        .help("PR-B: drive every QFactor instantiation pass (3-CNOT KAK and "
              "post-synthesis polish) with the LBFGS minimiser instead of "
              "coordinate descent. Roughly 10x fewer to_tensor evaluations "
              "on dense U3 parameter sets; recommended whenever --three-cnot-kak "
              "is set, or when opt-level >= 1 needs to polish a non-trivial "
              "block.");
    parser.add_argument<bool>("--pas")
        .action(dvlab::argparse::store_true)
        .help("Permutation-Aware Synthesis: try all qubit permutations for "
              "2-4 qubit blocks and pick the fewest CX.");
}

void apply_u3cx_options(dvlab::argparse::ArgumentParser const& parser, U3CxCompileOptions& u3,
                        qsyn::device::DeviceMgr const* device_mgr = nullptr,
                        size_t n_qubits = 0) {
    u3.use_u3syn        = parser.parsed("--u3syn") || parser.parsed("--bqskit");
    u3.use_bqskit       = u3.use_u3syn;
    u3.use_qsearch      = parser.parsed("--qsearch");
    u3.use_qfast        = parser.parsed("--qfast");
    u3.use_qpredict     = parser.parsed("--qpredict");
    if (parser.parsed("--leap-native")) {
        u3.block_engine = BlockSynthEngine::LeapNative;
    } else if (parser.parsed("--qsearch-native")) {
        u3.block_engine = BlockSynthEngine::QSearchNative;
    } else if (parser.parsed("--leap")) {
        u3.block_engine = BlockSynthEngine::Leap;
    } else if (u3.use_qpredict) {
        u3.block_engine = BlockSynthEngine::QPredict;
    } else if (u3.use_qfast) {
        u3.block_engine = BlockSynthEngine::QFast;
    } else if (u3.use_qsearch) {
        u3.block_engine = BlockSynthEngine::QSearch;
    }
    u3.prefer_qsearch_blocks = parser.parsed("--prefer-qsearch");
    u3.force_monolithic      = parser.parsed("--monolithic");
    if (parser.parsed("--opt-level")) {
        u3.optimization_level = parser.get<int>("--opt-level");
        u3.bqskit_opt_level   = u3.optimization_level;
    }
    u3.try_three_cnot_kak    = parser.parsed("--three-cnot-kak");
    u3.three_cnot_restarts   = parser.get<int>("--three-cnot-restarts");
    u3.three_cnot_use_lbfgs  = parser.parsed("--three-cnot-lbfgs");
    u3.qfactor_use_lbfgs     = parser.parsed("--lbfgs") || u3.three_cnot_use_lbfgs;
    if (parser.parsed("--device-topology") && device_mgr != nullptr &&
        dvlab::utils::mgr_has_data(*device_mgr) && n_qubits > 0) {
        u3.use_device_coupling = true;
        u3.coupling            = coupling_from_device(*device_mgr->get(), n_qubits);
    }
    u3.use_pas = parser.parsed("--pas");
}

void apply_u3cx_compile_args(dvlab::argparse::ArgumentParser const& parser, CpfPipelineOptions& opt,
                           qsyn::device::DeviceMgr const* device_mgr = nullptr,
                           size_t n_qubits = 0) {
    apply_u3cx_options(parser, opt.u3cx, device_mgr, n_qubits);
}

CmdExecResult run_pipeline_on_focused(QCirMgr& qcir_mgr, qsyn::device::DeviceMgr& device_mgr,
                                      ArgumentParser const& parser) {
    if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;

    CpfPipelineOptions opt;
    opt.skip_u3cx         = parser.parsed("--skip-u3cx");
    opt.run_cpf_fold      = !parser.parsed("--no-fold");
    apply_cpf_fold_strategy_arg(parser, opt);
    opt.run_full_optimize = parser.parsed("--full");
    opt.prefer_graysynth  = !parser.parsed("--naive-rotation");
    apply_pauli_compress_arg(parser, opt);
    if (parser.parsed("--rounds")) {
        opt.max_rounds = static_cast<std::size_t>(std::max(1, parser.get<int>("--rounds")));
    }
    apply_u3cx_compile_args(parser, opt, &device_mgr, qcir_mgr.get()->get_num_qubits());

    CpfPipelineStats stats;
    auto const&        src = *qcir_mgr.get();
    auto               out = run_cpf_pipeline(src, stats, opt);
    if (!out.has_value()) return CmdExecResult::error;

    log_pipeline_stats(stats);
    store_qcir(qcir_mgr, std::move(*out), parser.parsed("--replace") || parser.parsed("-r"));
    return CmdExecResult::done;
}

}  // namespace

Command qcir_to_u3cx_cmd(QCirMgr& qcir_mgr, qsyn::device::DeviceMgr& device_mgr) {
    return {"to-u3cx",
            [&](ArgumentParser& parser) {
                parser.description(
                    "Compile the focused QCir to a logical U3+CX circuit (partitioned "
                    "QSD/KAK + partition; --u3syn for BQSKit; --opt-level 1-3).");
                add_u3cx_compile_args(parser);
                parser.add_argument<bool>("-r", "--replace").action(store_true);
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;
                U3CxCompileOptions u3opt;
                apply_u3cx_options(parser, u3opt, &device_mgr, qcir_mgr.get()->get_num_qubits());
                auto out = compile_to_u3_cnot(*qcir_mgr.get(), u3opt);
                if (!out.has_value()) return CmdExecResult::error;
                out->set_filename(qcir_mgr.get()->get_filename());
                out->add_procedures(qcir_mgr.get()->get_procedures());
                store_qcir(qcir_mgr, std::move(*out), parser.parsed("--replace"));
                return CmdExecResult::done;
            }};
}

Command qcir_to_zyz_cmd(QCirMgr& qcir_mgr) {
    return {"to-zyz",
            [&](ArgumentParser& parser) {
                parser.description(
                    "Expand the focused QCir to ZYZ+CX form: single-qubit U3 gates become "
                    "RZ(theta) RY(phi) RZ(lambda); two-qubit gates stay as CX (etc.).");
                parser.add_argument<bool>("-r", "--replace").action(store_true);
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;
                auto out = compile_to_zyz_cnot(*qcir_mgr.get());
                if (!out.has_value()) return CmdExecResult::error;
                out->set_filename(qcir_mgr.get()->get_filename());
                out->add_procedures(qcir_mgr.get()->get_procedures());
                store_qcir(qcir_mgr, std::move(*out), parser.parsed("--replace"));
                return CmdExecResult::done;
            }};
}

Command qcir_to_tableau_cmd(QCirMgr& qcir_mgr, experimental::TableauMgr& tableau_mgr) {
    return {"to-tableau",
            [&](ArgumentParser& parser) {
                parser.description(
                    "Convert the focused QCir to a canonical Tableau using trace-replay, "
                    "optional dag_fold, collapse, and lossless pauli-compress (D-merge + F-cancel).");
                parser.add_argument<bool>("--fold")
                    .action(store_true)
                    .help("Run dag_fold + collapse after trace-replay.");
                parser.add_argument<bool>("--no-pauli-compress")
                    .action(store_true)
                    .help("Skip lossless pauli-compress (D-merge + F-cancel) after collapse.");
                parser.add_argument<bool>("-r", "--replace")
                    .action(store_true)
                    .help("Replace the focused Tableau if one is already checked out "
                          "(otherwise add a new Tableau).");
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;

                auto tableau = build_cpf_tableau(*qcir_mgr.get());
                if (!tableau.has_value()) return CmdExecResult::error;

                CpfPipelineStats stats;
                stats.rotations_after_trace_replay = tableau->n_pauli_rotations();
                stats.cliffords_after_trace_replay = tableau->n_cliffords();

                CpfPipelineOptions opt;
                opt.run_cpf_fold      = parser.parsed("--fold");
                opt.use_dag_fold      = parser.parsed("--fold");
                opt.run_full_optimize = false;
                apply_pauli_compress_arg(parser, opt);
                apply_cpf_fold(*tableau, stats, opt);
                canonicalize_cpf_tableau(*tableau, stats);
                apply_lossless_pauli_compress(*tableau, stats, opt);
                log_cpf_pipeline_step_counts(stats, opt.run_cpf_fold);

                if (parser.parsed("--replace") && dvlab::utils::mgr_has_data(tableau_mgr)) {
                    *tableau_mgr.get() = std::move(*tableau);
                } else {
                    tableau_mgr.add(tableau_mgr.get_next_id(),
                                    std::make_unique<experimental::Tableau>(std::move(*tableau)));
                }
                tableau_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                tableau_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                tableau_mgr.get()->add_procedure(
                    parser.parsed("--fold") ? "QC2TABL-CPF-Collapse" : "QC2TABL-Trace-Collapse");
                if (opt.run_lossless_pauli_compress) {
                    tableau_mgr.get()->add_procedure("PauliCompress-Lossless");
                }
                return CmdExecResult::done;
            }};
}

Command qcir_from_tableau_cmd(QCirMgr& qcir_mgr, experimental::TableauMgr& tableau_mgr) {
    return {"from-tableau",
            [&](ArgumentParser& parser) {
                parser.description(
                    "Synthesise the focused Tableau back to a QCir. Default: Gray-synth "
                    "for diagonal Pauli-rotation blocks, naive fallback otherwise.");
                parser.add_argument<bool>("--naive-rotation")
                    .action(store_true)
                    .help("Always use naive rotation synthesis (skip Gray-synth).");
                parser.add_argument<bool>("-r", "--replace").action(store_true);
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                if (!dvlab::utils::mgr_has_data(tableau_mgr)) return CmdExecResult::error;

                CpfPipelineStats stats;
                CpfPipelineOptions opt;
                opt.prefer_graysynth = !parser.parsed("--naive-rotation");

                auto out = synthesize_tableau_to_qcir(*tableau_mgr.get(), stats, opt);
                if (!out.has_value()) return CmdExecResult::error;

                if (stats.used_graysynth_per_block) {
                    spdlog::info("from-tableau: used Gray-synth for at least one rotation block.");
                }
                if (stats.used_naive_fallback) {
                    spdlog::warn("from-tableau: non-diagonal block(s) synthesised with naive.");
                }
                out->set_filename(tableau_mgr.get()->get_filename());
                out->add_procedures(tableau_mgr.get()->get_procedures());
                store_qcir(qcir_mgr, std::move(*out), parser.parsed("--replace"));
                return CmdExecResult::done;
            }};
}

Command qcir_cpf_pipeline_cmd(QCirMgr& qcir_mgr, qsyn::device::DeviceMgr& device_mgr) {
    return {"cpf-pipeline",
            [&](ArgumentParser& parser) {
                parser.description(
                    "Full CPF product pipeline on the focused QCir:\n"
                    "  to-u3cx -> to-zyz -> trace_replay -> dag_fold -> collapse "
                    "-> pauli-compress (lossless) -> Gray-synth (QCir).\n"
                    "Same as the top-level `qcpfq` shortcut.");
                parser.add_argument<bool>("--skip-u3cx")
                    .action(store_true)
                    .help("Skip the U3+CX synthesis step (input already in a rotation basis).");
                add_u3cx_compile_args(parser);
                parser.add_argument<bool>("--no-fold")
                    .action(store_true)
                    .help("Skip CPF fold (only extract tableau structure).");
                parser.add_argument<bool>("--no-dag-fold")
                    .action(store_true)
                    .help("Use legacy global_fold only (skip Pauli-DAG global prune).");
                parser.add_argument<std::string>("--fold-strategy")
                    .default_value("dag")
                    .help("dag | global | staq | no_prune | matching");
                parser.add_argument<int>("--rounds")
                    .default_value(3)
                    .help("Retranspile rounds (Python cpf_global max_rounds; default 3).");
                parser.add_argument<bool>("--full")
                    .action(store_true)
                    .help("Also run full_optimize (TODD etc.) before synthesis.");
                parser.add_argument<bool>("--no-pauli-compress")
                    .action(store_true)
                    .help("Skip lossless pauli-compress (D-merge + F-cancel) after collapse.");
                parser.add_argument<bool>("--naive-rotation")
                    .action(store_true)
                    .help("Use naive rotation synthesis instead of Gray-synth.");
                parser.add_argument<bool>("-r", "--replace").action(store_true);
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                return run_pipeline_on_focused(qcir_mgr, device_mgr, parser);
            }};
}

bool add_qcpfq_cmd(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr,
                   qsyn::device::DeviceMgr& device_mgr) {
    return cli.add_command(
        {"qcpfq",
         [&](ArgumentParser& parser) {
             parser.description(
                 "CPF product pipeline on the focused QCir (replace in-place by default).\n"
                 "  U3+CX -> ZYZ+CX -> trace_replay -> dag_fold -> collapse "
                 "-> pauli-compress (lossless) -> Gray-synth QCir");
             parser.add_argument<bool>("--skip-u3cx").action(store_true);
             add_u3cx_compile_args(parser);
             parser.add_argument<bool>("--no-fold").action(store_true);
             parser.add_argument<bool>("--no-dag-fold").action(store_true);
             parser.add_argument<std::string>("--fold-strategy").default_value("dag");
             parser.add_argument<int>("--rounds").default_value(3);
             parser.add_argument<bool>("--full").action(store_true);
             parser.add_argument<bool>("--no-pauli-compress").action(store_true);
             parser.add_argument<bool>("--naive-rotation").action(store_true);
             parser.add_argument<bool>("--keep")
                 .action(store_true)
                 .help("Keep the original QCir (add a new one) instead of replacing.");
         },
         [&](ArgumentParser const& parser) -> CmdExecResult {
             if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;

             CpfPipelineOptions opt;
             opt.skip_u3cx         = parser.parsed("--skip-u3cx");
             opt.run_cpf_fold      = !parser.parsed("--no-fold");
             apply_cpf_fold_strategy_arg(parser, opt);
             opt.run_full_optimize = parser.parsed("--full");
             opt.prefer_graysynth  = !parser.parsed("--naive-rotation");
             apply_pauli_compress_arg(parser, opt);
             if (parser.parsed("--rounds")) {
                 opt.max_rounds = static_cast<std::size_t>(std::max(1, parser.get<int>("--rounds")));
             }
             apply_u3cx_compile_args(parser, opt, &device_mgr, qcir_mgr.get()->get_num_qubits());

             CpfPipelineStats stats;
             auto               out = run_cpf_pipeline(*qcir_mgr.get(), stats, opt);
             if (!out.has_value()) return CmdExecResult::error;

             log_pipeline_stats(stats);
             store_qcir(qcir_mgr, std::move(*out), !parser.parsed("--keep"));
             return CmdExecResult::done;
         }});
}

}  // namespace qsyn::qcir
