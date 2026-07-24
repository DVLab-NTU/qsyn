/****************************************************************************
  PackageName  [ synthesis / search ]
  Synopsis     [ QSearch + LEAP main loops, sitting on top of the
                 frontier / layer-generator / heuristic / minimizer
                 abstractions. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qsearch.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <memory>

#include "./frontier.hpp"
#include "./heuristic.hpp"
#include "./layer_generator.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "tensor/opt/cost.hpp"

namespace qsyn::synthesis::search {

namespace {

std::size_t extract_n_qubits(QTensor<double> const& t) {
    if (t.shape().size() != 2) return 0;
    auto const dim = t.shape()[0];
    if (dim < 2) return 0;
    // dim must be a power of two.
    std::size_t n = 0;
    std::size_t d = dim;
    while (d > 1) {
        if ((d & 1u) != 0) return 0;
        d >>= 1u;
        ++n;
    }
    return n;
}

// Instantiate the free U3 parameters of `circ` against `target`.
// Returns the final residual; mutates `circ` in place.
double instantiate(qcir::QCir& circ, QTensor<double> const& target,
                   std::size_t budget, tensor::opt::MinimizerKind kind) {
    tensor::opt::QCirHilbertSchmidtCost cost{circ, target};
    if (cost.n_params() == 0) {
        return cost.evaluate({});
    }
    auto x0 = cost.snapshot_current();
    tensor::opt::MinimizeOptions mopt;
    mopt.max_iterations = budget;
    mopt.tolerance      = 1e-9;
    mopt.verbosity      = 0;
    mopt.lbfgs_fd_step  = 1e-4;
    auto       minimizer = tensor::opt::make_minimizer(kind);
    auto const m_res     = minimizer->minimize(cost, x0, mopt);
    cost.commit(m_res.x);
    return m_res.final_value;
}

// Count CX gates in a circuit. Used as a tie-breaker / summary metric.
std::size_t count_cx(qcir::QCir const& circ) {
    std::size_t n = 0;
    for (auto const* g : circ.get_gates()) {
        auto const t = g->get_operation().get_type();
        if (t.size() >= 2 && t[0] == 'c' && t[1] == 'p') {
            // ControlGate::get_type() = "c..." + underlying (e.g. "cpx" for CX).
            ++n;
        }
    }
    return n;
}

// Shared inner driver. `prefix` is appended-to during the search; on
// exit it contains the best ansatz found so far. Returns the best
// residual.
double run_qsearch(qcir::QCir& best_out,
                   QTensor<double> const& target,
                   QSearchOptions const& opt,
                   qcir::QCir const& seed) {
    auto const n_qubits = seed.get_num_qubits();
    SimpleLayerGenerator gen{n_qubits};
    AStarHeuristic       heuristic{opt.heuristic_alpha};

    qcir::QCir seed_copy = seed;
    auto const t_seed_start = std::chrono::steady_clock::now();
    double const seed_res = instantiate(seed_copy, target, opt.instantiate_iters, opt.minimizer);
    if (opt.verbosity >= 2) {
        auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t_seed_start)
                            .count();
        spdlog::info("qsearch: seed instantiate residual={:.3e} ({} ms, {} qubits)",
                     seed_res, ms, n_qubits);
    }

    Frontier frontier;
    {
        Candidate root;
        root.circuit  = std::move(seed_copy);
        root.residual = seed_res;
        root.depth    = 0;
        root.priority = heuristic.priority(root.residual, root.depth);
        frontier.push(std::move(root));
    }

    Candidate best;
    best.circuit  = seed;
    best.residual = seed_res;
    best.depth    = 0;

    std::size_t pops = 0;
    while (!frontier.empty() && pops < opt.max_iterations) {
        Candidate cur = frontier.pop();
        ++pops;
        if (opt.verbosity >= 2) {
            spdlog::info("qsearch: pop #{:3d}  depth={}  residual={:.3e}  priority={:.3e}",
                         pops, cur.depth, cur.residual, cur.priority);
        }

        if (cur.residual < best.residual) {
            best = cur;
        }
        if (best.residual <= opt.success_threshold) break;
        if (cur.depth >= opt.max_depth) continue;

        auto const t_expand_start = std::chrono::steady_clock::now();
        auto children = gen.expand(cur.circuit);
        for (auto& child : children) {
            double const r = instantiate(child, target, opt.instantiate_iters, opt.minimizer);
            Candidate next;
            next.circuit  = std::move(child);
            next.residual = r;
            next.depth    = cur.depth + 1;
            next.priority = heuristic.priority(next.residual, next.depth);
            frontier.push(std::move(next));

            if (next.residual < best.residual ||
                (next.residual == best.residual && next.depth < best.depth)) {
                // (we already moved `next` into the frontier; track via `r` instead)
            }
        }

        // Re-scan: pull the heap's top to update best (cheap because the
        // heap orders by priority not residual; we may miss the actual
        // residual-minimal candidate without this pass).
        Candidate const& top = frontier.peek();
        if (top.residual < best.residual) best = top;

        if (opt.verbosity >= 2) {
            auto const exp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - t_expand_start)
                                    .count();
            spdlog::info("qsearch: pop #{:3d} depth={} residual={:.3e} -> {} children in {} ms; "
                         "best={:.3e} frontier={}",
                         pops, cur.depth, cur.residual, children.size(), exp_ms,
                         best.residual, frontier.size());
        }
    }

    best_out = best.circuit;
    if (opt.verbosity >= 1) {
        spdlog::info("qsearch: finished after {} pops, best residual {:.3e}, depth {}, CX={}.",
                     pops, best.residual, best.depth, count_cx(best.circuit));
    }
    return best.residual;
}

}  // namespace

std::optional<qcir::QCir>
qsearch_synthesize(QTensor<double> const& target, QSearchOptions const& opt) {
    auto const n = extract_n_qubits(target);
    if (n == 0) {
        spdlog::error("qsearch_synthesize: target must be a 2^n x 2^n unitary; got shape "
                      "[{}, {}].",
                      target.shape().size() > 0 ? target.shape()[0] : 0,
                      target.shape().size() > 1 ? target.shape()[1] : 0);
        return std::nullopt;
    }

    qcir::QCir best;
    auto const seed = build_root_ansatz(n);
    double const r = run_qsearch(best, target, opt, seed);
    if (r > opt.success_threshold) {
        spdlog::warn("qsearch_synthesize: budget exhausted; final residual {:.3e} > eps {:.3e}.",
                     r, opt.success_threshold);
        return std::nullopt;
    }
    return best;
}

std::optional<qcir::QCir>
leap_synthesize(QTensor<double> const& target, LeapOptions const& opt) {
    auto const n = extract_n_qubits(target);
    if (n == 0) {
        spdlog::error("leap_synthesize: target must be a 2^n x 2^n unitary.");
        return std::nullopt;
    }

    qcir::QCir best = build_root_ansatz(n);
    double     best_res = 1.0;

    QSearchOptions inner = opt;
    inner.max_depth      = std::min(opt.max_depth, opt.prefix_freeze_period);

    for (std::size_t round = 0; round < opt.max_freeze_rounds; ++round) {
        qcir::QCir round_best;
        double const r = run_qsearch(round_best, target, inner, best);
        if (r < best_res - 1e-12) {
            best_res = r;
            best     = std::move(round_best);
        } else {
            if (opt.verbosity >= 1) {
                spdlog::info("leap: round {} did not improve (best residual={:.3e}); stopping.",
                             round, best_res);
            }
            break;
        }
        if (best_res <= opt.success_threshold) {
            if (opt.verbosity >= 1) {
                spdlog::info("leap: converged after {} rounds, residual {:.3e}.",
                             round + 1, best_res);
            }
            return best;
        }
    }

    if (best_res > opt.success_threshold) {
        spdlog::warn("leap_synthesize: budget exhausted; final residual {:.3e} > eps {:.3e}.",
                     best_res, opt.success_threshold);
        return std::nullopt;
    }
    return best;
}

}  // namespace qsyn::synthesis::search
