/****************************************************************************
  PackageName  [ tensor / qfactor ]
  Synopsis     [ QFactor-lite: numerical instantiation that holds the gate
                 topology fixed and optimises every U3 parameter so that the
                 ansatz reproduces a target unitary.

                 This is intentionally a *lite* port of the BQSKit /
                 QFactor-JAX algorithm: we trade analytic per-gate
                 environment computation for a robust coordinate-descent
                 driver that only needs the existing `to_tensor(QCir)` and
                 `cosine_similarity` primitives.  It scales to the same
                 circuit sizes that `qcir synthesize` already supports
                 (~5 qubits, dozens of gates) and is intended as a polish
                 pass after KAK/QSD or CPF-driven extraction.

                 NOTE -- Connectivity / coupling-graph awareness is OUT
                 OF SCOPE; BQSKit's own QFactor likewise ignores topology
                 and operates on a fixed (logical) gate ansatz. Use
                 qsyn's `device` + `qcir optimize --physical` for the
                 mapping side; QFactor-lite simply polishes the U3
                 parameters of whatever ansatz the caller hands in. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::tensor::qfactor {

enum class QFactorStrategy {
    CoordinateDescent,  // original probe loop (default; cheap per-iter but
                        // hundreds of passes)
    LBFGS,              // limited-memory BFGS with backtracking line search
                        // (~10x fewer to_tensor calls on dense ansatzes)
};

struct QFactorOptions {
    // Hard cap on outer iterations (passes for coord-descent, iter for LBFGS).
    std::size_t max_iterations = 200;
    // Stop early if the cosine-distance residual drops below this.
    double tolerance = 1e-8;
    // Initial step size, in radians, used for the +/- probe. Halved every
    // pass that produces no improvement. (Coord-descent only.)
    double initial_step = 0.4;
    // Multiplicative factor applied to the step on each stagnant pass.
    double step_shrink = 0.5;
    // Floor below which the step is considered "exhausted" and the
    // search terminates.
    double min_step = 1e-9;
    // 0 = quiet, 1 = summary log line, 2 = per-pass progress.
    std::size_t verbosity = 1;
    // Which numerical optimiser to use.
    QFactorStrategy strategy = QFactorStrategy::CoordinateDescent;
    // LBFGS history length; ignored when strategy = CoordinateDescent.
    std::size_t lbfgs_history = 7;
    // Finite-difference step used by LBFGS gradient.
    double lbfgs_fd_step = 1e-4;
};

struct QFactorResult {
    // Residual at iteration 0 (before any tuning).
    double initial_residual = 0.0;
    // Residual at the last accepted pass.
    double final_residual = 0.0;
    // Number of outer coordinate-descent passes that ran.
    std::size_t n_passes = 0;
    // True when `final_residual <= tolerance`.
    bool converged = false;
    // Number of UGate parameters discovered (3 per UGate).  Zero means
    // there is nothing to tune and the routine is a no-op.
    std::size_t n_parameters = 0;
};

// Tune every UGate in `ansatz` (in place) so that
//   cosine_similarity(to_tensor(ansatz), target)
// is maximised.  Non-UGate gates (CX, H, RZ, ...) are kept untouched -
// the QFactor philosophy is "trust the topology, polish the leaves".
// Returns the final residual + convergence flag for the caller to log.
[[nodiscard]] QFactorResult instantiate(qcir::QCir& ansatz,
                                        QTensor<double> const& target,
                                        QFactorOptions const& opt = {});

}  // namespace qsyn::tensor::qfactor
