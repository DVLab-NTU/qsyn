/****************************************************************************
  PackageName  [ synthesis / search ]
  Synopsis     [ Native QSearch / LEAP synthesizers.

                 QSearch (Davis et al. 2020) is a best-first tree search
                 over partial circuits: pop the most promising leaf,
                 expand via a LayerGenerator, instantiate each child's
                 free U3 parameters with QFactor (Minimizer of choice),
                 push back to frontier. Stop when residual <= eps.

                 LEAP (Smith et al. 2021) augments QSearch with a
                 "prefix-freeze" trick: every K layers (default 4), the
                 algorithm commits the best path so far and restarts the
                 search from the committed prefix. This keeps the tree
                 size bounded and trades a small amount of solution
                 quality for a large speedup at deep targets.

                 Both routines target the U3 + CX gate set and assume
                 all-to-all connectivity. Coupling-map awareness is OUT
                 OF SCOPE -- callers route afterwards. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>

#include "qcir/qcir.hpp"
#include "tensor/opt/minimizer.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::synthesis::search {

template <typename T> using QTensor = qsyn::tensor::QTensor<T>;

struct QSearchOptions {
    // Stop when residual <= success_threshold.
    double      success_threshold = 1e-6;
    // Max number of layers to add before giving up.
    std::size_t max_depth         = 6;
    // Max number of frontier pops before giving up.
    std::size_t max_iterations    = 256;
    // QFactor budget per child instantiation. Smaller = faster but
    // worse residual; the default keeps each pop O(seconds) on small
    // (<=4 qubit) targets.
    std::size_t instantiate_iters = 60;
    // Heuristic weighting for AStarHeuristic.  Higher = more greedy.
    double      heuristic_alpha   = 0.05;
    // Minimizer used by the instantiate inner loop.  LBFGS is the
    // PR-B default; CoordinateDescent is kept as a fallback for cases
    // where LBFGS fails to make progress (degenerate Jacobian etc.).
    tensor::opt::MinimizerKind minimizer = tensor::opt::MinimizerKind::LBFGS;
    // Verbosity: 0 silent, 1 per-pop summary, 2 per-pop + per-child.
    std::size_t verbosity         = 1;
};

struct LeapOptions : QSearchOptions {
    // Commit the best path every `prefix_freeze_period` layers and
    // restart the search from that prefix.
    std::size_t prefix_freeze_period = 4;
    // Max number of freeze rounds before bailing out.
    std::size_t max_freeze_rounds    = 8;
};

// Search for a U3+CX circuit equivalent to `target`. Returns nullopt
// when the search budget is exhausted without reaching the threshold.
[[nodiscard]] std::optional<qcir::QCir>
qsearch_synthesize(QTensor<double> const& target, QSearchOptions const& opt = {});

// LEAP variant: same call, but with the prefix-freeze loop.
[[nodiscard]] std::optional<qcir::QCir>
leap_synthesize(QTensor<double> const& target, LeapOptions const& opt = {});

}  // namespace qsyn::synthesis::search
