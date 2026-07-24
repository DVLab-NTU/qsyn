/****************************************************************************
  PackageName  [ tensor / qsd ]
  Synopsis     [ Quantum Shannon Decomposition (QSD) for arbitrary n-qubit
                 unitaries. Recursive structure that bottoms out at PR-6's
                 KAK / ZYZ synthesisers; the general-n recursion is left as
                 a TODO and currently falls through to qsyn's existing
                 gray-code `tensor::Decomposer` for n >= 3.

                 Reference:
                   Shende, Bullock, Markov,
                   "Synthesis of Quantum Logic Circuits", DAC'04. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

// NOTE -- Connectivity / coupling-graph awareness is OUT OF SCOPE for the
// qsyn QSD dispatcher. BQSKit's upstream `QSDPass` is itself a logical
// (all-to-all) pass; physical routing is handled separately by
// `bqskit.passes.mapping.*`. The qsyn counterpart sits in the `device` +
// `duostra` modules and `qcir optimize --physical` and is intentionally
// left disabled in the synthesis path until a dedicated connectivity
// PR ports the BQSKit `setmodel.py` workflow.
#pragma once

#include <optional>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::tensor::qsd {

using qcir::QCir;

// Top-level dispatcher.
//   n == 1: ZYZ -> single U3 gate (via kak::single_qubit_synthesize).
//   n == 2: KAK -> 6-CNOT + 4 outer U3s (via kak::two_qubit_synthesize,
//                 with gray-code fallback if KAK fails).
//   n >= 3: Quantum Shannon Decomposition.  Current implementation:
//
//                U = (V_1 \oplus V_2)  *  CS  *  (W_1 \oplus W_2)^dagger
//
//           where the V_i, W_i are (n-1)-qubit unitaries and CS is a
//           uniformly-controlled rotation block. The cosine-sine
//           decomposition (CSD) producing this factorisation is provided
//           by xtensor-blas SVD; for now we expose a scaffold and fall
//           back to `tensor::Decomposer` for actual circuit emission.
//
struct QSDOptions {
    // BQSKit FullQSDPass: ScanningGateRemovalPass between decomposition rounds.
    bool inter_round_gate_removal = false;
    double synthesis_epsilon      = 1e-8;
    // Forwarded to `kak::two_qubit_synthesize`. Off by default because
    // the 3-CNOT QFactor candidate is slow under coordinate descent.
    bool try_three_cnot_qfactor = false;
    int three_cnot_restarts     = 1;
    // Use LBFGS minimiser (PR-B) for the 3-CNOT QFactor ansatz instead
    // of coordinate descent. Roughly an order of magnitude fewer
    // to_tensor calls on the 24-parameter ansatz.
    bool three_cnot_use_lbfgs = false;
};

// Returns std::nullopt if all strategies fail.
[[nodiscard]] std::optional<QCir>
synthesize(QTensor<double> const& matrix, QSDOptions const& opt = {});

}  // namespace qsyn::tensor::qsd
