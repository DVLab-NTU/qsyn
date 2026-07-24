/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Linear Pauli-sum stream (staq rotation-folding / CPF global). ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"
#include "util/phase.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

using PauliLabelKey = std::string;  // `PauliProduct::to_bit_string()` (sign-normalised)

struct FoldTermsResult {
    std::unordered_map<PauliLabelKey, dvlab::Phase> pruned;
    std::size_t n_merged           = 0;
    std::size_t n_removed_zero     = 0;
    std::size_t n_removed_clifford = 0;
};

struct GlobalPruneStats {
    std::size_t n_terms_before     = 0;
    std::size_t n_terms_after      = 0;
    std::size_t n_merged_global    = 0;
    std::size_t n_removed_zero     = 0;
    std::size_t n_removed_clifford = 0;
    std::size_t n_rotations_pruned = 0;
};

// Ordered Clifford / rotation stream mirroring staq's `circuit_callback` and
// Python CPF's `extract_pe_stream_tableau` output.
using PauliDagEntry = std::variant<StabilizerTableau, PauliRotation>;

struct PauliDag {
    std::size_t n_qubits = 0;
    std::vector<PauliDagEntry> entries;
};

[[nodiscard]] PauliDag from_tableau(Tableau const& tableau);
[[nodiscard]] Tableau to_tableau(PauliDag const& dag);

// Sum phases per Pauli label across the whole stream (CPF `cpf_global`).
[[nodiscard]] std::unordered_map<PauliLabelKey, dvlab::Phase>
accumulate_global_terms(PauliDag const& dag);

// Merge / normalise / drop zero and Clifford-angle classes (CPF `fold_terms`).
[[nodiscard]] FoldTermsResult
fold_terms(std::unordered_map<PauliLabelKey, dvlab::Phase> const& raw);

// Labels present in `raw` but absent from `fold_terms(...).pruned`.
[[nodiscard]] std::unordered_set<PauliLabelKey>
globally_removed_labels(std::unordered_map<PauliLabelKey, dvlab::Phase> const& raw,
                        FoldTermsResult const& folded);

// Drop rotation nodes whose Pauli class was globally pruned; keep Clifford
// structure and surviving rotations with their original angles (Python replay).
GlobalPruneStats global_prune_pass(Tableau& tableau);

}  // namespace qsyn::experimental::cpf::pauli_dag
