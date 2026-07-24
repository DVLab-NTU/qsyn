/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ staq rotation_folding::fold_forward on a flat timeline. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "./timeline.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

struct StaqFoldStats {
    std::size_t n_passes             = 0;
    std::size_t n_merges             = 0;
    std::size_t n_rotations_before   = 0;
    std::size_t n_rotations_after    = 0;
};

// One rotation at `index` is pushed backward through the timeline, merging
// into earlier rotations when `try_merge` succeeds and commuting through
// Clifford barriers otherwise (staq `fold_forward`).
[[nodiscard]] bool fold_rotation_at(std::vector<TimelineEntry>& entries, std::size_t index);

// Optional ID tracking for CPF_MERGE_LOG (call around staq + graph merge).
void merge_track_begin(Timeline& timeline, int pass_tag);
void merge_track_end();

// Repeated full-timeline sweeps until no merge occurs.
StaqFoldStats staq_fold_timeline(Timeline& timeline);

}  // namespace qsyn::experimental::cpf::pauli_dag
