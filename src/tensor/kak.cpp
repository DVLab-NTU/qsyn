/****************************************************************************
  PackageName  [ tensor / kak ]
  Synopsis     [ Implementation of KAK and friends. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./kak.hpp"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>
#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/containers/xarray.hpp>
#include <xtensor/views/xview.hpp>

#include "qcir/basic_gate_type.hpp"
#include "tensor/decomposer.hpp"
#include "tensor/qfactor.hpp"
#include "util/phase.hpp"

namespace qsyn::tensor::kak {

namespace {

using xt::xtensor;
using std::numbers::pi;

using cmat44 = xt::xtensor<c64, 2>;
using cmat22 = xt::xtensor<c64, 2>;
using rmat44 = xt::xtensor<double, 2>;

// Magic basis (Qiskit / Vatan-Williams convention). M U(SU(2)*SU(2)) M^dagger
// lives in SO(4), so KAK is essentially an eigendecomposition in this basis.
//
//        1   [ 1   0   0   i ]
// M  =  ---  [ 0   i   1   0 ]
//       sqrt2 [ 0   i  -1   0 ]
//             [ 1   0   0  -i ]
cmat44 const& magic_basis() {
    static cmat44 const M = []() {
        constexpr double s = 1.0 / std::numbers::sqrt2;
        c64 const I{0, 1};
        return cmat44{
            {s,      0.,     0.,      I * s},
            {0.,     I * s,  s,       0.   },
            {0.,     I * s, -c64(s),  0.   },
            {s,      0.,     0.,     -I * s}};
    }();
    return M;
}

cmat44 const& magic_basis_dagger() {
    static cmat44 const Mdag = xt::conj(xt::transpose(magic_basis()));
    return Mdag;
}

// Convert a qsyn QTensor's flat 2D view into an xt complex matrix. The
// indexing matches the standard math convention M(out_row, in_col); empty
// test (rygate) confirms `t(0, 1)` equals `-sin(theta/2)` (i.e. matrix
// element Ry[0,1]), so no transpose is required here.
cmat44 to_cmat44(QTensor<double> const& mat) {
    cmat44 out = xt::zeros<c64>({4, 4});
    for (size_t r = 0; r < 4; ++r)
        for (size_t c = 0; c < 4; ++c)
            out(r, c) = mat(r, c);
    return out;
}

cmat22 to_cmat22(QTensor<double> const& mat) {
    cmat22 out = xt::zeros<c64>({2, 2});
    for (size_t r = 0; r < 2; ++r)
        for (size_t c = 0; c < 2; ++c)
            out(r, c) = mat(r, c);
    return out;
}

std::array<c64, 4> flatten_cmat22(cmat22 const& m) {
    return {m(0, 0), m(0, 1), m(1, 0), m(1, 1)};
}

auto kron22(cmat22 const& A, cmat22 const& B) {
    cmat44 R = xt::zeros<c64>({4, 4});
    for (size_t r = 0; r < 2; ++r)
        for (size_t c = 0; c < 2; ++c)
            for (size_t br = 0; br < 2; ++br)
                for (size_t bc = 0; bc < 2; ++bc)
                    R(2 * r + br, 2 * c + bc) = A(r, c) * B(br, bc);
    return R;
}

// Tensor-product factorisation: given K ≈ A \otimes B (SU(2) factors), recover
// A and B.  Try every matrix entry as gauge anchor and keep the best residual.
std::optional<std::pair<cmat22, cmat22>>
factor_tensor_product(cmat44 const& K) {
    std::optional<std::pair<cmat22, cmat22>> best;
    double best_err = 1e30;

    auto try_anchor = [&](size_t bi, size_t bj) {
        auto const ai  = bi / 2;
        auto const aj  = bj / 2;
        auto const bri = bi % 2;
        auto const bcj = bj % 2;

        cmat22 B = xt::zeros<c64>({2, 2});
        for (size_t r = 0; r < 2; ++r)
            for (size_t c = 0; c < 2; ++c)
                B(r, c) = K(2 * ai + r, 2 * aj + c);
        auto const anchor = B(bri, bcj);
        if (std::abs(anchor) < 1e-12) return;
        B /= anchor;

        cmat22 A = xt::zeros<c64>({2, 2});
        for (size_t r = 0; r < 2; ++r)
            for (size_t c = 0; c < 2; ++c)
                A(r, c) = K(2 * r + bri, 2 * c + bcj);

        auto const detA = A(0, 0) * A(1, 1) - A(0, 1) * A(1, 0);
        auto const detB = B(0, 0) * B(1, 1) - B(0, 1) * B(1, 0);
        if (std::abs(detA) < 1e-12 || std::abs(detB) < 1e-12) return;
        A /= std::sqrt(detA);
        B /= std::sqrt(detB);

        auto const R  = kron22(A, B);
        double err    = 0;
        for (size_t i = 0; i < 4; ++i)
            for (size_t j = 0; j < 4; ++j)
                err += std::abs(K(i, j) - R(i, j));
        if (err < best_err) {
            best_err = err;
            best     = std::make_pair(A, B);
        }
    };

    for (size_t bi = 0; bi < 4; ++bi)
        for (size_t bj = 0; bj < 4; ++bj) try_anchor(bi, bj);

    // Also try K = B \otimes A (swap tensor factor order).
    for (size_t bi = 0; bi < 4; ++bi) {
        for (size_t bj = 0; bj < 4; ++bj) {
            auto const ai  = bi / 2;
            auto const aj  = bj / 2;
            auto const bri = bi % 2;
            auto const bcj = bj % 2;
            cmat22 A = xt::zeros<c64>({2, 2});
            for (size_t r = 0; r < 2; ++r)
                for (size_t c = 0; c < 2; ++c)
                    A(r, c) = K(2 * r + bri, 2 * c + bcj);
            auto const anchor = A(bri, bcj);
            if (std::abs(anchor) < 1e-12) continue;
            A /= anchor;
            cmat22 B = xt::zeros<c64>({2, 2});
            for (size_t r = 0; r < 2; ++r)
                for (size_t c = 0; c < 2; ++c)
                    B(r, c) = K(2 * ai + r, 2 * aj + c);
            auto const detA = A(0, 0) * A(1, 1) - A(0, 1) * A(1, 0);
            auto const detB = B(0, 0) * B(1, 1) - B(0, 1) * B(1, 0);
            if (std::abs(detA) < 1e-12 || std::abs(detB) < 1e-12) continue;
            A /= std::sqrt(detA);
            B /= std::sqrt(detB);
            auto const R  = kron22(A, B);
            double err    = 0;
            for (size_t i = 0; i < 4; ++i)
                for (size_t j = 0; j < 4; ++j)
                    err += std::abs(K(i, j) - R(i, j));
            if (err < best_err) {
                best_err = err;
                best     = std::make_pair(A, B);
            }
        }
    }

    if (!best.has_value() || best_err > 1e-2) return std::nullopt;
    return best;
}

// 3-CNOT structural ansatz that can represent any SU(4): eight U3 gates
// interleaved with three CNOTs (alternating control direction). This
// over-parameterises SU(4) (24 params vs. 16 DoF) which makes the
// instantiation landscape much friendlier for coordinate descent.
[[nodiscard]] QCir build_three_cnot_ansatz() {
    QCir qc{2};
    auto const zero_u = qcir::UGate(dvlab::Phase(0), dvlab::Phase(0), dvlab::Phase(0));
    qc.append(zero_u, {0});
    qc.append(zero_u, {1});
    qc.append(qcir::CXGate(), {0, 1});
    qc.append(zero_u, {0});
    qc.append(zero_u, {1});
    qc.append(qcir::CXGate(), {1, 0});
    qc.append(zero_u, {0});
    qc.append(zero_u, {1});
    qc.append(qcir::CXGate(), {0, 1});
    qc.append(zero_u, {0});
    qc.append(zero_u, {1});
    return qc;
}

// Randomly perturb every UGate's three parameters with a uniform offset
// in [-amp, amp].  Deterministic given a fixed `seed`.
void randomise_u_params(QCir& qc, std::uint32_t seed, double amp) {
    std::minstd_rand                       rng{seed};
    std::uniform_real_distribution<double> dist{-amp, amp};
    for (auto* gate : qc.get_gates()) {
        auto const u_opt = gate->get_operation().get_underlying_if<qcir::UGate>();
        if (!u_opt.has_value()) continue;
        auto const theta  = dvlab::Phase::phase_to_d(u_opt->get_theta()) + dist(rng);
        auto const phi    = dvlab::Phase::phase_to_d(u_opt->get_phi()) + dist(rng);
        auto const lambda = dvlab::Phase::phase_to_d(u_opt->get_lambda()) + dist(rng);
        gate->set_operation(qcir::Operation{qcir::UGate(
            dvlab::Phase(theta, 1e-9), dvlab::Phase(phi, 1e-9), dvlab::Phase(lambda, 1e-9))});
    }
}

[[nodiscard]] size_t count_cx_gates_anon(QCir const& circ) {
    size_t n = 0;
    for (auto const* g : circ.get_gates()) {
        auto const t = g->get_operation().get_type();
        if (t.size() >= 2 && t[0] == 'c' && t[1] == 'x') ++n;
    }
    return n;
}

}  // namespace

std::optional<QCir>
try_three_cnot_synthesize(QTensor<double> const& matrix, double epsilon,
                          int n_restarts, std::size_t max_iterations,
                          bool use_lbfgs) {
    if (matrix.shape().size() != 2 || matrix.shape()[0] != 4 || matrix.shape()[1] != 4) {
        return std::nullopt;
    }
    if (n_restarts < 1) n_restarts = 1;

    qfactor::QFactorOptions qopt;
    qopt.tolerance      = epsilon;
    qopt.max_iterations = max_iterations;
    qopt.initial_step   = 0.6;
    qopt.step_shrink    = 0.5;
    qopt.min_step       = 1e-10;
    qopt.verbosity      = 0;
    qopt.strategy       = use_lbfgs ? qfactor::QFactorStrategy::LBFGS
                                    : qfactor::QFactorStrategy::CoordinateDescent;
    // LBFGS converges in fewer outer iters; cap at a sensible LBFGS budget.
    if (use_lbfgs) {
        qopt.max_iterations = std::min<std::size_t>(max_iterations, 80);
    }

    // Restart with several random seeds; coordinate descent on the
    // 24-parameter ansatz is non-convex so a single shot occasionally
    // gets trapped at a local minimum. Single restart is the cheap
    // default; callers can request more via TwoQubitSynthesizeOptions.
    QCir                            best;
    double                          best_residual = std::numeric_limits<double>::infinity();
    static constexpr std::array<std::uint32_t, 4> seeds{12345, 67890, 246810, 9876543};

    for (int trial = 0; trial < n_restarts; ++trial) {
        QCir ansatz = build_three_cnot_ansatz();
        if (trial != 0) {
            auto const seed = seeds[std::min<std::size_t>(trial, seeds.size() - 1)];
            randomise_u_params(ansatz, seed, std::numbers::pi);
        }
        auto const res = qfactor::instantiate(ansatz, matrix, qopt);
        if (res.final_residual < best_residual) {
            best_residual = res.final_residual;
            best          = ansatz;
            if (res.converged) break;
        }
    }

    if (best_residual > epsilon) {
        spdlog::debug("try_three_cnot_synthesize: residual {:.2e} > eps {:.2e}; rejecting.",
                      best_residual, epsilon);
        return std::nullopt;
    }
    return best;
}

std::optional<QCir>
single_qubit_synthesize(QTensor<double> const& matrix) {
    if (matrix.shape().size() != 2 || matrix.shape()[0] != 2 || matrix.shape()[1] != 2) {
        spdlog::error("single_qubit_synthesize expects a 2x2 matrix.");
        return std::nullopt;
    }

    // Inline ZYZ extraction (mirrors `tensor::Decomposer::_decompose_zyz`
    // but reproduced here so we don't depend on that class' private
    // friendship surface).
    //     M = e^{i phi} Rz(alpha) Ry(beta) Rz(gamma)
    // and we map onto qsyn's `UGate(theta, phi, lambda) =
    //     Rz(phi) Ry(theta) Rz(lambda)` form (see `to_tensor(UGate)`).
    auto const a = matrix(0, 0);
    auto const b = matrix(0, 1);
    auto const c = matrix(1, 0);
    auto const d = matrix(1, 1);

    double const init_beta = (std::abs(a) > 1.0) ? 0.0 : std::acos(std::abs(a));
    std::array<double, 4> const beta_candidates = {init_beta, pi - init_beta,
                                                    pi + init_beta, 2 * pi - init_beta};

    for (auto const beta : beta_candidates) {
        c64 const cos_b{std::cos(beta) + 1e-9, 0};
        c64 const sin_b{std::sin(beta) + 1e-9, 0};
        auto const a1 = a / cos_b;
        auto const b1 = b / sin_b;
        auto const c1 = c / sin_b;
        auto const d1 = d / cos_b;

        double alpha = 0, gamma = 0, phi = 0;
        if (std::abs(b) < 1e-4) {
            alpha = std::arg(d1 / a1) / 2.0;
            gamma = alpha;
        } else if (std::abs(a) < 1e-4) {
            alpha = std::arg(-c1 / b1) / 2.0;
            gamma = -alpha;
        } else {
            alpha = std::arg(c1 / a1);
            gamma = std::arg(d1 / c1);
        }

        c64 const alpha_plus = std::exp(c64{0, 0.5 * (alpha + gamma)});
        c64 const alpha_minus = std::exp(c64{0, 0.5 * (alpha - gamma)});

        phi = (std::abs(a) < 1e-4) ? std::arg(c1 / alpha_minus) : std::arg(a1 * alpha_plus);

        // Reconstruct and check residual.
        c64 const e_iphi = std::exp(c64{0, phi});
        c64 const r00 = e_iphi * std::exp(c64{0, -0.5 * (alpha + gamma)}) * c64{std::cos(beta), 0};
        c64 const r01 = -e_iphi * std::exp(c64{0, -0.5 * (alpha - gamma)}) * c64{std::sin(beta), 0};

        if (std::abs(r00 - a) < 1e-5 && std::abs(r01 - b) < 1e-5) {
            // Build U(theta, phi_zyz, lambda_zyz) where theta = 2*beta
            // (because `beta` here is the half-angle), phi_zyz = alpha,
            // lambda_zyz = gamma. The Phase(double) constructor takes
            // radians and stores `radians / pi` internally, so we pass
            // raw radian values here.
            auto const theta_phase  = dvlab::Phase(2.0 * beta);
            auto const phi_phase    = dvlab::Phase(alpha);
            auto const lambda_phase = dvlab::Phase(gamma);

            QCir qc{1};
            qc.append(qcir::UGate(theta_phase, phi_phase, lambda_phase), {0});
            return qc;
        }
    }

    spdlog::error("single_qubit_synthesize: ZYZ decomposition failed to converge.");
    return std::nullopt;
}

// exp(i theta P_a P_b) where P_a, P_b in {X, Y, Z}: the standard CNOT
// ladder. We always rotate around Z on qubit 1 and conjugate the input
// basis by single-qubit gates.
//
// For the diagonal exp(i theta ZZ):   CX(0,1) Rz_1(-2 theta) CX(0,1)
// For exp(i theta XX):                (H \otimes H) [exp(i theta ZZ)] (H \otimes H)
// For exp(i theta YY):                (V \otimes V) [exp(i theta ZZ)] (V^dagger \otimes V^dagger)
//                                     where V = R_X(pi/2) = sqrt(X) maps Y -> Z.
//                                     S * H also works: H Sdg Y S H = Z.
QCir cartan_synthesize(double a, double b, double c) {
    QCir qc{2};

    auto const append_zz_rot = [&](double theta) {
        qc.append(qcir::CXGate(), {0, 1});
        // exp(i theta ZZ) on qubit pair {0, 1} via CX-Rz(-2 theta)-CX.
        // Phase(double) takes radians, so pass `-2 * theta` directly.
        qc.append(qcir::RZGate(dvlab::Phase(-2.0 * theta)), {1});
        qc.append(qcir::CXGate(), {0, 1});
    };

    // exp(i a XX): basis change H on both qubits.
    if (std::abs(a) > 1e-12) {
        qc.append(qcir::HGate(), {0});
        qc.append(qcir::HGate(), {1});
        append_zz_rot(a);
        qc.append(qcir::HGate(), {0});
        qc.append(qcir::HGate(), {1});
    }

    // exp(i b YY): basis change V = sqrt(X) on both qubits (= Rx(pi/2)).
    // We synthesise V via S * H * Sdg (any single-qubit way that maps Y -> Z works).
    // Concretely use the identity Y = Sdg X S, then transform X -> Z via H:
    //     Y = Sdg H Z H S, so exp(i b YY) = (Sdg H \otimes Sdg H) exp(i b ZZ) (H S \otimes H S).
    if (std::abs(b) > 1e-12) {
        qc.append(qcir::PZGate(dvlab::Phase(1, 2)), {0});  // S
        qc.append(qcir::HGate(), {0});
        qc.append(qcir::PZGate(dvlab::Phase(1, 2)), {1});  // S
        qc.append(qcir::HGate(), {1});
        append_zz_rot(b);
        qc.append(qcir::HGate(), {0});
        qc.append(qcir::PZGate(dvlab::Phase(-1, 2)), {0});  // Sdg
        qc.append(qcir::HGate(), {1});
        qc.append(qcir::PZGate(dvlab::Phase(-1, 2)), {1});  // Sdg
    }

    // exp(i c ZZ): trivial CNOT-Rz-CNOT.
    if (std::abs(c) > 1e-12) {
        append_zz_rot(c);
    }

    return qc;
}

std::optional<KAKResult>
kak_decompose(QTensor<double> const& matrix) {
    if (matrix.shape().size() != 2 || matrix.shape()[0] != 4 || matrix.shape()[1] != 4) {
        spdlog::error("kak_decompose expects a 4x4 matrix.");
        return std::nullopt;
    }

    auto const U  = to_cmat44(matrix);
    auto const&  M  = magic_basis();
    auto const&  Md = magic_basis_dagger();

    // Strip global phase -> SU(4).
    auto const detU = xt::linalg::det(U);
    auto const phase = std::pow(detU, 0.25);
    cmat44 const Us = U / phase;

    // Transform into magic basis.
    cmat44 const Um = xt::linalg::dot(xt::linalg::dot(Md, Us), M);

    // M2 = Um @ Um^T. Symmetric (M2^T = M2), unitary.
    cmat44 const M2 = xt::linalg::dot(Um, xt::transpose(Um));

    // Real part is real symmetric; eigenvectors form a real orthogonal O
    // that *also* (because Im(M2) is a polynomial of Re(M2) when both are
    // simultaneously diagonalisable via the same O - true for unitary
    // symmetric M2) diagonalises Im(M2).  We jitter the combination to
    // avoid eigenvalue degeneracies.
    rmat44 M2_real = xt::real(M2);
    rmat44 M2_imag = xt::imag(M2);
    rmat44 const combo = 0.6 * M2_real + 0.4 * M2_imag;

    auto eig_pair = xt::linalg::eigh(combo);
    rmat44 const O = std::get<1>(eig_pair);  // 4x4 real orthogonal

    // Diagonalise M2 -- the diagonal entries give us e^{2i theta_k}.
    cmat44 const D = xt::linalg::dot(xt::linalg::dot(xt::transpose(O), M2), O);

    std::array<double, 4> thetas{};
    for (size_t k = 0; k < 4; ++k) {
        thetas[k] = std::arg(D(k, k)) * 0.5;
    }

    // Force det(O) = +1 so that O^T is in SO(4) and maps to SU(2) x SU(2).
    auto detO = xt::linalg::det(O);
    cmat44 O_signed = O;
    if (detO < 0) {
        // flip the first column to make det positive; absorb the sign into
        // theta_0 by adding pi.
        for (size_t r = 0; r < 4; ++r) O_signed(r, 0) = -O_signed(r, 0);
        thetas[0] += pi;
    }

    // K2 in magic basis is O^T; K1 in magic basis is Um @ O @ diag(e^{-i theta_k}).
    cmat44 Dinv = xt::zeros<c64>({4, 4});
    for (size_t k = 0; k < 4; ++k) Dinv(k, k) = std::exp(c64{0, -thetas[k]});
    cmat44 const K1_mag = xt::linalg::dot(xt::linalg::dot(Um, O_signed), Dinv);
    cmat44 const K2_mag = xt::transpose(O_signed);

    // Map back to standard basis: K = M @ K_mag @ M^dagger. The result is
    // (up to small numerical error) of the tensor-product form A \otimes B.
    cmat44 const K1 = xt::linalg::dot(xt::linalg::dot(M, K1_mag), Md);
    cmat44 const K2 = xt::linalg::dot(xt::linalg::dot(M, K2_mag), Md);

    auto K1_factored = factor_tensor_product(K1);
    auto K2_factored = factor_tensor_product(K2);
    if (!K1_factored.has_value() || !K2_factored.has_value()) {
        spdlog::debug("kak_decompose: failed to factor K1 / K2 into SU(2) x SU(2).");
        return std::nullopt;
    }

    // Cartan parameters from thetas: the basis change M . exp(i diag(theta_k))
    // . M^dagger equals exp(i (a XX + b YY + c ZZ)) where
    //   t0 =  a - b + c
    //   t1 = -a + b + c          (signs depend on M; this convention matches
    //   t2 = -a - b - c           Vatan-Williams 2004, eq. 5.)
    //   t3 =  a + b - c
    // Solving the 3x3 sub-system (t0..t2):
    KAKResult res;
    res.a = ( thetas[0] - thetas[1] - thetas[2] + thetas[3]) * 0.25;
    res.b = (-thetas[0] + thetas[1] - thetas[2] + thetas[3]) * 0.25;
    res.c = ( thetas[0] + thetas[1] - thetas[2] - thetas[3]) * 0.25;

    res.A1 = flatten_cmat22(K1_factored->first);
    res.B1 = flatten_cmat22(K1_factored->second);
    res.A2 = flatten_cmat22(K2_factored->first);
    res.B2 = flatten_cmat22(K2_factored->second);
    res.global_phase = phase;

    return res;
}

// Standalone wrapper that re-exports the in-namespace helper at TU scope.
[[nodiscard]] static size_t count_cx_gates(QCir const& circ) {
    return count_cx_gates_anon(circ);
}

std::optional<QCir> two_qubit_synthesize(QTensor<double> const& matrix) {
    return two_qubit_synthesize(matrix, TwoQubitSynthesizeOptions{});
}

std::optional<QCir>
two_qubit_synthesize(QTensor<double> const& matrix,
                     TwoQubitSynthesizeOptions const& opt) {
    std::vector<QCir> candidates;

    // ------------------------------------------------------------------
    // Candidate 1 + 2: analytic KAK paths (6-CNOT cartan + optional 2-CNOT).
    // Both depend on `kak_decompose` succeeding; if it does not we still
    // try the QFactor 3-CNOT ansatz and the gray-code fallback below.
    // ------------------------------------------------------------------
    if (auto kak = kak_decompose(matrix); kak.has_value()) {
        QCir result{2};

        auto append_single = [&](std::array<c64, 4> const& m, size_t q) -> bool {
            QTensor<double> tens{{m[0], m[1]}, {m[2], m[3]}};
            auto sub = single_qubit_synthesize(tens);
            if (!sub.has_value()) return false;
            for (auto const& g : sub->get_gates()) {
                result.append(g->get_operation(), {static_cast<QubitIdType>(q)});
            }
            return true;
        };

        bool kak_ok = true;
        kak_ok &= append_single(kak->A2, 0);
        kak_ok &= append_single(kak->B2, 1);

        auto centre = cartan_synthesize(kak->a, kak->b, kak->c);
        for (auto const& g : centre.get_gates()) {
            result.append(g->get_operation(), g->get_qubits());
        }

        kak_ok &= append_single(kak->A1, 0);
        kak_ok &= append_single(kak->B1, 1);

        if (kak_ok) candidates.push_back(result);

        // 2-CNOT template applies only when c ≈ 0.
        auto cartan_2cnot = [&]() -> std::optional<QCir> {
            if (std::abs(kak->c) > 1e-8) return std::nullopt;
            QCir qc{2};
            auto append_zz = [&](double theta) {
                qc.append(qcir::CXGate(), {0, 1});
                qc.append(qcir::RZGate(dvlab::Phase(-2.0 * theta)), {1});
                qc.append(qcir::CXGate(), {0, 1});
            };
            if (std::abs(kak->a) > 1e-12) {
                qc.append(qcir::HGate(), {0});
                qc.append(qcir::HGate(), {1});
                append_zz(kak->a);
                qc.append(qcir::HGate(), {0});
                qc.append(qcir::HGate(), {1});
            }
            if (std::abs(kak->b) > 1e-12) {
                qc.append(qcir::PZGate(dvlab::Phase(1, 2)), {0});
                qc.append(qcir::HGate(), {0});
                qc.append(qcir::PZGate(dvlab::Phase(1, 2)), {1});
                qc.append(qcir::HGate(), {1});
                append_zz(kak->b);
                qc.append(qcir::HGate(), {0});
                qc.append(qcir::PZGate(dvlab::Phase(-1, 2)), {0});
                qc.append(qcir::HGate(), {1});
                qc.append(qcir::PZGate(dvlab::Phase(-1, 2)), {1});
            }
            return qc;
        };

        if (auto c2 = cartan_2cnot(); c2.has_value()) {
            QCir wrapped{2};
            bool ok = true;
            ok &= append_single(kak->A2, 0);
            ok &= append_single(kak->B2, 1);
            for (auto const* g : c2->get_gates()) wrapped.append(g->get_operation(), g->get_qubits());
            ok &= append_single(kak->A1, 0);
            ok &= append_single(kak->B1, 1);
            if (ok) candidates.push_back(std::move(wrapped));
        }
    }

    // ------------------------------------------------------------------
    // Candidate 3 (opt-in): 3-CNOT QFactor-instantiated ansatz.
    // Off by default because coordinate-descent instantiation is slow;
    // enable via `TwoQubitSynthesizeOptions::try_three_cnot_qfactor`.
    // ------------------------------------------------------------------
    if (opt.try_three_cnot_qfactor) {
        if (auto three = try_three_cnot_synthesize(matrix, opt.three_cnot_epsilon,
                                                   opt.three_cnot_restarts,
                                                   opt.three_cnot_max_iter,
                                                   opt.three_cnot_use_lbfgs);
            three.has_value()) {
            candidates.push_back(std::move(*three));
        }
    }

    // ------------------------------------------------------------------
    // Candidate 4: gray-code fallback (correctness baseline).
    // ------------------------------------------------------------------
    Decomposer dec;
    if (auto gray = dec.decompose(matrix); gray.has_value()) {
        candidates.push_back(std::move(*gray));
    }

    if (candidates.empty()) {
        spdlog::error("two_qubit_synthesize: every candidate path failed.");
        return std::nullopt;
    }

    // Pick lowest CX count; on a tie keep the earlier (analytic) entry
    // for reproducibility.
    size_t best_cx = SIZE_MAX;
    size_t best_i  = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        auto const n = count_cx_gates(candidates[i]);
        if (n < best_cx) {
            best_cx = n;
            best_i  = i;
        }
    }
    spdlog::debug("two_qubit_synthesize: picked candidate {} with {} CX (out of {}).",
                  best_i, best_cx, candidates.size());
    return std::move(candidates[best_i]);
}

}  // namespace qsyn::tensor::kak
