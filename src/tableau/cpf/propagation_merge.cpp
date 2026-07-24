/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Continuous Phase Folding -- propagation merge across a
                 single Clifford segment. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./propagation_merge.hpp"

#include <spdlog/spdlog.h>

#include <variant>

#include "./angle_utils.hpp"
#include "./merge_log.hpp"
#include "tableau/tableau_optimization.hpp"

namespace qsyn::experimental::cpf {

namespace {

// Compute `C * P * C^dagger` where `C` is given as a StabilizerTableau.
//
// `extract_clifford_operators` returns a CliffordOperatorString `ops` such
// that applying `ops` (in order) to an identity StabilizerTableau reproduces
// `C`. The same `ops`, when applied via PauliProduct::apply, performs the
// forward conjugation step-by-step, ultimately yielding `C * P * C^dagger`.
//
// In particular, when `C` is the identity the returned `ops` is empty and
// this function is the identity on `P`.
[[nodiscard]] PauliProduct propagate_through(PauliProduct const&      p,
                                             StabilizerTableau const& c) {
    auto       result = p;
    auto const ops    = extract_clifford_operators(c);
    result.apply(ops);
    return result;
}

}  // namespace

SegmentReport classify_segment(PauliRotation const&     left,
                               StabilizerTableau const& intermediate,
                               PauliRotation const&     right) {
    auto const intermediate_is_identity = intermediate.is_identity();

    // Forward-propagate `P_L` through the intermediate Clifford.
    auto       propagated = propagate_through(left.pauli_product(), intermediate);
    auto const sign       = propagated.is_neg() ? -1 : +1;
    if (propagated.is_neg()) propagated.negate();

    MergeClass klass = MergeClass::blocked;
    int        s     = 0;
    if (propagated == right.pauli_product()) {
        if (intermediate_is_identity && sign == 1) {
            klass = MergeClass::same_pauli;
            s     = 1;
        } else if (sign == 1) {
            klass = MergeClass::propagation_aligned;
            s     = 1;
        } else {
            klass = MergeClass::propagation_negated;
            s     = -1;
        }
    }

    return SegmentReport{klass, s, std::move(propagated)};
}

namespace {

// Apply one merge to the rotations on either side of `intermediate`,
// assuming the caller has already verified the segment is mergeable
// (i.e. `report.klass != blocked`).
//
// Concretely:
//   - the *last* rotation of `left_block` is removed,
//   - the *first* rotation of `right_block` gets `phase += sign * theta_l`.
//
// Returns true if a merge took place.
bool try_merge_boundary(std::vector<PauliRotation>& left_block,
                        StabilizerTableau const&    intermediate,
                        std::vector<PauliRotation>& right_block,
                        PropagationMergeStats&      stats) {
    if (left_block.empty() || right_block.empty()) return false;

    auto&      left      = left_block.back();
    auto&      right     = right_block.front();

    // Cheap pre-screen: if `left` is already zero-phase it cannot
    // contribute anything to `right` even if the segment is mergeable.
    // Dropping it here saves a classify_segment + propagate_through call
    // and keeps `angle_utils` predicates exercised inside the pipeline.
    if (is_zero_phase(left.phase())) {
        left_block.pop_back();
        return true;
    }

    auto const report    = classify_segment(left, intermediate, right);
    if (report.klass == MergeClass::blocked) return false;

    if (merge_log_enabled()) {
        char const* klass = "blocked";
        switch (report.klass) {
            case MergeClass::same_pauli:
                klass = "same_pauli";
                break;
            case MergeClass::propagation_aligned:
                klass = "propagation_aligned";
                break;
            case MergeClass::propagation_negated:
                klass = "propagation_negated";
                break;
            case MergeClass::blocked:
                klass = "blocked";
                break;
        }
        auto const ops = extract_clifford_operators(intermediate);
        merge_log_line(fmt::format(
            "{{\"event\":\"merge\",\"kind\":\"propagation\","
            "\"class\":\"{}\",\"sign\":{},"
            "\"src_pauli_at_site\":\"{}\",\"src_phase\":\"{}\","
            "\"src_pauli_propagated\":\"{}\","
            "\"dst_pauli\":\"{}\",\"dst_phase_before\":\"{}\","
            "\"clifford_ops\":\"{}\"}}",
            klass, report.sign,
            left.pauli_product().to_string('+'), left.phase().get_print_string(),
            report.propagated_pauli.to_string('+'),
            right.pauli_product().to_string('+'), right.phase().get_print_string(),
            format_clifford_ops(ops)));
    }

    // `report.sign` is +-1; multiply by an int (which is unambiguously
    // arithmetic) to avoid the Phase*Phase overload ambiguity.
    right.phase() += left.phase() * report.sign;
    left_block.pop_back();

    if (report.klass == MergeClass::same_pauli) {
        ++stats.n_same_pauli;
    } else {
        ++stats.n_propagation;
    }
    return true;
}

}  // namespace

PropagationMergeStats propagation_merge(Tableau& tableau) {
    PropagationMergeStats stats;
    if (tableau.is_empty()) return stats;

    auto const n_qubits  = tableau.n_qubits();
    auto const identity  = StabilizerTableau{n_qubits};

    bool changed = true;
    while (changed) {
        changed = false;
        ++stats.n_passes;

        // Scan triples of consecutive sub-tableaux. We handle two patterns:
        //   (a) [..., rotations, StabilizerTableau, rotations, ...]
        //       -> classic propagation merge across the Clifford.
        //   (b) [..., rotations, rotations, ...]
        //       -> degenerate "no-Clifford" case; reuse the same machinery
        //          with an identity intermediate.
        //
        // We re-index each scan so that we can mutate the tableau without
        // tripping over invalidated iterators.
        for (std::size_t i = 0; i + 1 < tableau.size();) {
            auto*       left_block        = std::get_if<std::vector<PauliRotation>>(&tableau[i]);
            auto const* mid_clifford      = (i + 2 < tableau.size())
                                                ? std::get_if<StabilizerTableau>(&tableau[i + 1])
                                                : nullptr;
            auto*       right_block_with_c = (mid_clifford != nullptr)
                                                ? std::get_if<std::vector<PauliRotation>>(&tableau[i + 2])
                                                : nullptr;
            auto*       right_block_no_c   = std::get_if<std::vector<PauliRotation>>(&tableau[i + 1]);

            bool merged_here = false;

            if (left_block != nullptr && mid_clifford != nullptr && right_block_with_c != nullptr) {
                merged_here = try_merge_boundary(*left_block, *mid_clifford,
                                                 *right_block_with_c, stats);
            } else if (left_block != nullptr && right_block_no_c != nullptr) {
                merged_here = try_merge_boundary(*left_block, identity,
                                                 *right_block_no_c, stats);
            }

            if (merged_here) {
                changed = true;
                // Keep `i` to allow further merges on the same boundary
                // (since the new last rotation of the left block may now
                // align with the new first rotation of the right block).
                continue;
            }
            ++i;
        }

        // Canonicalise after every pass: drop zero-phase rotations, fold
        // adjacent identity Cliffords / empty rotation lists, etc.
        auto const n_rot_before = tableau.n_pauli_rotations();
        remove_identities(tableau);
        auto const n_rot_after  = tableau.n_pauli_rotations();
        if (n_rot_after < n_rot_before) {
            stats.n_removed_zero += (n_rot_before - n_rot_after);
        }
    }

    return stats;
}

}  // namespace qsyn::experimental::cpf
