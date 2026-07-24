/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Iterate cross-Clifford propagation merges until fixpoint.
                 This realises the "global phase-polynomial fold" described
                 in the CPF reference implementation, but with continuous
                 (rational) angles tracked via `dvlab::Phase`. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "./propagation_merge.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental::cpf {

struct GlobalFoldStats {
    std::size_t n_passes               = 0;  // outer fixpoint iterations
    std::size_t n_local_merges         = 0;  // same-block PauliRotation merges
    std::size_t n_propagation_merges   = 0;  // cross-Clifford merges via `propagation_merge`
    std::size_t n_rotations_removed    = 0;  // identity rotations dropped overall
    std::size_t n_segments_collapsed   = 0;  // Stab/rot blocks that became empty and got pruned
    // Clifford-angle rotations (pi/2 multiples) that survived the final
    // fixpoint and are candidates for absorption by `full_optimize` /
    // `absorb_clifford_rotations`. Mirrors Python `cpf_angles::is_clifford_angle`.
    std::size_t n_clifford_angle_left  = 0;
};

// Iterate the following until the tableau structure stops changing:
//   1. Within every rotation block, run `merge_rotations` to fuse adjacent
//      same-Pauli rotations.
//   2. Run `propagation_merge` once to expose merges across one Clifford
//      segment.
//   3. Drop zero-phase rotations and prune empty segments.
//
// The interleaved structure of the input is preserved; rotations only ever
// move along boundaries (or disappear) - they never get permanently
// conjugated through a Clifford segment by this routine, which is exactly
// the property the CPF reference implementation relies on.
//
// For best results, build the input tableau with `cpf::trace_replay` so
// each Clifford / rotation segment matches the original gate ordering.
GlobalFoldStats global_fold(Tableau& tableau);

}  // namespace qsyn::experimental::cpf
