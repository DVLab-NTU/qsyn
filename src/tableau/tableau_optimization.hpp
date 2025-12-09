/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include "./tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"

namespace qsyn {

namespace experimental {

void full_optimize(Tableau& tableau);

void collapse(Tableau& tableau);
void collapse_with_classical(Tableau& tableau);

void remove_identities(std::vector<PauliRotation>& rotation);
void remove_identities(Tableau& tableau);

void absorb_clifford_rotations(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations);
void properize(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations);

void properize(Tableau& tableau);

void merge_rotations(std::vector<PauliRotation>& rotation);
void merge_rotations(Tableau& tableau);

// hadamard minimization
// implemented in ./optimize/internal_h_opt.cpp

void minimize_internal_hadamards(Tableau& tableau);
void minimize_internal_hadamards_n_gadgetize(Tableau& tableau);

// H gadgetization - replaces H gates with gadgets using ancilla qubits and measurements
std::pair<Tableau, StabilizerTableau> minimize_hadamards_n_gadgetize(Tableau tableau, StabilizerTableau context);

struct PhasePolynomialOptimizationStrategy {
    using Polynomial                               = std::vector<PauliRotation>;
    virtual ~PhasePolynomialOptimizationStrategy() = default;

    virtual std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const = 0;
};

struct ToddPhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
    std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const override;
};

struct FastToddPhasePolynomialOptimizationStrategy : public PhasePolynomialOptimizationStrategy {
    std::pair<StabilizerTableau, Polynomial> optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const override;
};

void optimize_phase_polynomial(StabilizerTableau& clifford, std::vector<PauliRotation>& polynomial, PhasePolynomialOptimizationStrategy const& strategy);
void optimize_phase_polynomial(Tableau& tableau, PhasePolynomialOptimizationStrategy const& strategy);

struct MatroidPartitionStrategy {
    using Polynomial                    = std::vector<PauliRotation>;
    using Partitions                    = std::vector<Polynomial>;
    virtual ~MatroidPartitionStrategy() = default;

    virtual Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const = 0;

    bool is_independent(Polynomial const& polynomial, size_t num_ancillae) const;
};

/**
 * @brief partitions the given polynomial by naively picking terms until the matroid independence condition is violated
 *
 */
struct NaiveMatroidPartitionStrategy : public MatroidPartitionStrategy {
    Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const override;
};

inline bool is_phase_polynomial(std::vector<PauliRotation> const& polynomial) noexcept {
    return std::ranges::all_of(polynomial, [](PauliRotation const& rotation) { return rotation.is_diagonal(); }) &&
           std::ranges::all_of(polynomial, [n_qubits = polynomial.front().n_qubits()](PauliRotation const& rotation) { return rotation.n_qubits() == n_qubits; });
}

std::optional<std::vector<std::vector<PauliRotation>>> matroid_partition(std::vector<PauliRotation> const& polynomial, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);
std::optional<Tableau> matroid_partition(Tableau const& tableau, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);

}  // namespace experimental

}  // namespace qsyn
