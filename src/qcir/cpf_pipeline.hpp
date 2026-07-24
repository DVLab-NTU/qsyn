/****************************************************************************
  PackageName  [ qcir / cpf_pipeline ]
  Synopsis     [ Target CPF product pipeline: U3+CX -> ZYZ+CX -> trace_replay
                 -> dag_fold -> collapse -> pauli-compress (lossless) -> QCir. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "qcir/circuit_compile.hpp"
#include "qcir/qcir.hpp"
#include "tableau/cpf/global_fold.hpp"
#include "tableau/cpf/pauli_compress.hpp"
#include "tableau/pauli_dag/dag_fold.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::qcir {

// Tableau fold strategy (interleaved timeline; see `docs/cpf_strategy_bench.md`).
enum class CpfFoldStrategy {
    dag,              // default: staq + graph merge + global_prune + global_fold
    global_only,      // legacy `global_fold` only
    staq_only,        // staq backward fold sweeps only
    no_global_prune,  // dag without Pauli-label global prune
    matching,         // dag with max matching on merge edges (Cole-legal pairs)
};

[[nodiscard]] CpfFoldStrategy parse_cpf_fold_strategy(std::string const& name);

struct CpfPipelineStats {
    std::size_t rotations_after_trace_replay = 0;
    std::size_t rotations_before_fold      = 0;
    std::size_t rotations_after_fold       = 0;
    std::size_t rotations_after_collapse   = 0;
    std::size_t rotations_after_pauli_compress = 0;
    std::size_t cliffords_after_trace_replay = 0;
    std::size_t cliffords_after_collapse   = 0;
    std::size_t cliffords                  = 0;
    std::size_t n_rounds                   = 1;
    experimental::cpf::GlobalFoldStats fold_stats{};
    experimental::cpf::DagFoldStats  dag_fold_stats{};
    bool                              used_dag_fold            = false;
    bool                              ran_cpf_fold             = false;
    bool                              used_graysynth_per_block = false;
    bool                              used_naive_fallback      = false;
    bool                              ran_lossless_pauli_compress = false;
    experimental::cpf::PauliCompressStats pauli_compress_stats{};
};

struct CpfPipelineOptions {
    // Skip logical U3+CX re-synthesis (use when the input is already
    // rz/ry/cx-native, e.g. micro-benchmarks).
    bool skip_u3cx = false;
    // U3+CX compile: partitioned QSD/KAK or external BQSKit.
    U3CxCompileOptions u3cx{};
    // Run CPF fold after building the interleaved tableau.
    bool run_cpf_fold = true;
    // Use `dag_fold` (global Pauli-label prune + `global_fold`) instead of
    // `global_fold` alone. Default on for the product pipeline.
    bool use_dag_fold = true;
    // Fine-grained fold strategy when `use_dag_fold` is true (`dag` ignores
    // the legacy bool except `global_only` via `use_dag_fold=false`).
    CpfFoldStrategy fold_strategy = CpfFoldStrategy::dag;
    // Retranspile rounds (Python `cpf_global_from_rz` max_rounds). Each round
    // runs ZYZ+tableau fold+from-tableau on the previous output.
    std::size_t max_rounds = 3;
    // Run qsyn `full_optimize` (includes TODD; Clifford+T oriented).
    bool run_full_optimize = false;
    // Prefer Gray-synth for diagonal rotation blocks; naive otherwise.
    bool prefer_graysynth = true;
    // Step 3b -- lossless D-merge + F-cancel (`pauli_compress` with L2 budget 0).
    bool run_lossless_pauli_compress = true;
};

// Single pipeline round (steps 1-4) without the outer retranspile loop.
[[nodiscard]] std::optional<QCir> run_cpf_pipeline_round(QCir const& src,
                                                         CpfPipelineStats& stats,
                                                         CpfPipelineOptions const& opt);

// Step 1 -- BQSKit-style logical synthesis: arbitrary QCir -> U3 + CX.
[[nodiscard]] std::optional<QCir> compile_to_u3_cnot(QCir const& src,
                                                     U3CxCompileOptions const& opt = {});

// Step 2 -- Expand single-qubit unitaries to ZYZ (RZ, RY, RZ) + keep CX.
[[nodiscard]] std::optional<QCir> compile_to_zyz_cnot(QCir const& src);

// Step 3 -- `trace_replay` interleaved Tableau (Clifford || PauliRotation).
[[nodiscard]] std::optional<experimental::Tableau> build_cpf_tableau(QCir const& src);

// Step 3b -- optional CPF fold on an existing tableau.
void apply_cpf_fold(experimental::Tableau& tableau, CpfPipelineStats& stats,
                    CpfPipelineOptions const& opt);

// Step 3c -- collapse interleaved tableau to canonical [Clifford | PauliRotation[]].
void canonicalize_cpf_tableau(experimental::Tableau& tableau, CpfPipelineStats& stats);

// Step 3d -- lossless pauli-compress (D-merge + F-cancel) on canonical tableau.
void apply_lossless_pauli_compress(experimental::Tableau& tableau, CpfPipelineStats& stats,
                                 CpfPipelineOptions const& opt);

// Log per-step Pauli-rotation counts (trace_replay / fold / collapse / pauli-compress).
void log_cpf_pipeline_step_counts(CpfPipelineStats const& stats, bool ran_fold);

// Step 4 -- Tableau -> QCir; Gray-synth per diagonal block, naive fallback.
[[nodiscard]] std::optional<QCir> synthesize_tableau_to_qcir(
    experimental::Tableau const& tableau, CpfPipelineStats& stats,
    CpfPipelineOptions const& opt);

// Full pipeline (steps 1-4) on a copy of `src`.
[[nodiscard]] std::optional<QCir> run_cpf_pipeline(QCir const& src, CpfPipelineStats& stats,
                                                   CpfPipelineOptions const& opt = {});

}  // namespace qsyn::qcir
