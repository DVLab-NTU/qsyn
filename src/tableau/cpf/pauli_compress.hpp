/**
 * @file
 * @brief Pauli-rotation compression for the CPF pipeline.
 *
 * Ports the compression strategies prototyped in
 * scripts/pca_compress/compress_ncf.py (D-merge / F-cancel / Clifford-snap
 * under an L2 budget, a.k.a. HYBRID) onto qsyn's native Tableau data
 * structure, and provides a fast `from-pauli-list` entry point that builds the
 * canonical `[Clifford | PauliRotation[]]` form directly from a Pauli list,
 * bypassing the QASM/QCir round-trip.
 *
 * @copyright Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "tableau/tableau.hpp"

namespace qsyn {

namespace experimental {

namespace cpf {

/**
 * @brief Build a canonical Tableau `[Identity-Clifford | PauliRotation[]]`
 *        directly from a list of (Pauli string, angle) pairs.
 *
 * For single-Trotter-step Hamiltonian-simulation circuits this is exactly the
 * result of running the full pipeline (to-zyz -> trace-replay -> fold ->
 * collapse), but in O(#Pauli) time instead of the O(#Pauli^3) cost incurred by
 * the QASM round-trip.
 *
 * @param n_qubits number of qubits.
 * @param rotations list of (pauli_string, angle) pairs. `pauli_string` is a
 *        string of I/X/Y/Z of length `n_qubits`; `angle` is the radian value of
 *        the rotation in the `exp(i * angle * P)` convention used by qsyn's
 *        Tableau printer.
 */
Tableau from_pauli_list(size_t n_qubits,
                        std::vector<std::pair<std::string, double>> const& rotations);

struct PauliCompressOptions {
    double l2_budget     = 0.0;   // L2 budget for lossy Clifford snapping; 0 => lossless (merge + cancel only)
    bool absorb_clifford = true;  // fold rotations snapped to a non-zero Clifford angle into the leading Clifford
};

struct PauliCompressStats {
    size_t n_before           = 0;
    size_t n_after            = 0;
    size_t n_merged           = 0;    // rotations removed by D-merge
    size_t n_cancelled        = 0;    // rotations removed by F-cancel (zero phase)
    size_t n_snapped_zero     = 0;    // rotations snapped to 0 (dropped)
    size_t n_snapped_clifford = 0;    // rotations snapped to a non-zero Clifford angle
    double l2_used            = 0.0;  // realized L2 cost of the snapping
};

/**
 * @brief Compress the Pauli rotations of a Tableau in place.
 *
 * Steps: collapse to canonical form -> merge identical Paulis (lossless) ->
 * cancel zero-phase rotations (lossless) -> greedily snap rotations to their
 * nearest Clifford angle (cheapest first) while the cumulative L2 cost stays
 * within `opts.l2_budget` (lossy).
 */
PauliCompressStats pauli_compress(Tableau& tableau, PauliCompressOptions const& opts = {});

}  // namespace cpf

}  // namespace experimental

}  // namespace qsyn
