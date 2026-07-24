/****************************************************************************
  PackageName  [ qcir / circuit_compile ]
  Synopsis     [ BQSKit-style partitioned compile to U3+CX. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>

#include "qcir/coupling_constraints.hpp"
#include "qcir/qcir.hpp"
#include "qcir/quick_partitioner.hpp"

namespace qsyn::device {
class Device;
}

namespace qsyn::qcir {

enum class BlockSynthEngine {
    Native,
    QSearch,        // External BQSKit QSearch (subprocess).
    Leap,           // External BQSKit LEAP   (subprocess).
    QFast,
    QPredict,
    QSearchNative,  // PR-C native QSearch (no external subprocess).
    LeapNative,     // PR-C native LEAP    (no external subprocess).
};

struct U3CxCompileOptions {
    // BQSKit QuickPartitioner block size (max distinct qubits per unitary region).
    size_t max_block_qubits = 3;
    PartitionStrategy partition_strategy = PartitionStrategy::QuickScan;
    bool              partition_merge_regions = true;
    // After partition, split at CX/entangling boundaries (native KAK path).
    bool partition_at_entangling = true;
    // Use monolithic to_tensor + QSD when n_qubits <= this (2^n matrix fits in RAM).
    size_t monolithic_max_qubits = 4;
    // External full compile via `scripts/bqskit_u3cx_compile.py`.  --bqskit is an alias.
    bool use_u3syn = false;
    bool use_bqskit = false;
    // Per-block QSearch/LEAP via `scripts/bqskit_block_synth.py` (falls back to native).
    bool use_qsearch = false;
    bool use_qfast   = false;
    bool use_qpredict = false;
    // Try QSearch per block when bqskit_block_synth.py exists (before native).
    bool prefer_qsearch_blocks = false;
    BlockSynthEngine block_engine = BlockSynthEngine::Native;
    CouplingConstraints coupling{};
    bool use_device_coupling = false;
    // QSD FullQSDPass: scanning gate removal between CSD rounds.
    bool qsd_inter_round_scan = true;
    // Try the 3-CNOT QFactor-instantiated ansatz for 2-qubit blocks.
    // Off by default (slow with coordinate descent); becomes practical
    // after PR-B lands a real LBFGS minimizer.
    bool   try_three_cnot_kak  = false;
    int    three_cnot_restarts = 1;
    // PR-B opt-in: use the LBFGS minimiser whenever QFactor is invoked
    // (both the 3-CNOT KAK ansatz and the post-synthesis polish pass).
    // Order of magnitude fewer to_tensor evaluations than coordinate
    // descent on dense parameter sets.
    bool   qfactor_use_lbfgs   = false;
    // Convenience alias kept for callers that only care about the KAK
    // path. `qfactor_use_lbfgs` overrides this when set.
    bool   three_cnot_use_lbfgs = false;
    // Maps to BQSKit compile(optimization_level=...); native path uses 1–3 only.
    int optimization_level = 1;
    int  bqskit_opt_level = 1;
    bool force_monolithic = false;
    double synthesis_epsilon = 1e-8;
    // Use QFAST for blocks with at least this many qubits (when use_qfast).
    size_t qfast_min_qubits = 4;
    // Permutation-Aware Synthesis for 2..pas_max_qubits blocks (BQSKit PAS).
    bool   use_pas           = false;
    size_t pas_max_qubits    = 4;
};

[[nodiscard]] BlockSynthEngine effective_block_engine(U3CxCompileOptions const& opt);

[[nodiscard]] bool use_external_u3syn(U3CxCompileOptions const& opt);
[[nodiscard]] bool use_external_block_synth(U3CxCompileOptions const& opt);

// Retarget RZ/RY/RX/PZ/... single-qubit gates to U3; keep CX and U3.
// Also rebases ECR/SWAP/CZ/CY/iSWAP/... 2-qubit gates to U3+CX via KAK
// (per-gate). 3+-qubit gates (CCX/CCZ/MCX) are passed through and must
// be decomposed by an upstream partition + QSD pass.
[[nodiscard]] QCir retarget_to_u3_cx(QCir const& src);

// Hard contract on a U3+CX output: every gate is either a UGate or a CXGate.
[[nodiscard]] bool circuit_is_u3_cx_only(QCir const& circ);

// Partitioned or monolithic compile to U3+CX.
[[nodiscard]] std::optional<QCir> compile_to_u3_cnot_impl(QCir const& src,
                                                         U3CxCompileOptions const& opt = {});

}  // namespace qsyn::qcir
