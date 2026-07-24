/****************************************************************************
  PackageName  [ tensor / qsd ]
  Synopsis     [ Quantum Shannon Decomposition implementation. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qsd.hpp"

#include <spdlog/spdlog.h>

#include <cmath>
#include <numbers>

#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/containers/xarray.hpp>
#include <xtensor/views/xview.hpp>

#include "qcir/basic_gate_type.hpp"
#include "qcir/scanning_gate_removal.hpp"
#include "tensor/csd.hpp"
#include "tensor/decomposer.hpp"
#include "tensor/kak.hpp"
#include "tensor/tensor_util.hpp"

namespace qsyn::tensor::qsd {

namespace {

using c64  = std::complex<double>;
using cmat = xt::xtensor<c64, 2>;

[[nodiscard]] std::optional<size_t>
qubit_count_from_shape(QTensor<double> const& mat) {
    if (mat.shape().size() != 2) return std::nullopt;
    auto const dim = mat.shape()[0];
    if (dim != mat.shape()[1]) return std::nullopt;
    if (dim == 0 || (dim & (dim - 1)) != 0) return std::nullopt;
    size_t n = 0;
    for (auto d = dim; d > 1; d >>= 1) ++n;
    return n;
}

cmat qtensor_to_cmat(QTensor<double> const& mat) {
    auto const dim = mat.shape()[0];
    cmat out = xt::zeros<c64>({dim, dim});
    for (size_t r = 0; r < dim; ++r)
        for (size_t c = 0; c < dim; ++c)
            out(r, c) = mat(r, c);
    return out;
}

QTensor<double> cmat_to_qtensor(cmat const& m) {
    auto const dim = m.shape(0);
    // NOTE -- must construct via TensorShape (not `{dim, dim}` brace-init,
    // which would match the rank-1 nested_initializer_list_t<DataType, 1>
    // overload and produce a 1-D length-2 tensor of values [dim, dim],
    // followed by an out-of-bounds assertion failure in the Decomposer
    // fallback path.
    TensorShape const shape{dim, dim};
    QTensor<double>   out{shape};
    for (size_t r = 0; r < dim; ++r)
        for (size_t c = 0; c < dim; ++c)
            out(r, c) = m(r, c);
    return out;
}

[[nodiscard]] size_t nth_gray(size_t n) { return n ^ (n >> 1); }

[[nodiscard]] unsigned popcount(size_t n) {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_popcount(static_cast<unsigned>(n)));
#else
    unsigned c = 0;
    while (n) {
        c += static_cast<unsigned>(n & 1U);
        n >>= 1;
    }
    return c;
#endif
}

// Uniformly-controlled RY on MSB qubit `n-1`, controls `0..n-2`.
// Ported from Cirq `quantum_shannon_decomposition._multiplexed_cossin`.
QCir multiplex_ry(size_t n_qubits, std::vector<double> const& angles) {
    QCir qc{n_qubits};
    if (n_qubits == 0 || angles.empty()) return qc;

    size_t const msb = n_qubits - 1;
    auto const n_angles = angles.size();

    for (size_t j = 0; j < n_angles; ++j) {
        double rotation = 0;
        for (size_t i = 0; i < n_angles; ++i) {
            auto const parity = popcount(nth_gray(j) & i) % 2;
            rotation += parity ? -angles[i] : angles[i];
        }
        rotation *= 2.0 / static_cast<double>(n_angles);

        size_t const select_string = nth_gray(j) ^ nth_gray(j + 1);
        ptrdiff_t select_qubit     = -1;
        for (size_t i = 0; i < n_angles; ++i) {
            if ((select_string >> i) & 1U) {
                select_qubit = static_cast<ptrdiff_t>(i);
                break;
            }
        }
        if (select_qubit >= 0) {
            select_qubit = std::max(-select_qubit - 1, -static_cast<ptrdiff_t>(n_angles - 1));
        }

        if (std::abs(rotation) > 1e-12) {
            qc.append(qcir::RYGate(dvlab::Phase(rotation)), {msb});
        }
        if (select_qubit >= 0 && static_cast<size_t>(select_qubit) < n_qubits - 1) {
            qc.append(qcir::CXGate(),
                      {static_cast<QubitIdType>(select_qubit), static_cast<QubitIdType>(msb)});
        }
    }
    return qc;
}

// Embed an (n-1)-qubit circuit on qubits {0,..,n-2} inside n qubits.
void embed_subcircuit(QCir& parent, QCir const& sub, size_t n_parent) {
    for (auto const* g : sub.get_gates()) {
        parent.append(g->get_operation(), g->get_qubits());
    }
    (void)n_parent;
}

std::optional<QCir> qsd_recursive(cmat const& U, size_t n, QSDOptions const& opt);

[[nodiscard]] std::optional<size_t> qubit_count_from_cmat(cmat const& m) {
    auto const dim = m.shape(0);
    if (dim != m.shape(1) || dim == 0 || (dim & (dim - 1)) != 0) return std::nullopt;
    size_t n = 0;
    for (auto d = dim; d > 1; d >>= 1) ++n;
    return n;
}

std::optional<QCir> synthesize_sub(cmat const& block, QSDOptions const& opt) {
    auto const n_opt = qubit_count_from_cmat(block);
    if (!n_opt.has_value()) return std::nullopt;
    return qsd_recursive(block, *n_opt, opt);
}

// MSB demultiplexer (Cirq `_msb_demuxer`).
std::optional<QCir> msb_demux(size_t n_qubits, cmat const& u1, cmat const& u2, QSDOptions const& opt) {
    auto const half = u1.shape(0);
    if (u2.shape(0) != half || half == 0) return std::nullopt;

    cmat const u2_adj = xt::conj(xt::transpose(u2));
    cmat const prod   = xt::linalg::dot(u1, u2_adj);
    cmat const herm   = 0.5 * (prod + xt::conj(xt::transpose(prod)));

    auto eig_pair = xt::linalg::eig(herm);
    auto evals    = std::get<0>(eig_pair);
    auto V        = std::get<1>(eig_pair);

    std::vector<c64> d(half);
    for (size_t i = 0; i < half; ++i) {
        d[i] = std::sqrt(evals(i));
        if (std::abs(evals(i)) < 1e-12) d[i] = c64{0, 0};
    }

    cmat D = xt::zeros<c64>({half, half});
    for (size_t i = 0; i < half; ++i) D(i, i) = d[i];
    cmat const W = xt::linalg::dot(xt::linalg::dot(D, xt::conj(xt::transpose(V))), u2);

    QCir result{n_qubits};

    auto w_circ = synthesize_sub(W, opt);
    if (!w_circ.has_value()) return std::nullopt;
    embed_subcircuit(result, *w_circ, n_qubits);

    std::vector<double> rz_angles(half);
    for (size_t i = 0; i < half; ++i) {
        rz_angles[i] = -std::arg(d[i]);
    }
    QCir rz_part{n_qubits};
    size_t const msb = n_qubits - 1;
    for (size_t j = 0; j < rz_angles.size(); ++j) {
        double rotation = 0;
        for (size_t i = 0; i < rz_angles.size(); ++i) {
            auto const parity = popcount(nth_gray(j) & i) % 2;
            rotation += parity ? -rz_angles[i] : rz_angles[i];
        }
        rotation *= 2.0 / static_cast<double>(rz_angles.size());
        size_t const select_string = nth_gray(j) ^ nth_gray(j + 1);
        ptrdiff_t select_qubit     = -1;
        for (size_t i = 0; i < rz_angles.size(); ++i) {
            if ((select_string >> i) & 1U) {
                select_qubit = static_cast<ptrdiff_t>(i);
                break;
            }
        }
        if (select_qubit >= 0) {
            select_qubit = std::max(-select_qubit - 1, -static_cast<ptrdiff_t>(rz_angles.size() - 1));
        }
        if (std::abs(rotation) > 1e-12) {
            rz_part.append(qcir::RZGate(dvlab::Phase(rotation)), {msb});
        }
        if (select_qubit >= 0 && static_cast<size_t>(select_qubit) < n_qubits - 1) {
            rz_part.append(qcir::CXGate(),
                           {static_cast<QubitIdType>(select_qubit), static_cast<QubitIdType>(msb)});
        }
    }
    result.compose(rz_part);

    auto v_circ = synthesize_sub(V, opt);
    if (!v_circ.has_value()) return std::nullopt;
    result.compose(*v_circ);

    return result;
}

std::optional<QCir> qsd_recursive(cmat const& U, size_t n, QSDOptions const& opt) {
    if (n == 1) return kak::single_qubit_synthesize(cmat_to_qtensor(U));
    if (n == 2) {
        kak::TwoQubitSynthesizeOptions kopt;
        kopt.try_three_cnot_qfactor = opt.try_three_cnot_qfactor;
        kopt.three_cnot_restarts    = opt.three_cnot_restarts;
        kopt.three_cnot_use_lbfgs   = opt.three_cnot_use_lbfgs;
        kopt.three_cnot_epsilon     = opt.synthesis_epsilon * 100;  // looser tol for QFactor.
        return kak::two_qubit_synthesize(cmat_to_qtensor(U), kopt);
    }

    auto cs = csd::cossin_separate(U);
    if (!cs.has_value()) {
        spdlog::warn("qsd: cossin failed for n={}; falling back to gray-code.", n);
        Decomposer dec;
        return dec.decompose(cmat_to_qtensor(U));
    }

    auto const target = cmat_to_qtensor(U);
    qcir::ScanningGateRemovalOptions scan_opt;
    scan_opt.synthesis_epsilon = opt.synthesis_epsilon;

    auto inter_round_scan = [&](QCir& circ) {
        if (!opt.inter_round_gate_removal) return;
        circ = qcir::scanning_gate_removal_pass(circ, target, scan_opt);
    };

    QCir circ{n};

    auto v_part = msb_demux(n, cs->v1h, cs->v2h, opt);
    if (!v_part.has_value()) return std::nullopt;
    circ.compose(*v_part);
    inter_round_scan(circ);

    circ.compose(multiplex_ry(n, cs->theta));
    inter_round_scan(circ);

    auto u_part = msb_demux(n, cs->u1, cs->u2, opt);
    if (!u_part.has_value()) return std::nullopt;
    circ.compose(*u_part);
    inter_round_scan(circ);

    return circ;
}

}  // namespace

std::optional<QCir> synthesize(QTensor<double> const& matrix, QSDOptions const& opt) {
    auto const n_opt = qubit_count_from_shape(matrix);
    if (!n_opt.has_value()) {
        spdlog::error("qsd::synthesize: input is not a 2^n x 2^n square matrix.");
        return std::nullopt;
    }
    auto const n = *n_opt;

    if (n == 1) {
        return kak::single_qubit_synthesize(matrix);
    }
    if (n == 2) {
        kak::TwoQubitSynthesizeOptions kopt;
        kopt.try_three_cnot_qfactor = opt.try_three_cnot_qfactor;
        kopt.three_cnot_restarts    = opt.three_cnot_restarts;
        kopt.three_cnot_use_lbfgs   = opt.three_cnot_use_lbfgs;
        kopt.three_cnot_epsilon     = opt.synthesis_epsilon * 100;
        auto kak_result = kak::two_qubit_synthesize(matrix, kopt);
        if (kak_result.has_value()) return kak_result;

        spdlog::warn("qsd::synthesize: KAK failed for 2-qubit input; falling back to gray-code decomposer.");
        Decomposer dec;
        return dec.decompose(matrix);
    }

    auto const U = qtensor_to_cmat(matrix);
    auto circ    = qsd_recursive(U, n, opt);
    if (!circ.has_value()) {
        spdlog::warn("qsd::synthesize: recursive QSD failed for n={}; gray-code fallback.", n);
        Decomposer dec;
        return dec.decompose(matrix);
    }
    return circ;
}

}  // namespace qsyn::tensor::qsd
