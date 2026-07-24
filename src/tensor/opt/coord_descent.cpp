/****************************************************************************
  PackageName  [ tensor / opt ]
  Synopsis     [ Coordinate-descent minimiser. Faithful re-implementation
                 of the original QFactor-lite probe loop, but pluggable
                 through the generic `Minimizer` interface so that other
                 cost functions (search-tree leaves in PR-C, etc.) can
                 share the same driver. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <vector>

#include "./minimizer.hpp"

namespace qsyn::tensor::opt {

MinimizeResult CoordinateDescentMinimizer::minimize(CostFunction& f,
                                                    std::vector<double> const& x0,
                                                    MinimizeOptions const& opt) {
    MinimizeResult res;
    res.x             = x0;
    res.initial_value = f.evaluate(res.x);
    res.final_value   = res.initial_value;

    auto const n = res.x.size();
    if (n == 0) {
        res.converged = res.final_value <= opt.tolerance;
        return res;
    }

    if (opt.verbosity >= 1) {
        spdlog::info("coord-descent: starting ({} params, initial residual = {:.3e})",
                     n, res.initial_value);
    }

    double step      = opt.cd_initial_step;
    double best      = res.initial_value;
    std::size_t pass = 0;

    while (pass < opt.max_iterations && best > opt.tolerance && step >= opt.cd_min_step) {
        bool improved = false;
        for (std::size_t i = 0; i < n; ++i) {
            double const saved = res.x[i];

            res.x[i]            = saved + step;
            double const f_plus = f.evaluate(res.x);

            res.x[i]             = saved - step;
            double const f_minus = f.evaluate(res.x);

            if (f_plus < best - 1e-15 && f_plus <= f_minus) {
                res.x[i] = saved + step;
                best     = f_plus;
                improved = true;
            } else if (f_minus < best - 1e-15) {
                // -step is already in place.
                best     = f_minus;
                improved = true;
            } else {
                res.x[i] = saved;
                // Restore-evaluate would be wasteful; the next probe
                // overwrites another coordinate, and a final evaluate
                // happens at loop bottom.
            }
        }

        ++pass;
        if (opt.verbosity >= 2) {
            spdlog::info("coord-descent: pass {:3d}  step={:.2e}  residual={:.3e}  {}",
                         pass, step, best, improved ? "(improved)" : "(stagnant)");
        }
        if (!improved) step *= opt.cd_step_shrink;
    }

    res.n_iterations = pass;
    res.final_value  = best;
    res.converged    = best <= opt.tolerance;

    if (opt.verbosity >= 1) {
        spdlog::info("coord-descent: finished after {} passes, residual {:.3e} -> {:.3e}{}",
                     res.n_iterations, res.initial_value, res.final_value,
                     res.converged ? " (converged)" : "");
    }
    return res;
}

}  // namespace qsyn::tensor::opt
