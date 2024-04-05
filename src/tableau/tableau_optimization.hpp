/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include <concepts>
#include <functional>
#include <unordered_map>
#include <utility>
#include <list>

#include "./tableau.hpp"
#include "tableau/pauli_rotation.hpp"

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
    using Term_Set                      = std::unordered_map<size_t, PauliRotation>;
    using Partitions_Set                = std::list<Term_Set>;
    virtual ~MatroidPartitionStrategy() = default;

    virtual Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const = 0;

    bool is_independent(Polynomial const& polynomial, size_t num_ancillae) const;
    bool is_independent(Polynomial const& polynomial, Term_Set const& set, size_t num_ancillae) const;
};

/**
 * @brief partitions the given polynomial by naively picking terms until the matroid independence condition is violated
 *
 */
struct NaiveMatroidPartitionStrategy : public MatroidPartitionStrategy {
    Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const override;
};

struct TparPartitionStrategy : public MatroidPartitionStrategy {
    Partitions partition(Polynomial const& polynomial, size_t num_ancillae) const override;

    struct Path {
        std::list<std::pair<size_t, std::reference_wrapper<Term_Set>>> path_list;

        Path () {}
        Path (size_t i, std::reference_wrapper<Term_Set> t) { this->insert(i, t); }
        Path (size_t i, std::reference_wrapper<Term_Set> t,  const Path& p) : path_list(p.path_list)  {
            this->insert(i, t);
        }

        auto head() { return path_list.front(); }
        auto head_ele() { return path_list.front().first; }
        auto head_partition() { return path_list.front().second; }

        auto begin() { return path_list.begin(); }
        auto end() { return path_list.end(); }

        void insert(size_t i, std::reference_wrapper<Term_Set> t) { path_list.push_front(std::make_pair(i, t)); }
        void pop() { path_list.pop_front(); }
    };
    
    void print_termset(const Term_Set&) const;
};

inline bool is_phase_polynomial(std::vector<PauliRotation> const& polynomial) noexcept {
    return std::ranges::all_of(polynomial, [](PauliRotation const& rotation) { return rotation.is_diagonal(); }) &&
           std::ranges::all_of(polynomial, [n_qubits = polynomial.front().n_qubits()](PauliRotation const& rotation) { return rotation.n_qubits() == n_qubits; });
}

std::optional<std::vector<std::vector<PauliRotation>>> matroid_partition(std::vector<PauliRotation> const& polynomial, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);
std::optional<Tableau> matroid_partition(Tableau const& tableau, MatroidPartitionStrategy const& strategy, size_t num_ancillae = 0);

}  // namespace experimental

}  // namespace qsyn
