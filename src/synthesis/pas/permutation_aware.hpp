/****************************************************************************
  PackageName  [ synthesis / pas ]
  Synopsis     [ Permutation-Aware Synthesis (PAS).

                 Given a target unitary U on n qubits and a base
                 synthesiser F(U) -> QCir, PAS searches over all n!
                 qubit permutations sigma, builds the permuted target
                 U' = P_sigma U P_sigma^{-1}, calls F(U') to get a
                 circuit C', and selects the permutation whose C' has
                 the fewest CX gates. The chosen sigma is then
                 "realised" by prepending / appending SWAP layers so
                 that the externally observable unitary matches U.

                 BQSKit reference: `bqskit/passes/synthesis/pas.py`
                 (PermutationAwareSynthesisPass). For n=3 the search
                 space is 6 permutations; for n=4 it is 24. We cap n
                 at 4 by default because the inner synth call (QSearch /
                 LEAP / QSD) dominates the cost and the win plateaus.

                 NOTE -- Connectivity / coupling-graph awareness is OUT
                 OF SCOPE: PAS picks the *logical* permutation that
                 minimises CX. Physical placement is a separate pass.
                 The inserted SWAPs are logical; downstream mapping or
                 a CPF global_fold can absorb them where possible. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <functional>
#include <optional>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::synthesis::pas {

using qsyn::tensor::QTensor;

struct PasOptions {
    // Hard cap on n_qubits the PAS loop will enumerate. For n above
    // this, PAS becomes a no-op (forwards to the base synthesiser
    // once on the identity permutation).
    std::size_t max_qubits = 4;
    // Insert physical SWAPs at the block boundary to realise the
    // permutation. When false the caller is responsible for tracking
    // / absorbing the permutation elsewhere.
    bool insert_boundary_swaps = true;
    // 0 = silent, 1 = summary, 2 = per-permutation residual / CX.
    std::size_t verbosity = 0;
};

// Functor signature for the base synthesiser. Returns nullopt to mean
// "give up on this permutation". The QCir it returns acts on the
// *permuted* qubit order (0..n-1 in the permuted frame).
using BaseSynth = std::function<std::optional<qcir::QCir>(QTensor<double> const& permuted_target)>;

// PAS driver. `target` is the original n-qubit unitary; `base_synth`
// is invoked at most n! times.
[[nodiscard]] std::optional<qcir::QCir>
pas_synthesize(QTensor<double> const& target,
               BaseSynth const& base_synth,
               PasOptions const& opt = {});

// Apply a qubit permutation to a 2^n x 2^n unitary matrix.
// `perm[i]` = the new index of qubit i (i.e. P|i_{n-1} ... i_0> =
// |i_{perm(n-1)} ... i_{perm(0)}>). Returns a fresh QTensor.
[[nodiscard]] QTensor<double>
apply_qubit_permutation(QTensor<double> const& U,
                        std::vector<std::size_t> const& perm);

}  // namespace qsyn::synthesis::pas
