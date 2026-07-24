/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Build an interleaved Tableau from a QCir, preserving the
                 original Clifford / non-Clifford segmentation. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <optional>

#include "qcir/qcir.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental::cpf {

// Walk through every gate of `qcir` in application order and produce a
// Tableau of the form
//
//   [Stab_0, rotations_0, Stab_1, rotations_1, ..., Stab_k, rotations_k]
//
// where each Stab_i is a StabilizerTableau collecting consecutive Clifford
// gates, and each rotations_i is a `vector<PauliRotation>` collecting
// consecutive non-Clifford rotations. Identity segments are kept too so the
// segmentation matches the original gate stream; the user is free to call
// `remove_identities` afterwards.
//
// Unlike `to_tableau(QCir)`, no Clifford absorption / rotation conjugation
// is performed. The Pauli operator of each rotation is exactly the axis the
// gate was written in. This is the input form required by
// `propagation_merge` / `global_fold` to expose cross-Clifford merge
// opportunities.
//
// Returns std::nullopt if any gate in the circuit is not supported by
// `append_to_tableau`.
[[nodiscard]] std::optional<Tableau> trace_replay(qcir::QCir const& qcir);

}  // namespace qsyn::experimental::cpf
