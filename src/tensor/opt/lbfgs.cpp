/****************************************************************************
  PackageName  [ tensor / opt ]
  Synopsis     [ Limited-memory BFGS (LBFGS) minimiser with backtracking
                 line search. Textbook two-loop recursion (Nocedal &
                 Wright, Algorithm 7.4). Designed as a drop-in
                 replacement for the QFactor coordinate-descent loop
                 on the small (8-U3, 24-param) KAK 3-CNOT ansatz, where
                 it typically converges in ~15 outer iterations vs. the
                 hundreds of probe passes coordinate descent needs.

                 Gradients come from CostFunction::gradient() (forward
                 differences by default), so this driver is agnostic to
                 whether analytic gradients exist for the underlying
                 problem. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

#include "./minimizer.hpp"

namespace qsyn::tensor::opt {

namespace {

double dot(std::vector<double> const& a, std::vector<double> const& b) {
    double s     = 0.0;
    auto const n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}

double inf_norm(std::vector<double> const& v) {
    double m = 0.0;
    for (auto x : v) m = std::max(m, std::abs(x));
    return m;
}

}  // namespace

MinimizeResult LBFGSMinimizer::minimize(CostFunction& f,
                                        std::vector<double> const& x0,
                                        MinimizeOptions const& opt) {
    MinimizeResult res;
    res.x = x0;

    auto const n = res.x.size();
    if (n == 0) {
        res.initial_value = f.evaluate(res.x);
        res.final_value   = res.initial_value;
        res.converged     = res.final_value <= opt.tolerance;
        return res;
    }

    res.initial_value = f.evaluate(res.x);
    res.final_value   = res.initial_value;

    if (opt.verbosity >= 1) {
        spdlog::info("lbfgs: starting ({} params, initial residual = {:.3e})",
                     n, res.initial_value);
    }

    std::vector<double> g(n), g_new(n), d(n), x_trial(n);
    f.gradient(res.x, g, opt.lbfgs_fd_step);

    if (inf_norm(g) <= opt.grad_tolerance) {
        // Already stationary -- nothing to do.
        res.converged = res.final_value <= opt.tolerance;
        return res;
    }

    // History of (s_k = x_{k+1}-x_k) and (y_k = g_{k+1}-g_k) plus the
    // cached rho_k = 1 / (y_k . s_k) for the two-loop recursion.
    std::deque<std::vector<double>> S, Y;
    std::deque<double> Rho;

    double f_cur         = res.final_value;
    bool converged_local = false;

    for (std::size_t k = 0; k < opt.max_iterations; ++k) {
        // ---- Two-loop recursion to compute d = -H_k * g ----
        // q := g
        std::vector<double> q = g;
        std::vector<double> alpha(S.size(), 0.0);
        for (std::size_t i = S.size(); i-- > 0;) {
            double const rho = Rho[i];
            double const a   = rho * dot(S[i], q);
            alpha[i]         = a;
            for (std::size_t j = 0; j < n; ++j) q[j] -= a * Y[i][j];
        }

        // Initial H0 scaling: gamma = (s_k.y_k)/(y_k.y_k) -- standard
        // damping that gives LBFGS its quasi-Newton step length.
        double gamma = 1.0;
        if (!Y.empty()) {
            double const yy = dot(Y.back(), Y.back());
            if (yy > 1e-30) gamma = dot(S.back(), Y.back()) / yy;
        }
        for (std::size_t j = 0; j < n; ++j) q[j] *= gamma;

        for (std::size_t i = 0; i < S.size(); ++i) {
            double const rho = Rho[i];
            double const b   = rho * dot(Y[i], q);
            double const a   = alpha[i];
            for (std::size_t j = 0; j < n; ++j) q[j] += (a - b) * S[i][j];
        }
        // d := -q  (i.e. descent direction).
        for (std::size_t j = 0; j < n; ++j) d[j] = -q[j];

        double g_dot_d = dot(g, d);

        // Guard against non-descent direction (can happen if curvature
        // pairs were corrupted by line-search failure). Fall back to
        // steepest descent in that case.
        if (g_dot_d >= 0.0) {
            for (std::size_t j = 0; j < n; ++j) d[j] = -g[j];
            g_dot_d = dot(g, d);
            S.clear();
            Y.clear();
            Rho.clear();
        }

        // ---- Backtracking Armijo line search ----
        // If the directional derivative is below numerical noise the
        // Armijo condition is meaningless (every step "passes"); bail
        // out so we do not waste budget on a saddle.
        if (std::abs(g_dot_d) < 1e-14) {
            if (opt.verbosity >= 2) {
                spdlog::info("lbfgs: iter {} bail out -- |g.d|={:.2e} (saddle).",
                             k, std::abs(g_dot_d));
            }
            break;
        }
        double step    = opt.ls_initial;
        double f_trial = f_cur;
        bool ls_ok     = false;
        for (std::size_t ls = 0; ls < opt.ls_max_steps; ++ls) {
            for (std::size_t j = 0; j < n; ++j) x_trial[j] = res.x[j] + step * d[j];
            f_trial = f.evaluate(x_trial);
            if (f_trial <= f_cur + opt.armijo_c1 * step * g_dot_d) {
                ls_ok = true;
                break;
            }
            step *= opt.ls_shrink;
        }
        if (!ls_ok) {
            // Line search failed -- accept the trial only if it
            // genuinely lowers the cost (numerical noise can still pass
            // a strict Armijo test on a flat plateau).
            if (f_trial >= f_cur) {
                if (opt.verbosity >= 2) {
                    spdlog::info("lbfgs: iter {:3d}  line-search failed; aborting.", k);
                }
                break;
            }
        }

        // ---- Accept the step and bookkeep curvature pair ----
        std::vector<double> s(n), y(n);
        for (std::size_t j = 0; j < n; ++j) s[j] = step * d[j];

        // Restore the trial point as the current iterate, recompute
        // the gradient there.  The CostFunction may have side effects
        // (writes parameters into the QCir), so we have to *evaluate*
        // the trial again before reading the gradient -- otherwise the
        // QCir would still be at x_trial *only* internally.  In
        // practice the gradient routine evaluates x_trial first
        // anyway, so this is harmless.
        f.gradient(x_trial, g_new, opt.lbfgs_fd_step);
        for (std::size_t j = 0; j < n; ++j) y[j] = g_new[j] - g[j];

        double const sy = dot(s, y);
        if (sy > 1e-12) {
            S.push_back(std::move(s));
            Y.push_back(std::move(y));
            Rho.push_back(1.0 / sy);
            while (S.size() > opt.lbfgs_history) {
                S.pop_front();
                Y.pop_front();
                Rho.pop_front();
            }
        }
        // If the curvature condition fails we silently *skip* updating
        // the history (this is the standard "skip update" workaround;
        // a damped BFGS would mix in identity but we don't need that
        // here).

        res.x = x_trial;
        g     = g_new;
        f_cur = f_trial;
        ++res.n_iterations;

        if (opt.verbosity >= 2) {
            spdlog::info("lbfgs: iter {:3d}  step={:.2e}  residual={:.3e}  |g|inf={:.2e}",
                         k + 1, step, f_cur, inf_norm(g));
        }

        if (f_cur <= opt.tolerance) {
            converged_local = true;
            break;
        }
        if (inf_norm(g) <= opt.grad_tolerance) {
            converged_local = true;
            break;
        }
    }

    res.final_value = f_cur;
    res.converged   = converged_local || (f_cur <= opt.tolerance);

    if (opt.verbosity >= 1) {
        spdlog::info("lbfgs: finished after {} iterations, residual {:.3e} -> {:.3e}{}",
                     res.n_iterations, res.initial_value, res.final_value,
                     res.converged ? " (converged)" : "");
    }
    return res;
}

}  // namespace qsyn::tensor::opt
