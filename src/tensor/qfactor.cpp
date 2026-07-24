/****************************************************************************
  PackageName  [ tensor / qfactor ]
  Synopsis     [ QFactor-lite instantiation. After PR-B this file is a thin
                 adapter: it builds a `QCirHilbertSchmidtCost` over the
                 caller's QCir, dispatches to a `Minimizer` chosen by
                 `QFactorOptions::strategy`, and writes the optimised
                 parameters back into the ansatz.

                 The numerical machinery (coord-descent / LBFGS) lives in
                 `tensor/opt/` so it can be reused by the search
                 algorithms in PR-C without going through QFactor. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024-2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qfactor.hpp"

#include <spdlog/spdlog.h>

#include <memory>

#include "tensor/opt/cost.hpp"
#include "tensor/opt/minimizer.hpp"

namespace qsyn::tensor::qfactor {

namespace {

opt::MinimizeOptions make_minimize_options(QFactorOptions const& q) {
    opt::MinimizeOptions o;
    o.max_iterations = q.max_iterations;
    o.tolerance      = q.tolerance;
    o.verbosity      = q.verbosity;

    o.cd_initial_step = q.initial_step;
    o.cd_step_shrink  = q.step_shrink;
    o.cd_min_step     = q.min_step;

    o.lbfgs_history = q.lbfgs_history;
    o.lbfgs_fd_step = q.lbfgs_fd_step;
    return o;
}

}  // namespace

QFactorResult instantiate(qcir::QCir& ansatz,
                          QTensor<double> const& target,
                          QFactorOptions const& opt) {
    QFactorResult res;

    opt::QCirHilbertSchmidtCost cost{ansatz, target};
    res.n_parameters = cost.n_params();

    auto const x0 = cost.snapshot_current();
    if (res.n_parameters == 0) {
        res.initial_residual = cost.evaluate(x0);
        res.final_residual   = res.initial_residual;
        res.converged        = res.initial_residual <= opt.tolerance;
        if (opt.verbosity >= 1) {
            spdlog::info("qfactor: no UGate parameters to tune; residual = {:.3e}",
                         res.initial_residual);
        }
        return res;
    }

    auto const m_opt = make_minimize_options(opt);
    auto const kind  = (opt.strategy == QFactorStrategy::LBFGS)
                           ? opt::MinimizerKind::LBFGS
                           : opt::MinimizerKind::CoordinateDescent;
    auto minimizer   = opt::make_minimizer(kind);

    auto m_res = minimizer->minimize(cost, x0, m_opt);

    // Commit the best parameter vector. This call double-writes after the
    // last `evaluate` inside the minimiser (which already left `cost`'s
    // internal ansatz at m_res.x), but is essential when the minimiser
    // aborts early on a worse point.
    cost.commit(m_res.x);

    res.initial_residual = m_res.initial_value;
    res.final_residual   = m_res.final_value;
    res.n_passes         = m_res.n_iterations;
    res.converged        = m_res.converged;
    return res;
}

}  // namespace qsyn::tensor::qfactor
