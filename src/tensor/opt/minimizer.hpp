/****************************************************************************
  PackageName  [ tensor / opt ]
  Synopsis     [ Generic unconstrained minimiser interface and two
                 concrete drivers:
                   - CoordinateDescentMinimizer: 1-D probe loop, the
                     original QFactor-lite behaviour.
                   - LBFGSMinimizer: Limited-memory BFGS with Armijo
                     backtracking line search. Uses CostFunction
                     gradient() (finite differences by default) and
                     typically converges in O(10) outer iterations,
                     which is ~10x fewer to_tensor() evaluations than
                     coordinate descent on the 8-U3 KAK 3-CNOT ansatz.

                 Both drivers are pure consumers of `CostFunction`: they
                 do not know whether the parameters describe a KAK
                 ansatz, a search-tree leaf, or anything else. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "./cost.hpp"

namespace qsyn::tensor::opt {

struct MinimizeOptions {
    // Outer iteration cap.
    std::size_t max_iterations = 100;
    // Stop when residual <= tolerance.
    double tolerance = 1e-8;
    // Stop when ||grad||_inf <= grad_tolerance.
    double grad_tolerance = 1e-10;
    // 0 = silent, 1 = summary, 2 = per-iteration.
    std::size_t verbosity = 1;

    // -------- LBFGS-specific --------
    // History length (number of (s, y) pairs retained).
    std::size_t lbfgs_history = 7;
    // FD step used by CostFunction::gradient default.
    double lbfgs_fd_step = 1e-4;
    // Armijo backtracking constant (f_new <= f_x + c1 * step * g.d).
    double armijo_c1 = 1e-4;
    // Max # of backtracking probes per line search.
    std::size_t ls_max_steps = 25;
    // Initial line-search step.
    double ls_initial = 1.0;
    // Backtracking shrink factor.
    double ls_shrink = 0.5;

    // -------- Coordinate-descent specific --------
    double cd_initial_step = 0.4;
    double cd_step_shrink  = 0.5;
    double cd_min_step     = 1e-9;
};

struct MinimizeResult {
    std::vector<double> x;
    double initial_value     = 0.0;
    double final_value       = 0.0;
    std::size_t n_iterations = 0;
    bool converged           = false;
};

class Minimizer {
public:
    virtual ~Minimizer()                                             = default;
    virtual MinimizeResult minimize(CostFunction& f,
                                    std::vector<double> const& x0,
                                    MinimizeOptions const& opt = {}) = 0;
};

class CoordinateDescentMinimizer final : public Minimizer {
public:
    MinimizeResult minimize(CostFunction& f,
                            std::vector<double> const& x0,
                            MinimizeOptions const& opt) override;
};

class LBFGSMinimizer final : public Minimizer {
public:
    MinimizeResult minimize(CostFunction& f,
                            std::vector<double> const& x0,
                            MinimizeOptions const& opt) override;
};

enum class MinimizerKind { CoordinateDescent,
                           LBFGS };

inline std::unique_ptr<Minimizer> make_minimizer(MinimizerKind kind) {
    switch (kind) {
        case MinimizerKind::LBFGS:
            return std::make_unique<LBFGSMinimizer>();
        case MinimizerKind::CoordinateDescent:
            return std::make_unique<CoordinateDescentMinimizer>();
    }
    return std::make_unique<CoordinateDescentMinimizer>();
}

}  // namespace qsyn::tensor::opt
