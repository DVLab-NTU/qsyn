/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Continuous Phase Folding -- propagation merge across a
                 single Clifford segment. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental::cpf {

// Classification result for a segment `[R_{P_L}(theta_l), C, R_{P_R}(theta_r)]`.
//
//   - `same_pauli`             : `C` is identity AND `P_L == P_R`.
//   - `propagation_aligned`    : `C * P_L * C^dagger ==  P_R`.
//   - `propagation_negated`    : `C * P_L * C^dagger == -P_R`.
//   - `blocked`                : none of the above; the two rotations
//                                cannot be fused without altering `C`.
enum class MergeClass : std::uint8_t {
    same_pauli,
    propagation_aligned,
    propagation_negated,
    blocked,
};

// A pure (no-side-effect) classification of a segment. The `sign` field
// reports the multiplier to apply to `theta_l` when merging into `theta_r`:
//
//   theta_r' = theta_r + sign * theta_l                 (mergeable cases)
//   sign     = 0                                        (blocked)
struct SegmentReport {
    MergeClass   klass;
    int          sign;
    PauliProduct propagated_pauli;  // C * P_L * C^dagger (sign-stripped)
};

// Classify the segment `[left, intermediate, right]`. Pass an identity
// StabilizerTableau when no Clifford sits between `left` and `right`.
[[nodiscard]] SegmentReport classify_segment(
    PauliRotation const&     left,
    StabilizerTableau const& intermediate,
    PauliRotation const&     right);

struct PropagationMergeStats {
    std::size_t n_same_pauli{0};
    std::size_t n_propagation{0};
    std::size_t n_removed_zero{0};
    std::size_t n_passes{0};
};

// Greedy propagation-merge driver over a full Tableau. Iteratively merges
// boundary rotations across each `[..., rotations, StabilizerTableau,
// rotations, ...]` pattern until a full pass yields no further merges.
//
// The Tableau is left in a "remove_identities"-canonical state on return.
PropagationMergeStats propagation_merge(Tableau& tableau);

}  // namespace qsyn::experimental::cpf
