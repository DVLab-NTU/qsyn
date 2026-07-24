/****************************************************************************
  PackageName  [ tensor / opt ]
  Synopsis     [ CostFunction implementations: forward-difference gradient
                 and a QCir-based Hilbert-Schmidt residual evaluator. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./cost.hpp"

#include <cmath>
#include <utility>

#include "convert/qcir_to_tensor.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir_gate.hpp"
#include "tensor/tensor.hpp"
#include "util/phase.hpp"

namespace qsyn::tensor::opt {

namespace {

dvlab::Phase to_phase(double rad) { return dvlab::Phase{rad, 1e-9}; }

std::array<double, 3> read_u(qcir::UGate const& u) {
    return {dvlab::Phase::phase_to_d(u.get_theta()),
            dvlab::Phase::phase_to_d(u.get_phi()),
            dvlab::Phase::phase_to_d(u.get_lambda())};
}

void write_u(qcir::QCirGate* g, std::uint8_t which, double new_val) {
    auto const u_opt = g->get_operation().get_underlying_if<qcir::UGate>();
    if (!u_opt.has_value()) return;
    auto cur   = read_u(*u_opt);
    cur[which] = new_val;
    g->set_operation(
        qcir::Operation{qcir::UGate(to_phase(cur[0]), to_phase(cur[1]), to_phase(cur[2]))});
}

}  // namespace

void CostFunction::gradient(std::vector<double> const& x,
                            std::vector<double>& out_grad,
                            double fd_step) {
    auto const n = n_params();
    out_grad.assign(n, 0.0);

    // Forward differences: 1 + n evaluations, robust but expensive.
    // The line search in LBFGS dominates anyway, so simplicity wins
    // until we plumb analytic envelope gradients.
    double const f0                 = evaluate(x);
    std::vector<double> x_perturbed = x;
    for (std::size_t i = 0; i < n; ++i) {
        double const saved = x_perturbed[i];
        x_perturbed[i]     = saved + fd_step;
        double const fi    = evaluate(x_perturbed);
        out_grad[i]        = (fi - f0) / fd_step;
        x_perturbed[i]     = saved;
    }
    // Restore in case caller reuses the same x.
    (void)evaluate(x);
}

// -------------------- QCirHilbertSchmidtCost --------------------

QCirHilbertSchmidtCost::QCirHilbertSchmidtCost(qcir::QCir& ansatz, QTensor<double> target)
    : _ansatz(ansatz), _target(std::move(target)) {
    for (auto* gate : _ansatz.get_gates()) {
        if (gate->get_operation().is<qcir::UGate>()) {
            _params.push_back({gate, 0});
            _params.push_back({gate, 1});
            _params.push_back({gate, 2});
        }
    }
}

std::vector<double> QCirHilbertSchmidtCost::snapshot_current() const {
    std::vector<double> x;
    x.reserve(_params.size());
    qcir::QCirGate const* prev = nullptr;
    std::array<double, 3> cache{};
    for (auto const& h : _params) {
        if (h.gate != prev) {
            auto const u_opt = h.gate->get_operation().get_underlying_if<qcir::UGate>();
            cache            = u_opt.has_value() ? read_u(*u_opt) : std::array<double, 3>{};
            prev             = h.gate;
        }
        x.push_back(cache[h.which]);
    }
    return x;
}

void QCirHilbertSchmidtCost::commit(std::vector<double> const& x) { apply(x); }

void QCirHilbertSchmidtCost::apply(std::vector<double> const& x) {
    // _params come in (gate, 0), (gate, 1), (gate, 2) triples, so we can
    // write all three components in one set_operation call instead of
    // three.  Saves two operation copies per UGate.
    for (std::size_t i = 0; i + 2 < _params.size(); i += 3) {
        auto* gate     = _params[i].gate;
        double const t = x[i + 0];
        double const p = x[i + 1];
        double const l = x[i + 2];
        gate->set_operation(
            qcir::Operation{qcir::UGate(to_phase(t), to_phase(p), to_phase(l))});
    }
    // Defensive: handle a malformed param list (n_params not a multiple of 3).
    for (std::size_t i = (_params.size() / 3) * 3; i < _params.size(); ++i) {
        write_u(_params[i].gate, _params[i].which, x[i]);
    }
}

double QCirHilbertSchmidtCost::evaluate(std::vector<double> const& x) {
    apply(x);
    auto tens_opt = qsyn::to_tensor(_ansatz);
    if (!tens_opt.has_value()) return 1.0;
    auto tens = tens_opt->to_matrix();
    if (tens.shape() != _target.shape()) return 1.0;
    double const sim = cosine_similarity(tens, _target);
    return 1.0 - sim;
}

}  // namespace qsyn::tensor::opt
