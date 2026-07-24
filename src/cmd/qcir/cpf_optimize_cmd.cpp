/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ `qcir cpf-optimize` -- alias for the CPF product pipeline. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "qcir/cpf_pipeline.hpp"
#include "qcir/qcir.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

Command qcir_cpf_optimize_cmd(QCirMgr& qcir_mgr) {
    return {
        "cpf-optimize",
        [&](ArgumentParser& parser) {
            parser.description(
                "End-to-end CPF product pipeline (same as `qcpfq` / `qcir cpf-pipeline`):\n"
                "  U3+CX -> ZYZ+CX -> trace_replay -> dag_fold -> collapse "
                "-> pauli-compress (lossless) -> Gray-synth QCir.\n"
                "Legacy flag `--no-full` skips optional `full_optimize` (TODD); "
                "use `--full` on `cpf-pipeline` to enable it.");

            parser.add_argument<bool>("--skip-u3cx")
                .action(store_true)
                .help("Skip U3+CX re-synthesis when the circuit is already rotation-native.");

            parser.add_argument<bool>("--no-full")
                .action(store_true)
                .help("Legacy alias: same as omitting `--full` (full_optimize is off by default).");

            parser.add_argument<bool>("--full")
                .action(store_true)
                .help("Run full_optimize (tmerge + hopt + TODD) after global_fold.");

            parser.add_argument<bool>("--naive-rotation")
                .action(store_true)
                .help("Use naive rotation synthesis instead of Gray-synth.");

            parser.add_argument<bool>("--no-dag-fold")
                .action(store_true)
                .help("Use legacy global_fold only (skip Pauli-DAG global prune).");

            parser.add_argument<std::string>("--fold-strategy")
                .default_value("dag")
                .help("Tableau fold: dag | global | staq | no_prune | matching (see docs/cpf_strategy_bench.md).");

            parser.add_argument<int>("--rounds")
                .default_value(3)
                .help("Retranspile rounds (default 3).");

            parser.add_argument<bool>("--no-pauli-compress")
                .action(store_true)
                .help("Skip lossless pauli-compress (D-merge + F-cancel) after collapse.");

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help("Replace the focused QCir in-place with the optimised one.");
        },
        [&](ArgumentParser const& parser) -> CmdExecResult {
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;

            CpfPipelineOptions opt;
            opt.skip_u3cx          = parser.parsed("--skip-u3cx");
            opt.run_cpf_fold       = true;
            opt.use_dag_fold       = !parser.parsed("--no-dag-fold");
            if (parser.parsed("--fold-strategy")) {
                opt.fold_strategy = parse_cpf_fold_strategy(parser.get<std::string>("--fold-strategy"));
                if (opt.fold_strategy != CpfFoldStrategy::global_only) {
                    opt.use_dag_fold = true;
                }
            }
            opt.run_full_optimize  = parser.parsed("--full");
            opt.prefer_graysynth   = !parser.parsed("--naive-rotation");
            if (parser.parsed("--no-pauli-compress")) {
                opt.run_lossless_pauli_compress = false;
            }
            if (parser.parsed("--rounds")) {
                opt.max_rounds = static_cast<std::size_t>(std::max(1, parser.get<int>("--rounds")));
            }

            CpfPipelineStats stats;
            auto               out = run_cpf_pipeline(*qcir_mgr.get(), stats, opt);
            if (!out.has_value()) return CmdExecResult::error;

            spdlog::info("cpf-optimize: rotations {} -> {} ({} rounds{})",
                         stats.rotations_after_trace_replay, stats.rotations_after_pauli_compress,
                         stats.n_rounds,
                         stats.used_dag_fold ? ", dag_fold" : "");
            log_cpf_pipeline_step_counts(stats, stats.ran_cpf_fold);
            // Machine-readable line for scripts/cpf_bench.py (works with -q).
            fmt::print("cpf-optimize: strategy {} rotations {} -> {}\n",
                        parser.parsed("--fold-strategy")
                            ? parser.get<std::string>("--fold-strategy")
                            : (parser.parsed("--no-dag-fold") ? "global" : "dag"),
                        stats.rotations_after_trace_replay, stats.rotations_after_pauli_compress);

            out->set_filename(qcir_mgr.get()->get_filename());
            out->add_procedures(qcir_mgr.get()->get_procedures());
            out->add_procedure(parser.parsed("--full") ? "CPF-Optimize-Full" : "CPF-Optimize");

            if (parser.parsed("--replace")) {
                *qcir_mgr.get() = std::move(*out);
            } else {
                qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<QCir>(std::move(*out)));
            }
            return CmdExecResult::done;
        }};
}

}  // namespace qsyn::qcir
