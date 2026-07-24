/****************************************************************************
  PackageName  [ tensor / opt ]
  Synopsis     [ Generic cost-function interface for numerical optimisation
                 of QCir parameter vectors.

                 This is the foundation of PR-B (LBFGS-backed QFactor):
                 a `CostFunction` exposes a smooth-ish scalar objective
                 plus an optional analytic gradient. The default gradient
                 implementation uses forward finite differences, which is
                 always available for any cost function that can be
                 evaluated. Concrete subclasses can override `gradient()`
                 to provide a cheaper analytic implementation when one
                 exists (e.g. the BQSKit / QFactor environment trick).

                 The matching `Minimizer` (see minimizer.hpp) consumes any
                 CostFunction without knowing which one it is, so the
                 same LBFGS driver can polish KAK ansatzes, search
                 frontier candidates (PR-C), or arbitrary user ansatzes. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::tensor::opt {

// Minimal scalar cost function in n-dim real space.
class CostFunction {
public:
    virtual ~CostFunction()                                          = default;
    [[nodiscard]] virtual std::size_t n_params() const               = 0;
    [[nodiscard]] virtual double evaluate(std::vector<double> const& x) = 0;

    // Forward finite-difference gradient. Subclasses can override with
    // an analytic version. Writes into `out_grad` (resized to n_params).
    virtual void gradient(std::vector<double> const& x,
                          std::vector<double>&       out_grad,
                          double                     fd_step = 1e-4);
};

// Cost function for "make this QCir reproduce that unitary":
//   f(x) = 1 - cosine_similarity( to_tensor(qc[x]), target )
// where x is the flat list of (theta, phi, lambda) angles of every UGate
// inside `ansatz`. The constructor caches the parameter handles and the
// target tensor; `evaluate(x)` writes the parameters back into the QCir,
// runs `to_tensor`, and returns the residual.
//
// NOTE: This class mutates the QCir it was constructed with.  After the
// optimisation it is the caller's responsibility to extract / freeze the
// final parameters (typically: just keep the QCir, which now contains the
// best-found values).
class QCirHilbertSchmidtCost : public CostFunction {
public:
    QCirHilbertSchmidtCost(qcir::QCir& ansatz, QTensor<double> target);

    [[nodiscard]] std::size_t n_params() const override { return _params.size(); }
    [[nodiscard]] double evaluate(std::vector<double> const& x) override;

    // Snapshot the current UGate angles into a flat vector.  Useful as
    // a starting point x0 for the minimiser.
    [[nodiscard]] std::vector<double> snapshot_current() const;

    // Write a parameter vector into the underlying QCir without
    // running to_tensor.  Caller uses this to commit the final x*.
    void commit(std::vector<double> const& x);

private:
    struct Handle {
        qcir::QCirGate* gate;
        std::uint8_t    which;  // 0 = theta, 1 = phi, 2 = lambda
    };

    qcir::QCir&         _ansatz;
    QTensor<double>     _target;
    std::vector<Handle> _params;

    void apply(std::vector<double> const& x);
};

}  // namespace qsyn::tensor::opt
