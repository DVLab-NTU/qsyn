/****************************************************************************
  PackageName  [ synthesis / pas ]
  Synopsis     [ PermutationAwareSynthesis implementation. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./permutation_aware.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir_gate.hpp"
#include "tensor/tensor.hpp"
#include "tensor/tensor_util.hpp"

namespace qsyn::synthesis::pas {

namespace {

std::size_t extract_n_qubits(QTensor<double> const& t) {
    if (t.shape().size() != 2) return 0;
    auto const dim = t.shape()[0];
    if (dim < 2 || dim != t.shape()[1]) return 0;
    std::size_t n = 0;
    std::size_t d = dim;
    while (d > 1) {
        if ((d & 1u) != 0) return 0;
        d >>= 1u;
        ++n;
    }
    return n;
}

// Count CX gates in a QCir (matches the convention used in qsearch.cpp:
// ControlGate::get_type() returns "c<inner>", so CX is "cpx").
std::size_t count_cx(qcir::QCir const& circ) {
    std::size_t n = 0;
    for (auto const* g : circ.get_gates()) {
        auto const t = g->get_operation().get_type();
        if (t.size() >= 2 && t[0] == 'c' && t[1] == 'p') ++n;
    }
    return n;
}

// Remap basis index `idx` according to qubit permutation `perm`:
//   new bit at position perm[i] = old bit at position i
std::size_t permute_basis_index(std::size_t idx,
                                std::vector<std::size_t> const& perm) {
    std::size_t out = 0;
    for (std::size_t i = 0; i < perm.size(); ++i) {
        if (((idx >> i) & 1u) != 0) {
            out |= (1ULL << perm[i]);
        }
    }
    return out;
}

// Realise the chosen permutation with a SWAP network appended *after*
// `body`. We treat `body` as acting on the permuted qubit order; to
// recover the original logical order we permute the output qubits.
// A SWAP network is built using consecutive transpositions (bubble
// sort) -- not optimal in #SWAPs but valid for any permutation.
qcir::QCir build_swap_network(std::vector<std::size_t> const& perm) {
    qcir::QCir net{perm.size()};
    auto cur = perm;
    for (std::size_t i = 0; i < cur.size(); ++i) {
        for (std::size_t j = 0; j + 1 < cur.size() - i; ++j) {
            if (cur[j] > cur[j + 1]) {
                net.append(qcir::Operation{qcir::SwapGate()},
                           {static_cast<qsyn::QubitIdType>(j),
                            static_cast<qsyn::QubitIdType>(j + 1)});
                std::swap(cur[j], cur[j + 1]);
            }
        }
    }
    return net;
}

// Concatenate `b` onto the end of `a` (both n-qubit, in place via copy).
qcir::QCir concat(qcir::QCir const& a, qcir::QCir const& b) {
    qcir::QCir out = a;
    for (auto const* g : b.get_gates()) {
        out.append(g->get_operation(), g->get_qubits());
    }
    return out;
}

}  // namespace

QTensor<double> apply_qubit_permutation(QTensor<double> const& U,
                                        std::vector<std::size_t> const& perm) {
    auto const dim = U.shape()[0];
    auto const n   = perm.size();

    // Validate: shape and permutation length match.
    if (U.shape().size() != 2 || dim != (1ULL << n) || U.shape()[1] != dim) {
        return U;  // identity fallback; caller validated already
    }

    tensor::TensorShape const shape{dim, dim};
    QTensor<double> out{shape};
    // U'_{p(r), p(c)} = U_{r, c}   where p() is the basis-index remap.
    for (std::size_t r = 0; r < dim; ++r) {
        std::size_t const pr = permute_basis_index(r, perm);
        for (std::size_t c = 0; c < dim; ++c) {
            std::size_t const pc = permute_basis_index(c, perm);
            out(pr, pc)          = U(r, c);
        }
    }
    return out;
}

std::optional<qcir::QCir>
pas_synthesize(QTensor<double> const& target,
               BaseSynth const& base_synth,
               PasOptions const& opt) {
    auto const n = extract_n_qubits(target);
    if (n == 0) {
        spdlog::error("pas_synthesize: target must be a 2^n x 2^n unitary.");
        return std::nullopt;
    }
    if (n > opt.max_qubits) {
        if (opt.verbosity >= 1) {
            spdlog::info("pas: n={} > max_qubits={}, skipping enumeration.",
                         n, opt.max_qubits);
        }
        return base_synth(target);
    }

    // Initial permutation = identity.
    std::vector<std::size_t> identity(n);
    std::iota(identity.begin(), identity.end(), 0);

    std::optional<qcir::QCir> best;
    std::vector<std::size_t> best_perm = identity;
    std::size_t best_cx                = std::numeric_limits<std::size_t>::max();
    std::size_t tries                  = 0;

    auto const t_start = std::chrono::steady_clock::now();

    std::vector<std::size_t> perm = identity;
    do {
        ++tries;
        auto const t_one    = std::chrono::steady_clock::now();
        auto const permuted = apply_qubit_permutation(target, perm);
        auto result         = base_synth(permuted);
        auto const dt_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - t_one)
                               .count();

        if (!result.has_value()) {
            if (opt.verbosity >= 2) {
                spdlog::info("pas: try {} (perm=[{}]): base synth failed ({} ms).",
                             tries, fmt::join(perm, ","), dt_ms);
            }
            continue;
        }
        std::size_t const cx = count_cx(*result);
        if (opt.verbosity >= 2) {
            spdlog::info("pas: try {} (perm=[{}]): CX={} ({} ms).",
                         tries, fmt::join(perm, ","), cx, dt_ms);
        }

        if (cx < best_cx) {
            best_cx   = cx;
            best      = std::move(*result);
            best_perm = perm;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));

    if (!best.has_value()) {
        spdlog::warn("pas_synthesize: all {} permutations failed.", tries);
        return std::nullopt;
    }

    if (opt.verbosity >= 1) {
        auto const total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - t_start)
                                  .count();
        spdlog::info("pas: best perm = [{}], CX = {} (after {} tries, {} ms).",
                     fmt::join(best_perm, ","), best_cx, tries, total_ms);
    }

    // For the identity permutation, no SWAP layer is needed; return the
    // raw result.
    if (best_perm == identity || !opt.insert_boundary_swaps) {
        return best;
    }

    // Compose: SWAP-network (logical: undo perm) o body. The body acts
    // on the permuted-qubit frame; the SWAP layer brings qubits back
    // to the original logical ordering.
    auto swap_net = build_swap_network(best_perm);
    return concat(*best, swap_net);
}

}  // namespace qsyn::synthesis::pas
