/****************************************************************************
  PackageName  [ tensor / kak ]
  Synopsis     [ BQSKit-style two-qubit KAK (Cartan) decomposition and
                 single-qubit ZYZ synthesis into a U3 + CNOT target gate set.

                 NOTE -- Connectivity / coupling-graph awareness is OUT OF
                 SCOPE for this module. This mirrors BQSKit upstream: the
                 KAK / Cartan algorithms themselves do not take a coupling
                 map; physical routing is a separate pass. qsyn users who
                 need physical mapping should chain this with
                   `device read ...`  +  `qcir optimize --physical`
                 or `qcir translate <gate_set>` after synthesis. The
                 connectivity-aware path is intentionally disabled until a
                 dedicated PR ports BQSKit's `setmodel.py` workflow. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <array>
#include <complex>
#include <optional>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::tensor::kak {

using qcir::QCir;

using c64 = std::complex<double>;

// Result of decomposing U in SU(4) as
//     U = global_phase * (A1 \otimes B1) * exp(i (a XX + b YY + c ZZ))
//                                        * (A2 \otimes B2)
struct KAKResult {
    std::array<c64, 4>  A1{};  // 2x2 row-major SU(2)
    std::array<c64, 4>  B1{};
    std::array<c64, 4>  A2{};
    std::array<c64, 4>  B2{};
    double              a{};   // Cartan invariant on XX
    double              b{};   // Cartan invariant on YY
    double              c{};   // Cartan invariant on ZZ
    c64                 global_phase{1, 0};
};

// Synthesise an arbitrary single-qubit unitary into a single U3 (ZYZ) gate.
// `matrix` must be a 2x2 QTensor.
[[nodiscard]] std::optional<QCir>
single_qubit_synthesize(QTensor<double> const& matrix);

// Synthesise the Cartan operator   W(a, b, c) := exp(i (a XX + b YY + c ZZ))
// into a 2-qubit circuit. The decomposition uses six CNOTs (two per Pauli
// rotation). For an optimal 3-CNOT result that holds for any (a, b, c),
// see `try_three_cnot_synthesize`.
[[nodiscard]] QCir cartan_synthesize(double a, double b, double c);

// 3-CNOT optimal ansatz: builds the canonical
//     U3 ⊗ U3 · CX(0,1) · U3 ⊗ U3 · CX(1,0) · U3 ⊗ U3 · CX(0,1) · U3 ⊗ U3
// (eight U3s, 24 free parameters) and instantiates the parameters with
// `qfactor::instantiate` so the resulting circuit matches `matrix`
// up to `epsilon` (Hilbert–Schmidt cosine residual).
//
// Returns `nullopt` when the numerical instantiation does not converge —
// in that case `two_qubit_synthesize` keeps the 6-CNOT cartan path.
//
// NOTE — current driver is QFactor-lite (coordinate descent) which is
// O(passes × params × tensor_cost) and may take seconds per 2-qubit
// block. Default `n_restarts = 1` keeps the worst case bounded; tune
// up after PR-B lands a real LBFGS minimizer.
[[nodiscard]] std::optional<QCir>
try_three_cnot_synthesize(QTensor<double> const& matrix,
                          double      epsilon        = 1e-6,
                          int         n_restarts     = 1,
                          std::size_t max_iterations = 150,
                          bool        use_lbfgs      = false);

struct TwoQubitSynthesizeOptions {
    // When true, also try the 3-CNOT QFactor-instantiated ansatz and
    // pick the lowest-CX result among the analytic + numerical
    // candidates.  Off by default until PR-B speeds up instantiation.
    bool   try_three_cnot_qfactor   = false;
    double three_cnot_epsilon       = 1e-6;
    int    three_cnot_restarts      = 1;
    std::size_t three_cnot_max_iter = 150;
    // When true, use the LBFGS minimiser instead of coordinate descent
    // when running the 3-CNOT QFactor instantiation. PR-B's LBFGS is
    // typically ~10x fewer tensor evaluations on the 24-param ansatz.
    bool   three_cnot_use_lbfgs     = false;
};

// Glue routine: KAK decompose `matrix`, then emit the full circuit using
// `cartan_synthesize` for the centre and `single_qubit_synthesize` for the
// four outer SU(2) factors.  Optional 3-CNOT QFactor candidate is added
// when `opt.try_three_cnot_qfactor == true`.
[[nodiscard]] std::optional<QCir>
two_qubit_synthesize(QTensor<double> const& matrix,
                     TwoQubitSynthesizeOptions const& opt);

// Decompose a 4x4 unitary `matrix` into KAK form. Returns `nullopt` if the
// numerical decomposition fails (e.g. ill-conditioned input).
[[nodiscard]] std::optional<KAKResult>
kak_decompose(QTensor<double> const& matrix);

// Default-options overload.
[[nodiscard]] std::optional<QCir>
two_qubit_synthesize(QTensor<double> const& matrix);

}  // namespace qsyn::tensor::kak
