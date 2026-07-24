/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Iterate cross-Clifford propagation merges until fixpoint. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./global_fold.hpp"

#include <algorithm>
#include <variant>

#include "./angle_utils.hpp"
#include "tableau/tableau_optimization.hpp"

namespace qsyn::experimental::cpf {

namespace {

// Run `merge_rotations` on every rotation block in `tableau`. Returns the
// number of rotations that were fused (i.e. lost) in this pass.
std::size_t local_merge_all_blocks(Tableau& tableau) {
    std::size_t fused = 0;
    for (auto& sub : tableau) {
        auto* rot = std::get_if<std::vector<PauliRotation>>(&sub);
        if (rot == nullptr) continue;
        auto const before = rot->size();
        merge_rotations(*rot);  // does a same-Pauli sweep + remove_identities
        auto const after = rot->size();
        if (after < before) fused += (before - after);
    }
    return fused;
}

// Count rotations whose angle is an exact integer multiple of pi/2.
// These are "Clifford-equivalent" rotations: they could be absorbed
// into the surrounding StabilizerTableau via
// `tableau_optimization::absorb_clifford_rotations` (which is called by
// `merge_rotations(Tableau&)` / `full_optimize`). We only *count* them
// in `global_fold` to surface the opportunity; the actual absorption is
// owned by `full_optimize` because it requires the tableau to be in the
// canonical (single-prefix) form first. Mirrors Python CPF's
// `cpf_angles::is_clifford_angle` reporting.
std::size_t count_clifford_angle_rotations(Tableau const& tableau) {
    std::size_t clifford_count = 0;
    for (auto const& sub : tableau) {
        auto const* rot = std::get_if<std::vector<PauliRotation>>(&sub);
        if (rot == nullptr) continue;
        for (auto const& r : *rot) {
            if (is_zero_phase(r.phase())) continue;  // already a no-op
            if (is_clifford_phase(r.phase())) ++clifford_count;
        }
    }
    return clifford_count;
}

// Prune empty rotation blocks and adjacent identity Stabilizer blocks left
// behind by previous merges. Two consecutive StabilizerTableau segments are
// merged by composing the trailing one into the leading one and dropping
// the trailing one.
std::size_t prune_empty_segments(Tableau& tableau) {
    std::size_t removed = 0;

    // 1) Drop empty rotation blocks. The first (initial) StabilizerTableau
    //    is kept even if it is the identity, so the tableau remains valid.
    for (auto it = tableau.begin(); it != tableau.end();) {
        if (auto const* rot = std::get_if<std::vector<PauliRotation>>(&*it); rot != nullptr && rot->empty()) {
            it = tableau.erase(it, std::next(it));
            ++removed;
        } else {
            ++it;
        }
    }

    // 2) Coalesce neighbouring StabilizerTableau segments. After step 1 the
    //    rotation blocks between two Cliffords may be gone, leaving back-
    //    to-back Cliffords that can simply be composed together using the
    //    existing `apply` API.
    for (auto it = tableau.begin(); it != tableau.end();) {
        auto next = std::next(it);
        if (next == tableau.end()) break;

        auto* lhs = std::get_if<StabilizerTableau>(&*it);
        auto* rhs = std::get_if<StabilizerTableau>(&*next);
        if (lhs != nullptr && rhs != nullptr) {
            // Compose: apply rhs after lhs by re-playing rhs's Clifford
            // operator string on top of lhs. This is the same idiom used by
            // `tableau_optimization.cpp::merge_clifford_segments`.
            lhs->apply(extract_clifford_operators(*rhs));
            it = tableau.erase(next, std::next(next));
            ++removed;
            // step `it` back to recheck adjacency with the previous Stab.
            if (it != tableau.begin()) --it;
        } else {
            ++it;
        }
    }

    return removed;
}

}  // namespace

GlobalFoldStats global_fold(Tableau& tableau) {
    GlobalFoldStats stats;

    while (true) {
        ++stats.n_passes;
        auto const rots_before = tableau.n_pauli_rotations();

        // Step 1: within-block fusion.
        auto const local_fused = local_merge_all_blocks(tableau);
        stats.n_local_merges += local_fused;

        // Step 2: across-Clifford propagation merge.
        auto const pm = propagation_merge(tableau);
        stats.n_propagation_merges += pm.n_same_pauli + pm.n_propagation;

        // Step 3: prune blocks that became empty / coalesce neighbouring Stabs.
        auto const pruned = prune_empty_segments(tableau);
        stats.n_segments_collapsed += pruned;

        auto const rots_after = tableau.n_pauli_rotations();
        stats.n_rotations_removed += (rots_before - rots_after);

        // Fixpoint: no merges and no segment pruning this pass.
        bool const changed = local_fused > 0 ||
                             pm.n_same_pauli + pm.n_propagation > 0 ||
                             pruned > 0;
        if (!changed) break;
    }

    // Report -- but do not act on -- Clifford-angle rotations. Acting on
    // them needs the single-prefix canonical form that `full_optimize`
    // (= `tableau optimize tmerge`) produces; users that want maximum
    // reduction should follow `cpf-global` with `cpf-full`.
    stats.n_clifford_angle_left = count_clifford_angle_rotations(tableau);
    return stats;
}

}  // namespace qsyn::experimental::cpf
