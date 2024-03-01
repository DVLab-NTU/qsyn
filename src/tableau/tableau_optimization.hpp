/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include <concepts>

#include "./tableau.hpp"

namespace qsyn {

namespace experimental {

void collapse(Tableau& tableau);

void remove_identities(std::vector<PauliRotation>& rotation);
void remove_identities(Tableau& tableau);

void merge_rotations(std::vector<PauliRotation>& rotation);
void merge_rotations(Tableau& tableau);

std::pair<Tableau, StabilizerTableau> minimize_hadamards(Tableau tableau, StabilizerTableau context);
Tableau minimize_internal_hadamards(Tableau tableau);

struct MatroidPartitionStrategy {
    using Polynomial                    = std::vector<PauliRotation>;
    using Partitions                    = std::vector<Polynomial>;
    virtual ~MatroidPartitionStrategy() = default;

    virtual Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const = 0;

    bool is_independent(Polynomial const& polynomial, size_t num_ancillae) const;
};

struct NaiveMatroidPartitionStrategy : public MatroidPartitionStrategy {
    Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const override;
};

inline bool is_phase_polynomial(std::vector<PauliRotation> const& polynomial) noexcept {
    return std::ranges::all_of(polynomial, [](PauliRotation const& rotation) { return rotation.is_diagonal(); }) &&
           std::ranges::all_of(polynomial, [n_qubits = polynomial.front().n_qubits()](PauliRotation const& rotation) { return rotation.n_qubits() == n_qubits; });
}

std::vector<std::vector<PauliRotation>> matroid_partition(std::vector<PauliRotation> const& polynomial, size_t num_ancillae = 0, MatroidPartitionStrategy const& strategy = NaiveMatroidPartitionStrategy{});

}  // namespace experimental

}  // namespace qsyn
