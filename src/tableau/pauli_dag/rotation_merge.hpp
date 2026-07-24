/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ staq Rotation::try_merge / commute_left on PauliRotation. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <optional>
#include <utility>

#include "util/phase.hpp"

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

// `later` merges backward into the slot occupied by `earlier` (staq
// `later.try_merge(earlier)` with roles swapped: moving gate folds into prior).
// Returns global phase accumulated and the combined rotation for that slot.
[[nodiscard]] std::optional<std::pair<dvlab::Phase, PauliRotation>>
try_merge_into_earlier(PauliRotation const& later, PauliRotation const& earlier);

[[nodiscard]] bool rotations_commute(PauliRotation const& a, PauliRotation const& b);

// R(θ,P) with C on the left: C·R(θ,P) = R(θ,P')·C  =>  P' = C P C†
[[nodiscard]] PauliRotation commute_left(PauliRotation const& r,
                                        StabilizerTableau const& c);

}  // namespace qsyn::experimental::cpf::pauli_dag
