/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <functional>
#include <gsl/narrow>
#include <optional>
#include <ranges>
#include <string>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

/**
 * @brief Perform the best-known optimization routine on the tableau. The strategy may change in the future.
 *
 * @param tableau
 */
void full_optimize(Tableau& tableau) {
    size_t non_clifford_count = SIZE_MAX;
    size_t count              = 0;
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        non_clifford_count = tableau.n_pauli_rotations();
        spdlog::debug("TMerge");
        merge_rotations(tableau);
        spdlog::debug("Internal-H-opt");
        minimize_internal_hadamards(tableau);
        spdlog::debug("Phase polynomial optimization");
        optimize_phase_polynomial(tableau, ToddPhasePolynomialOptimizationStrategy{});
        spdlog::info("{}: Reduced the number of non-Clifford gates from {} to {}.", ++count, non_clifford_count, tableau.n_pauli_rotations());
    } while (non_clifford_count > tableau.n_pauli_rotations());
    minimize_internal_hadamards(tableau);
}

void optimize_for_equiv(Tableau& tableau) {
    spdlog::debug("TMerge (equiv path)");
    merge_rotations(tableau);
    spdlog::debug("Internal-H-opt (equiv path)");
    minimize_internal_hadamards(tableau);
}

namespace {
/**
 * @brief A view of the conjugation of a Clifford and a list of PauliRotations.
 *
 */
class ConjugationView : public PauliProductTrait<ConjugationView> {
public:
    ConjugationView(
        StabilizerTableau& clifford,
        std::vector<PauliRotation>& rotations,
        size_t upto) : _clifford{clifford}, _rotations{rotations}, _upto{upto} {}

    ConjugationView& h(size_t qubit) noexcept override {
        _clifford.get().h(qubit);
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].h(qubit);
        }
        return *this;
    }

    ConjugationView& s(size_t qubit) noexcept override {
        _clifford.get().s(qubit);
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].s(qubit);
        }
        return *this;
    }

    ConjugationView& cx(size_t control, size_t target) noexcept override {
        _clifford.get().cx(control, target);
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].cx(control, target);
        }
        return *this;
    }

private:
    std::reference_wrapper<StabilizerTableau> _clifford;
    std::reference_wrapper<std::vector<PauliRotation>> _rotations;
    size_t _upto;
};

}  // namespace

/**
 * @brief Pushing all the Clifford operators to the first sub-tableau and merging all the Pauli rotations.
 *
 * @param tableau
 */
void collapse(Tableau& tableau) {
    size_t const n_qubits = tableau.n_qubits();

    if (tableau.is_empty()) {
        return;
    }

    // prepend a stabilizer tableau to the front if the first sub-tableau is a list of PauliRotations
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{n_qubits});
    }

    if (tableau.size() <= 1) return;

    // make all clifford operators to be the identity except the first one
    auto clifford_string = CliffordOperatorString{};
    for (auto& subtableau : tableau | std::views::reverse) {
        std::visit(
            dvlab::overloaded(
                [&clifford_string](StabilizerTableau& st) {
                    st.apply(clifford_string);
                    clifford_string = extract_clifford_operators(st);
                },
                [&clifford_string](std::vector<PauliRotation>& pr) {
                    for (auto& rotation : pr) {
                        rotation.apply(clifford_string);
                    }
                }),
            subtableau);
    }

    // remove all clifford operators except the first one
    tableau.erase(
        std::remove_if(
            std::next(tableau.begin()),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                return std::holds_alternative<StabilizerTableau>(subtableau);
            }),
        tableau.end());

    if (tableau.size() == 1) {
        return;
    }

    // merge all rotations into the first rotation list
    auto& pauli_rotations = std::get<std::vector<PauliRotation>>(tableau[1]);

    for (auto& subtableau : tableau | std::views::drop(2)) {
        auto const& rotations = std::get<std::vector<PauliRotation>>(subtableau);
        pauli_rotations.insert(pauli_rotations.end(), rotations.begin(), rotations.end());
    }

    tableau.erase(dvlab::iterator::next(tableau.begin(), 2), tableau.end());

    DVLAB_ASSERT(tableau.size() == 2, "The tableau must have at most 2 sub-tableaux");
    DVLAB_ASSERT(std::holds_alternative<StabilizerTableau>(tableau.front()), "The first sub-tableau must be a StabilizerTableau");
    DVLAB_ASSERT(std::holds_alternative<std::vector<PauliRotation>>(tableau.back()), "The second sub-tableau must be a list of PauliRotations");
}

/**
 * @brief remove the Pauli rotations that evaluate to identity.
 *
 * @param rotations
 */
void remove_identities(std::vector<PauliRotation>& rotations) {
    rotations.erase(
        std::remove_if(
            rotations.begin(),
            rotations.end(),
            [](PauliRotation const& rotation) {
                return rotation.phase() == dvlab::Phase(0) ||
                       rotation.pauli_product().is_identity();
            }),
        rotations.end());
}

/**
 * @brief remove the tableau by removing the identity clifford operators and the identity Pauli rotations.
 *
 * @param tableau
 */
void remove_identities(Tableau& tableau) {
    // remove redundant pauli rotations
    std::ranges::for_each(tableau, [](SubTableau& subtableau) {
        if (auto pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            remove_identities(*pr);
        }
    });

    tableau.erase(
        std::remove_if(
            tableau.begin(),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                return dvlab::match(
                    subtableau,
                    [](StabilizerTableau const& subtableau) { return subtableau.is_identity(); },
                    [](std::vector<PauliRotation> const& subtableau) { return subtableau.empty(); });
            }),
        tableau.end());

    // remove redundant clifford operators and merge adjacent pauli rotations if possible
    for (auto const& [this_tabl, next_tabl] : tl::views::adjacent<2>(tableau)) {
        auto this_clifford = std::get_if<StabilizerTableau>(&this_tabl);
        auto next_clifford = std::get_if<StabilizerTableau>(&next_tabl);
        if (!this_clifford || !next_clifford) continue;

        this_clifford->apply(extract_clifford_operators(*next_clifford));
        *next_clifford = StabilizerTableau{this_clifford->n_qubits()};
    }

    tableau.erase(
        std::remove_if(
            tableau.begin(),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                auto st = std::get_if<StabilizerTableau>(&subtableau);
                return st && st->is_identity();
            }),
        tableau.end());
}

/**
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *
 * @param rotations
 */
void merge_rotations(std::vector<PauliRotation>& rotations) {
    // merge two rotations if they are commutative and have the same underlying pauli product
    for (size_t i = 0; i < rotations.size(); ++i) {
        for (size_t j = i + 1; j < rotations.size(); ++j) {
            if (!is_commutative(rotations[i], rotations[j])) break;
            if (rotations[i].pauli_product() == rotations[j].pauli_product()) {
                rotations[i].phase() += rotations[j].phase();
                rotations[j].phase() = dvlab::Phase(0);
            }
        }
    }

    // remove all rotations with zero phase
    remove_identities(rotations);
}

/**
 * @brief Absorb the Clifford rotations in `rotations` into the `clifford` tableau.
 *
 * @param clifford
 * @param rotations
 */
void absorb_clifford_rotations(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations) {
    for (size_t const i : std::views::iota(0ul, rotations.size())) {
        if (rotations[i].phase() != dvlab::Phase(1, 2) &&
            rotations[i].phase() != dvlab::Phase(-1, 2) &&
            rotations[i].phase() != dvlab::Phase(1)) continue;

        auto conjugation_view = ConjugationView{clifford, rotations, i};

        auto [ops, qubit] = extract_clifford_operators(rotations[i]);

        conjugation_view.apply(ops);

        if (rotations[i].phase() == dvlab::Phase(1, 2)) {
            conjugation_view.s(qubit);
        } else if (rotations[i].phase() == dvlab::Phase(-1, 2)) {
            conjugation_view.sdg(qubit);
        } else {
            assert(rotations[i].phase() == dvlab::Phase(1));
            conjugation_view.z(qubit);
        }
        rotations[i].phase() = dvlab::Phase(0);

        adjoint_inplace(ops);

        conjugation_view.apply(ops);
    }

    // remove all rotations with zero phase
    remove_identities(rotations);
}

/**
 * @brief make all rotations proper by absorbing the Clifford effect into the initial Clifford operator.
 *
 * @param clifford
 * @param rotations
 */
void properize(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations) {
    merge_rotations(rotations);

    // checks if the phase is in the range [0, π/2)
    auto const is_proper_phase = [](dvlab::Phase const& phase) {
        auto const numerator   = phase.numerator();
        auto const denominator = phase.denominator();
        return 0 <= numerator && 2 * numerator < denominator;
    };

    // properize the rotations from the last to the first
    // the order is important because absorbing a rotation may change the phase of the preceding rotations
    for (size_t const i : std::views::iota(0ul, rotations.size()) | std::views::reverse) {
        auto complement_phase = dvlab::Phase(0);
        while (!is_proper_phase(rotations[i].phase())) {
            rotations[i].phase() -= dvlab::Phase(1, 2);
            complement_phase += dvlab::Phase(1, 2);
        }
        if (complement_phase == dvlab::Phase(0)) continue;

        auto [ops, qubit] = extract_clifford_operators(rotations[i]);

        auto conjugation_view = ConjugationView{clifford, rotations, i};

        conjugation_view.apply(ops);

        if (complement_phase == dvlab::Phase(1, 2)) {
            conjugation_view.s(qubit);
        } else if (complement_phase == dvlab::Phase(-1, 2)) {
            conjugation_view.sdg(qubit);
        } else {
            assert(complement_phase == dvlab::Phase(1));
            conjugation_view.z(qubit);
        }

        adjoint_inplace(ops);

        conjugation_view.apply(ops);
    }

    remove_identities(rotations);
}

void properize(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }
    // ensures that the first sub-tableau is a stabilizer tableau
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{tableau.n_qubits()});
    }

    // merge consecutive pauli rotation tableaux into one
    auto new_tableau = Tableau{tableau.n_qubits()};
    new_tableau.push_back(tableau.front());
    for (auto const& subtableau : tableau | std::views::drop(1)) {
        std::visit(
            dvlab::overloaded(
                [](StabilizerTableau& st1, StabilizerTableau const& st2) {
                    st1.apply(extract_clifford_operators(st2));
                },
                [&](StabilizerTableau const& /* st1 */, std::vector<PauliRotation> const& pr2) {
                    new_tableau.push_back(pr2);
                },
                [&](std::vector<PauliRotation> const& /* pr1 */, StabilizerTableau const& st2) {
                    new_tableau.push_back(st2);
                },
                [&](std::vector<PauliRotation>& pr1, std::vector<PauliRotation> const& pr2) {
                    pr1.insert(pr1.end(), pr2.begin(), pr2.end());
                }),
            new_tableau.back(), subtableau);
    }

    tableau = new_tableau;

    auto clifford = std::ref(std::get<StabilizerTableau>(tableau.front()));
    for (auto& subtableau : tableau | std::views::drop(1)) {
        std::visit(
            dvlab::overloaded(
                [&clifford](StabilizerTableau& st) {
                    clifford = std::ref(st);
                },
                [&clifford](std::vector<PauliRotation>& pr) {
                    properize(clifford.get(), pr);
                }),
            subtableau);
    }

    remove_identities(tableau);
}

/**
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *        If a rotation becomes Clifford, absorb it into the initial Clifford operator.
 *        This algorithm is inspired by the paper [[1903.12456] Optimizing T gates in Clifford+T circuit as $π/4$ rotations around Paulis](https://arxiv.org/abs/1903.12456)
 *
 * @param clifford
 * @param rotations
 */
void merge_rotations(Tableau& tableau) {
    collapse(tableau);

    if (tableau.size() <= 1) {
        return;
    }

    auto& clifford   = std::get<StabilizerTableau>(tableau.front());
    auto& rotations  = std::get<std::vector<PauliRotation>>(tableau.back());
    auto n_rotations = SIZE_MAX;
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        n_rotations = rotations.size();
        merge_rotations(rotations);
        absorb_clifford_rotations(clifford, rotations);
    } while (rotations.size() < n_rotations);
}

// phase polynomial optimization

/**
 * @brief Reduce the number of terms for the phase polynomial. If the polynomial is not a phase polynomial, do nothing.
 *
 * @param polynomial
 * @param strategy
 */
void optimize_phase_polynomial(StabilizerTableau& clifford, std::vector<PauliRotation>& polynomial, PhasePolynomialOptimizationStrategy const& strategy) {
    if (!is_phase_polynomial(polynomial)) {
        return;
    }

    std::tie(clifford, polynomial) = strategy.optimize(clifford, polynomial);
}

/**
 * @brief Reduce the number of terms for all phase polynomials in the tableau.
 *
 * @param tableau
 * @param strategy
 */
void optimize_phase_polynomial(Tableau& tableau, PhasePolynomialOptimizationStrategy const& strategy) {
    if (tableau.is_empty()) {
        return;
    }
    // if the first sub-tableau is a list of PauliRotations, prepend a stabilizer tableau to the front
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{tableau.n_qubits()});
    }

    auto last_clifford = std::ref(std::get<StabilizerTableau>(tableau.front()));
    for (auto& subtableau : tableau) {
        if (auto pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            optimize_phase_polynomial(last_clifford.get(), *pr, strategy);
        } else {
            last_clifford = std::get<StabilizerTableau>(subtableau);
        }
    }

    remove_identities(tableau);
}

// matroid partitioning

/**
 * @brief split the phase polynomial into matroids. The matroids are represented by a list of PauliRotations, which must be all-diagonal.
 *
 * @param polynomial
 * @return std::vector<std::vector<PauliRotation>>
 */
std::optional<std::vector<std::vector<PauliRotation>>> matroid_partition(std::vector<PauliRotation> const& polynomial, MatroidPartitionStrategy const& strategy, size_t num_ancillae) {
    if (!is_phase_polynomial(polynomial)) {
        return std::nullopt;
    }

    return strategy.partition(polynomial, num_ancillae);
}

/**
 * @brief split the phase polynomial into matroids. The matroids are represented by a list of PauliRotations, which must be all-diagonal.
 *
 * @param polynomial
 * @param strategy
 * @param num_ancillae
 * @return Tableau
 */
std::optional<Tableau> matroid_partition(Tableau const& tableau, MatroidPartitionStrategy const& strategy, size_t num_ancillae) {
    auto new_tableau = Tableau{tableau.n_qubits()};

    for (auto const& subtableau : tableau) {
        if (auto const pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            auto partitions = matroid_partition(*pr, strategy, num_ancillae);
            if (!partitions) {
                return std::nullopt;
            }
            for (auto const& partition : *partitions) {
                new_tableau.push_back(partition);
            }
        } else {
            new_tableau.push_back(subtableau);
        }
    }

    return new_tableau;
}

/**
 * @brief check if the terms of the polynomial are linearly independent; if so, the transformation |x_1, ..., x_m>|0...0> |--> |y_1, ..., y_n> is reversible
 *
 * @param polynomial
 * @param num_ancillae
 * @return true
 * @return false
 */
auto MatroidPartitionStrategy::is_independent(std::vector<PauliRotation> const& polynomial, size_t num_ancillae) const -> bool {
    DVLAB_ASSERT(is_phase_polynomial(polynomial), "The input pauli rotations a phase polynomial.");

    auto const dim_v = polynomial.front().n_qubits();
    auto const n     = dim_v + num_ancillae;
    // equivalent to the independence oracle lemma:
    //     dim(V) - rank(S) <= n - |S|
    // in the literature, where n is the number of qubits = polynomial dimension + num_ancillae
    // ref: [Polynomial-time T-depth Optimization of Clifford+T circuits via Matroid Partitioning](https://arxiv.org/pdf/1303.2042.pdf)
    // Here, we reorganize the inequality to make circumvent unsigned integer overflow
    return dim_v + polynomial.size() <= n + matrix_rank(polynomial);
};

MatroidPartitionStrategy::Partitions NaiveMatroidPartitionStrategy::partition(MatroidPartitionStrategy::Polynomial const& polynomial, size_t num_ancillae) const {
    auto matroids = std::vector(1, std::vector<PauliRotation>{});  // starts with an empty matroid

    if (polynomial.empty()) {
        return matroids;
    }

    for (auto const& term : polynomial) {
        matroids.back().push_back(term);
        if (!this->is_independent(matroids.back(), num_ancillae)) {
            matroids.back().pop_back();
            matroids.push_back({term});
        }
    }

    DVLAB_ASSERT(std::ranges::none_of(matroids, [](std::vector<PauliRotation> const& matroid) { return matroid.empty(); }), "The matroids must not be empty.");

    return matroids;
}

// ---------------------------------------------------------------------------
// Non-Clifford Fusion (NCF) - https://arxiv.org/abs/2510.13573
// ---------------------------------------------------------------------------

namespace {

size_t paulihedral_overlap(PauliProduct const& a, PauliProduct const& b) {
    size_t const width = std::max(a.n_qubits(), b.n_qubits());
    size_t overlap     = 0;
    for (size_t q = 0; q < width; ++q) {
        if (a.is_i(q) || b.is_i(q)) continue;
        if (a.get_pauli_type(q) == b.get_pauli_type(q)) ++overlap;
    }
    return overlap;
}

size_t paulihedral_chain_score(PauliProduct const& a, PauliProduct const& b,
                               std::vector<PauliRotation> const& rotations,
                               std::vector<size_t> const& active, size_t i, size_t j) {
    size_t score = 0;
    for (size_t k : active) {
        if (k == i || k == j) continue;
        score += std::max(paulihedral_overlap(a, rotations[k].pauli_product()),
                          paulihedral_overlap(b, rotations[k].pauli_product()));
    }
    return score;
}

/**
 * @brief Partition rotations with graph + generator guided NCF grouping.
 *
 * Procedure (iterative on unpartitioned terms):
 *  1) Build commuting / anti-commuting graphs.
 *  2) Run Gaussian elimination over Pauli symplectic vectors to get generator indices.
 *  3) Enumerate anti-commuting generator pairs in index order (default), or all
 *     anti-commuting active pairs ranked by Paulihedral overlap (--overlap-priority).
 *  4) Pick the first / highest-overlap pair; if their generated product exists and is
 *     unpartitioned, group {i,j,k}, otherwise group {i,j}.
 *  5) If no anti-commuting generator pair exists, remaining terms are mutually commuting
 *     and are emitted as singletons.
 */
std::vector<std::vector<size_t>> ncf_partition_single_qubit_groups(std::vector<PauliRotation> const& rotations,
                                                                   NcfFusionOptions const& options) {
    size_t const n = rotations.size();
    std::vector<std::vector<size_t>> groups;
    std::vector<bool> used(n, false);

    auto const bit_key = [&](PauliProduct const& p) {
        return p.to_bit_string();
    };
    auto const row_bits = [&](PauliProduct const& p) {
        std::vector<uint8_t> row(2 * p.n_qubits(), 0);
        for (size_t q = 0; q < p.n_qubits(); ++q) {
            row[q]                = p.is_z_set(q) ? 1 : 0;
            row[q + p.n_qubits()] = p.is_x_set(q) ? 1 : 0;
        }
        return row;
    };

    std::unordered_map<std::string, std::vector<size_t>> key_to_indices;
    key_to_indices.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        key_to_indices[bit_key(rotations[i].pauli_product())].push_back(i);
    }

    auto const find_unpartitioned_product = [&](size_t i, size_t j) -> std::optional<size_t> {
        PauliProduct p = rotations[i].pauli_product() * rotations[j].pauli_product();
        auto try_key   = [&](PauliProduct const& pp) -> std::optional<size_t> {
            auto it = key_to_indices.find(bit_key(pp));
            if (it == key_to_indices.end()) return std::nullopt;
            for (size_t idx : it->second) {
                if (!used[idx] && idx != i && idx != j) return idx;
            }
            return std::nullopt;
        };
        if (auto k = try_key(p)) return k;
        p.negate();
        return try_key(p);
    };

    auto has_unpartitioned = [&]() {
        return std::ranges::any_of(std::views::iota(size_t{0}, n), [&](size_t i) { return !used[i]; });
    };

    size_t iter = 0;
    while (has_unpartitioned()) {
        std::vector<size_t> active{};
        active.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (!used[i]) active.push_back(i);
        }

        // Build commuting / anti-commuting graph adjacency on the active set.
        std::vector<std::vector<bool>> anti_adj(active.size(), std::vector<bool>(active.size(), false));
        for (size_t a = 0; a < active.size(); ++a) {
            for (size_t b = a + 1; b < active.size(); ++b) {
                bool anti = !rotations[active[a]].is_commutative(rotations[active[b]]);
                anti_adj[a][b] = anti;
                anti_adj[b][a] = anti;
            }
        }

        // Print commutation graphs on current unpartitioned vertices.
        spdlog::warn("NCF graph iter {}: vertices = {}", iter, active.size());
        for (size_t ai = 0; ai < active.size(); ++ai) {
            auto idx = active[ai];
            spdlog::warn("  v#{}: {}", idx, rotations[idx].to_string('+'));
        }
        std::vector<std::string> anti_edges{};
        std::vector<std::string> comm_edges{};
        for (size_t a = 0; a < active.size(); ++a) {
            for (size_t b = a + 1; b < active.size(); ++b) {
                auto const u = active[a];
                auto const v = active[b];
                if (anti_adj[a][b]) {
                    anti_edges.push_back(fmt::format("({}, {})", u, v));
                } else {
                    comm_edges.push_back(fmt::format("({}, {})", u, v));
                }
            }
        }
        spdlog::warn("  anti-comm edges: {}", anti_edges.empty() ? std::string{"<none>"} : fmt::format("{}", fmt::join(anti_edges, ", ")));
        spdlog::warn("  commuting edges: {}", comm_edges.empty() ? std::string{"<none>"} : fmt::format("{}", fmt::join(comm_edges, ", ")));

        // Gaussian elimination over GF(2) on symplectic rows to select generator indices.
        std::vector<size_t> generators{};
        std::vector<std::vector<uint8_t>> basis_rows{};
        std::vector<int> pivots(2 * rotations.front().n_qubits(), -1);
        for (size_t idx : active) {
            auto v = row_bits(rotations[idx].pauli_product());
            for (size_t col = 0; col < v.size(); ++col) {
                if (!v[col]) continue;
                if (pivots[col] == -1) {
                    pivots[col] = gsl::narrow<int>(basis_rows.size());
                    basis_rows.push_back(v);
                    generators.push_back(idx);
                    goto next_row;
                }
                auto const& b = basis_rows[gsl::narrow<size_t>(pivots[col])];
                for (size_t k = 0; k < v.size(); ++k) v[k] ^= b[k];
            }
        next_row:;
        }

        // Extract anti-commuting pairs: generator order (default) or Paulihedral overlap priority.
        std::optional<std::pair<size_t, size_t>> picked_pair = std::nullopt;
        size_t best_overlap                                    = 0;
        size_t best_chain                                      = 0;

        auto consider_pair = [&](size_t i, size_t j) {
            auto const [lo, hi] = std::minmax(i, j);
            if (options.overlap_priority) {
                size_t const ov    = paulihedral_overlap(rotations[i].pauli_product(), rotations[j].pauli_product());
                size_t const chain = paulihedral_chain_score(rotations[i].pauli_product(), rotations[j].pauli_product(),
                                                             rotations, active, i, j);
                if (!picked_pair.has_value() || ov > best_overlap ||
                    (ov == best_overlap && chain > best_chain) ||
                    (ov == best_overlap && chain == best_chain && lo < picked_pair->first) ||
                    (ov == best_overlap && chain == best_chain && lo == picked_pair->first && hi < picked_pair->second)) {
                    picked_pair  = {lo, hi};
                    best_overlap = ov;
                    best_chain   = chain;
                }
            } else if (!picked_pair.has_value()) {
                picked_pair = {lo, hi};
            }
        };

        if (options.overlap_priority) {
            for (size_t a = 0; a < active.size(); ++a) {
                for (size_t b = a + 1; b < active.size(); ++b) {
                    if (anti_adj[a][b]) consider_pair(active[a], active[b]);
                }
            }
        } else {
            for (size_t gi = 0; gi < generators.size() && !picked_pair.has_value(); ++gi) {
                size_t const i = generators[gi];
                auto pos_i_it  = std::ranges::find(active, i);
                if (pos_i_it == active.end()) continue;
                size_t const ai = std::distance(active.begin(), pos_i_it);
                for (size_t gj = gi + 1; gj < generators.size(); ++gj) {
                    size_t const j = generators[gj];
                    auto pos_j_it  = std::ranges::find(active, j);
                    if (pos_j_it == active.end()) continue;
                    size_t const aj = std::distance(active.begin(), pos_j_it);
                    if (anti_adj[ai][aj]) {
                        consider_pair(i, j);
                        break;
                    }
                }
            }
        }

        if (picked_pair.has_value() && options.overlap_priority) {
            auto const [i, j] = *picked_pair;
            spdlog::warn("NCF graph iter {}: overlap-priority pick ({}, {}) overlap={} chain={}", iter, i, j,
                         paulihedral_overlap(rotations[i].pauli_product(), rotations[j].pauli_product()),
                         paulihedral_chain_score(rotations[i].pauli_product(), rotations[j].pauli_product(), rotations,
                                                 active, i, j));
        }

        if (!picked_pair.has_value()) {
            // Remaining unpartitioned terms are mutually commuting; partition them into
            // packs of size <= q (logical qubit count), as requested by the NCF policy.
            size_t const q = rotations.empty() ? 0 : rotations.front().n_qubits();
            size_t const pack_size = std::max<size_t>(1, q);
            for (size_t start = 0; start < active.size(); start += pack_size) {
                std::vector<size_t> pack{};
                for (size_t t = start; t < std::min(active.size(), start + pack_size); ++t) {
                    pack.push_back(active[t]);
                    used[active[t]] = true;
                }
                groups.push_back(std::move(pack));
            }
            break;
        }

        auto const [i, j] = *picked_pair;
        if (auto k = find_unpartitioned_product(i, j); k.has_value()) {
            spdlog::warn("NCF graph iter {}: picked anti-pair ({}, {}) with product {}", iter, i, j, *k);
            groups.push_back({i, j, *k});
            used[i]  = true;
            used[j]  = true;
            used[*k] = true;
        } else {
            spdlog::warn("NCF graph iter {}: picked anti-pair ({}, {}) with no unpartitioned product", iter, i, j);
            groups.push_back({i, j});
            used[i] = true;
            used[j] = true;
        }
        ++iter;
    }
    return groups;
}

bool ncf_pairwise_commutative(std::vector<PauliRotation> const& all_rotations, std::vector<size_t> const& indices) {
    for (size_t i = 0; i < indices.size(); ++i) {
        for (size_t j = i + 1; j < indices.size(); ++j) {
            if (!all_rotations[indices[i]].is_commutative(all_rotations[indices[j]])) return false;
        }
    }
    return true;
}

/**
 * @brief Build Clifford C that conjugates the first rotation to Z on one qubit and,
 *        if the group has a second anticommuting rotation, map it to X on that qubit.
 *        Apply C to all rotations in the group (in place).
 */
CliffordOperatorString ncf_build_single_qubit_conjugation(std::vector<PauliRotation>& group) {
    if (group.empty()) return {};
    using COT = CliffordOperatorType;

    // Paper-faithful single-qubit elimination (NCF):
    // Perform tableau-style elimination row-by-row so the group is conjugated to act on a
    // single pivot qubit. For the 1st row:
    //   (A) Eliminate all 1s in Z using only S and H so only X-ones remain.
    //   (B) Reduce X-ones down to a single 1 using CNOTs between columns with X=1.
    //   (C) Apply H on the remaining column to move the 1 into Z (defines pivot qubit).
    // For the 2nd row:
    //   (A) Same Z-elimination (avoid H on pivot to keep row1's Z_pivot intact; S on pivot is OK).
    //   (B) Reduce X-ones so the single remaining 1 is located at the same pivot column.
    auto ops = CliffordOperatorString{};

    StabilizerTableau st(group[0].n_qubits());
    std::vector<PauliRotation> rot_copy = group;
    ConjugationView view(st, rot_copy, rot_copy.size());

    auto const n_qubits = group[0].n_qubits();

    DVLAB_ASSERT(group.size() >= 2, "NCF single-qubit conjugation expects anticommuting group of size >= 2");

    auto const apply1 = [&](COT t, size_t q0, size_t q1 = 0) {
        ops.emplace_back(t, std::array<size_t, 2>{q0, q1});
        view.apply(CliffordOperatorString{ops.back()});
    };

    auto const z_eliminate_to_x_only = [&](size_t row, std::optional<size_t> forbid_h_on) {
        for (size_t q = 0; q < n_qubits; ++q) {
            auto const p = rot_copy[row].get_pauli_type(q);
            if (p == Pauli::z) {
                DVLAB_ASSERT(!forbid_h_on.has_value() || q != *forbid_h_on, "Row has Z on pivot; cannot apply H on pivot");
                apply1(COT::h, q);
            } else if (p == Pauli::y) {
                // S maps Y -> -X, eliminating the Z component.
                apply1(COT::s, q);
            }
        }
        // sanity: row is now I/X only
        DVLAB_ASSERT(std::ranges::all_of(std::views::iota(size_t{0}, n_qubits), [&](size_t q) {
                         return rot_copy[row].is_i(q) || rot_copy[row].is_x(q);
                     }),
                     "After Z-elimination, row should be X-only");
    };

    auto const x_support = [&](size_t row) {
        return std::views::iota(size_t{0}, n_qubits) |
               std::views::filter([&](size_t q) { return rot_copy[row].is_x(q); }) |
               tl::to<std::vector>();
    };

    // ---- Row 0 elimination: find pivot and make row0 == Z_pivot ----
    z_eliminate_to_x_only(0, std::nullopt);
    auto supp0 = x_support(0);
    DVLAB_ASSERT(!supp0.empty(), "Row0 should not become identity during elimination");

    // Reduce X-ones to a single 1 using CNOTs between columns with X=1:
    // If X_c = X_t = 1, applying CX(c->t) clears X_t and keeps X_c.
    while (supp0.size() > 1) {
        size_t const c = supp0[0];
        size_t const t = supp0[1];
        apply1(COT::cx, c, t);
        // recompute support (small n)
        supp0 = x_support(0);
    }
    size_t const pivot = supp0.front();
    // Move that single X into Z, defining the pivot column.
    apply1(COT::h, pivot);
    DVLAB_ASSERT(rot_copy[0].is_z(pivot), "Row0 should be Z on pivot after final H");
    DVLAB_ASSERT(std::ranges::all_of(std::views::iota(size_t{0}, n_qubits), [&](size_t q) {
                     return (q == pivot) ? rot_copy[0].is_z(q) : rot_copy[0].is_i(q);
                 }),
                 "Row0 should act only on pivot");

    // ---- Row 1 elimination: align to the same pivot, keep row0's Z_pivot intact ----
    DVLAB_ASSERT(rot_copy[1].is_x(pivot) || rot_copy[1].is_y(pivot),
                 "For an anticommuting pair, row1 must have X/Y on pivot after row0 becomes Z_pivot");

    // Eliminate Z components for row1. Do not apply H on pivot (would swap row0's Z_pivot).
    z_eliminate_to_x_only(1, pivot);

    auto supp1 = x_support(1);
    DVLAB_ASSERT(!supp1.empty(), "Row1 should not become identity during elimination");

    // Ensure pivot is in support (if not, toggle it on using CX(c->pivot) from some X=1 c).
    if (!rot_copy[1].is_x(pivot)) {
        size_t const c = supp1.front();
        DVLAB_ASSERT(c != pivot, "support should contain a non-pivot qubit if pivot is not set");
        apply1(COT::cx, c, pivot);
    }

    // Now eliminate all other Xs using CX(pivot->t) for each t with X=1, t != pivot.
    for (size_t t = 0; t < n_qubits; ++t) {
        if (t == pivot) continue;
        if (rot_copy[1].is_x(t)) {
            apply1(COT::cx, pivot, t);
        }
    }

    // Final sanity: row1 is X-only on pivot.
    DVLAB_ASSERT(rot_copy[1].is_x(pivot), "Row1 should be X on pivot after alignment");
    DVLAB_ASSERT(std::ranges::all_of(std::views::iota(size_t{0}, n_qubits), [&](size_t q) {
                     return (q == pivot) ? rot_copy[1].is_x(q) : rot_copy[1].is_i(q);
                 }),
                 "Row1 should act only on pivot");

    group = std::move(rot_copy);
    return ops;
}

/**
 * @brief Build Clifford ops C for a single Pauli rotation block so that after conjugation
 *        it acts non-trivially on exactly one qubit (i.e., map the Pauli string to Z on one qubit).
 */
CliffordOperatorString ncf_build_single_rotation_conjugation(std::vector<PauliRotation>& group) {
    DVLAB_ASSERT(group.size() == 1, "ncf_build_single_rotation_conjugation expects exactly one rotation");
    if (group.empty()) return {};

    auto const [ops, qubit] = extract_clifford_operators(group[0]);
    (void)qubit;
    StabilizerTableau st(group[0].n_qubits());
    std::vector<PauliRotation> rot_copy = group;
    ConjugationView view(st, rot_copy, rot_copy.size());
    view.apply(ops);
    group = std::move(rot_copy);
    return ops;
}

}  // namespace

void ncf_fusion(Tableau& tableau, NcfFusionOptions const& options) {
    if (tableau.is_empty()) return;
    collapse(tableau);
    if (tableau.size() < 2) return;

    DVLAB_ASSERT(std::holds_alternative<StabilizerTableau>(tableau.front()), "after collapse front is Clifford");
    DVLAB_ASSERT(std::holds_alternative<std::vector<PauliRotation>>(tableau.back()), "after collapse back is rotations");

    auto const n_qubits = tableau.n_qubits();
    StabilizerTableau front_clifford = std::get<StabilizerTableau>(tableau.front());
    std::vector<PauliRotation> all_rotations = std::get<std::vector<PauliRotation>>(tableau.back());

    if (all_rotations.empty()) return;

    std::vector<std::vector<size_t>> const groups = ncf_partition_single_qubit_groups(all_rotations, options);

    Tableau new_tableau(n_qubits);
    new_tableau.erase(new_tableau.begin(), new_tableau.end());
    new_tableau.push_back(SubTableau{std::move(front_clifford)});

    size_t group_number = 0;
    for (auto const& indices : groups) {
        bool const commuting_pack = indices.size() > 1 && ncf_pairwise_commutative(all_rotations, indices);
        auto emit_one_block       = [&](std::vector<size_t> const& block_indices, std::string const& label_suffix) {
            auto const label_orig = [&]() {
                std::string s;
                for (size_t k = 0; k < block_indices.size(); ++k) {
                    if (k > 0) s += ", ";
                    s += "#" + std::to_string(block_indices[k]);
                }
                return s;
            }();

            std::vector<PauliRotation> group_rotations;
            group_rotations.reserve(block_indices.size());
            for (size_t idx : block_indices) {
                group_rotations.push_back(all_rotations[idx]);
            }

            // For commuting packs, emit each term as a singleton [C†][R'][C] block.
            CliffordOperatorString ops = (block_indices.size() == 1) ? ncf_build_single_rotation_conjugation(group_rotations)
                                                                     : ncf_build_single_qubit_conjugation(group_rotations);

            StabilizerTableau c_dagger(n_qubits);
            c_dagger.apply(adjoint(ops));
            new_tableau.push_back(SubTableau{std::move(c_dagger)});
            new_tableau.set_block_ops(new_tableau.size() - 1, adjoint(ops));

            new_tableau.push_back(SubTableau{std::move(group_rotations)});
            new_tableau.set_block_label(new_tableau.size() - 1, "NCF group " + std::to_string(group_number) + label_suffix + " (original " + label_orig + ")");

            StabilizerTableau c_tableau(n_qubits);
            c_tableau.apply(ops);
            new_tableau.push_back(SubTableau{std::move(c_tableau)});
            new_tableau.set_block_ops(new_tableau.size() - 1, ops);
            ++group_number;
        };

        if (commuting_pack) {
            for (size_t idx : indices) emit_one_block({idx}, " commuting-pack");
        } else {
            emit_one_block(indices, "");
        }
    }

    tableau = std::move(new_tableau);
    spdlog::info("NCF fusion: partitioned into {} groups ({} total Pauli rotations).", groups.size(), all_rotations.size());
}

namespace {

std::optional<CliffordOperatorString> ncf_try_build_conjugation(std::vector<PauliRotation>& group) {
    if (group.empty()) return CliffordOperatorString{};
    if (group.size() == 1) return ncf_build_single_rotation_conjugation(group);
    if (group.size() == 2 || group.size() == 3) {
        auto tmp = group;
        try {
            auto ops = ncf_build_single_qubit_conjugation(tmp);
            group    = std::move(tmp);
            return ops;
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::vector<std::vector<std::vector<size_t>>> ncf_group_merge_choices(std::vector<size_t> const& indices) {
    std::vector<std::vector<std::vector<size_t>>> choices{};
    if (indices.size() <= 1) {
        choices.push_back({indices});
        return choices;
    }
    if (indices.size() == 2) {
        choices.push_back({indices});
        choices.push_back({{indices[0]}, {indices[1]}});
        return choices;
    }
    if (indices.size() == 3) {
        choices.push_back({indices});
        choices.push_back({{indices[0], indices[1]}, {indices[2]}});
        choices.push_back({{indices[0], indices[2]}, {indices[1]}});
        choices.push_back({{indices[1], indices[2]}, {indices[0]}});
        choices.push_back({{indices[0]}, {indices[1]}, {indices[2]}});
        return choices;
    }
    choices.push_back({indices});
    return choices;
}

Tableau ncf_build_tableau_from_groups(
    size_t n_qubits,
    StabilizerTableau const& front_clifford,
    std::vector<PauliRotation> const& all_rotations,
    std::vector<std::vector<size_t>> const& final_groups,
    size_t case_number) {
    Tableau out(n_qubits);
    out.erase(out.begin(), out.end());
    out.push_back(SubTableau{front_clifford});

    size_t block_number = 0;
    for (auto const& indices : final_groups) {
        bool const commuting_pack = indices.size() > 1 && ncf_pairwise_commutative(all_rotations, indices);
        auto emit_case_block      = [&](std::vector<size_t> const& block_indices, std::string const& label_suffix) {
            std::vector<PauliRotation> group_rotations{};
            group_rotations.reserve(block_indices.size());
            for (size_t idx : block_indices) {
                group_rotations.push_back(all_rotations[idx]);
            }
            auto ops_opt = ncf_try_build_conjugation(group_rotations);
            if (!ops_opt) {
                return;
            }
            auto const& ops = *ops_opt;

            auto const label_orig = [&]() {
                std::string s;
                for (size_t k = 0; k < block_indices.size(); ++k) {
                    if (k > 0) s += ", ";
                    s += "#" + std::to_string(block_indices[k]);
                }
                return s;
            }();

            StabilizerTableau c_dagger(n_qubits);
            c_dagger.apply(adjoint(ops));
            out.push_back(SubTableau{std::move(c_dagger)});
            out.set_block_ops(out.size() - 1, adjoint(ops));

            out.push_back(SubTableau{std::move(group_rotations)});
            out.set_block_label(
                out.size() - 1,
                "NCF case " + std::to_string(case_number) + " block " + std::to_string(block_number) + label_suffix + " (original " + label_orig + ")");

            StabilizerTableau c_tableau(n_qubits);
            c_tableau.apply(ops);
            out.push_back(SubTableau{std::move(c_tableau)});
            out.set_block_ops(out.size() - 1, ops);
            ++block_number;
        };

        if (commuting_pack) {
            for (size_t idx : indices) emit_case_block({idx}, " commuting-pack");
        } else {
            emit_case_block(indices, "");
        }
    }
    return out;
}

}  // namespace

std::vector<Tableau> ncf_fusion_all(Tableau const& input_tableau, NcfFusionOptions const& options) {
    auto tableau = input_tableau;
    std::vector<Tableau> result{};

    if (tableau.is_empty()) return result;
    collapse(tableau);
    if (tableau.size() < 2) {
        result.push_back(tableau);
        return result;
    }

    DVLAB_ASSERT(std::holds_alternative<StabilizerTableau>(tableau.front()), "after collapse front is Clifford");
    DVLAB_ASSERT(std::holds_alternative<std::vector<PauliRotation>>(tableau.back()), "after collapse back is rotations");

    size_t const n_qubits = tableau.n_qubits();
    auto const& front_clifford = std::get<StabilizerTableau>(tableau.front());
    auto const& all_rotations = std::get<std::vector<PauliRotation>>(tableau.back());
    if (all_rotations.empty()) {
        result.push_back(tableau);
        return result;
    }

    auto find_unpartitioned_product = [&](std::vector<bool> const& used, size_t i, size_t j) -> std::optional<size_t> {
        PauliProduct p = all_rotations[i].pauli_product() * all_rotations[j].pauli_product();
        auto match = [&](PauliProduct const& pp) -> std::optional<size_t> {
            for (size_t k = 0; k < all_rotations.size(); ++k) {
                if (used[k] || k == i || k == j) continue;
                if (all_rotations[k].pauli_product() == pp) return k;
            }
            return std::nullopt;
        };
        if (auto k = match(p)) return k;
        p.negate();
        return match(p);
    };

    auto partition_key = [](std::vector<std::vector<size_t>> groups) {
        for (auto& g : groups) std::ranges::sort(g);
        std::ranges::sort(groups);
        std::string key{};
        for (auto const& g : groups) {
            key += "[";
            for (size_t i = 0; i < g.size(); ++i) {
                if (i > 0) key += ",";
                key += std::to_string(g[i]);
            }
            key += "]";
        }
        return key;
    };

    std::vector<std::vector<std::vector<size_t>>> base_partitions{};
    std::unordered_set<std::string> seen_base{};
    std::function<void(std::vector<bool>&, std::vector<std::vector<size_t>>&)> dfs =
        [&](std::vector<bool>& used, std::vector<std::vector<size_t>>& groups) {
            std::vector<size_t> active{};
            for (size_t i = 0; i < all_rotations.size(); ++i) {
                if (!used[i]) active.push_back(i);
            }
            if (active.empty()) {
                auto key = partition_key(groups);
                if (!seen_base.contains(key)) {
                    seen_base.insert(key);
                    base_partitions.push_back(groups);
                }
                return;
            }

            std::vector<std::pair<size_t, size_t>> anti_pairs{};
            for (size_t a = 0; a < active.size(); ++a) {
                for (size_t b = a + 1; b < active.size(); ++b) {
                    size_t const i = active[a];
                    size_t const j = active[b];
                    if (!all_rotations[i].is_commutative(all_rotations[j])) {
                        anti_pairs.emplace_back(i, j);
                    }
                }
            }

            if (anti_pairs.empty()) {
                groups.push_back(active);
                auto key = partition_key(groups);
                if (!seen_base.contains(key)) {
                    seen_base.insert(key);
                    base_partitions.push_back(groups);
                }
                groups.pop_back();
                return;
            }

            if (options.overlap_priority) {
                std::ranges::sort(anti_pairs, [&](auto const& lhs, auto const& rhs) {
                    auto const& [li, lj] = lhs;
                    auto const& [ri, rj] = rhs;
                    size_t const lov =
                        paulihedral_overlap(all_rotations[li].pauli_product(), all_rotations[lj].pauli_product());
                    size_t const rov =
                        paulihedral_overlap(all_rotations[ri].pauli_product(), all_rotations[rj].pauli_product());
                    if (lov != rov) return lov > rov;
                    size_t const lchain = paulihedral_chain_score(all_rotations[li].pauli_product(),
                                                                  all_rotations[lj].pauli_product(), all_rotations,
                                                                  active, li, lj);
                    size_t const rchain = paulihedral_chain_score(all_rotations[ri].pauli_product(),
                                                                  all_rotations[rj].pauli_product(), all_rotations,
                                                                  active, ri, rj);
                    if (lchain != rchain) return lchain > rchain;
                    return lhs < rhs;
                });
            }

            for (auto const& [i, j] : anti_pairs) {
                std::vector<size_t> block{i, j};
                if (auto k = find_unpartitioned_product(used, i, j)) block.push_back(*k);
                std::ranges::sort(block);

                for (auto idx : block) used[idx] = true;
                groups.push_back(block);
                dfs(used, groups);
                groups.pop_back();
                for (auto idx : block) used[idx] = false;
            }
        };

    std::vector<bool> used(all_rotations.size(), false);
    std::vector<std::vector<size_t>> current_groups{};
    dfs(used, current_groups);

    size_t case_number = 0;
    for (auto const& base_groups : base_partitions) {
        std::vector<std::vector<std::vector<std::vector<size_t>>>> per_group_choices{};
        per_group_choices.reserve(base_groups.size());
        for (auto const& g : base_groups) {
            per_group_choices.push_back(ncf_group_merge_choices(g));
        }

        std::vector<size_t> choice_idx(per_group_choices.size(), 0);
        bool done = per_group_choices.empty();
        while (!done) {
            std::vector<std::vector<size_t>> final_groups{};
            bool valid_case = true;
            for (size_t gi = 0; gi < per_group_choices.size(); ++gi) {
                auto const& choice = per_group_choices[gi][choice_idx[gi]];
                for (auto const& block : choice) {
                    if (block.size() > 1 && ncf_pairwise_commutative(all_rotations, block)) {
                        for (size_t idx : block) {
                            std::vector<PauliRotation> probe{all_rotations[idx]};
                            auto probe_copy = probe;
                            if (!ncf_try_build_conjugation(probe_copy)) {
                                valid_case = false;
                                break;
                            }
                        }
                        if (!valid_case) break;
                        final_groups.push_back(block);
                        continue;
                    }
                    std::vector<PauliRotation> probe{};
                    for (size_t idx : block) probe.push_back(all_rotations[idx]);
                    auto probe_copy = probe;
                    if (!ncf_try_build_conjugation(probe_copy)) {
                        valid_case = false;
                        break;
                    }
                    final_groups.push_back(block);
                }
                if (!valid_case) break;
            }
            if (valid_case) {
                result.push_back(ncf_build_tableau_from_groups(n_qubits, front_clifford, all_rotations, final_groups, case_number));
                ++case_number;
                if (options.max_cases > 0 && result.size() >= options.max_cases) break;
            }

            for (size_t i = choice_idx.size(); i-- > 0;) {
                ++choice_idx[i];
                if (choice_idx[i] < per_group_choices[i].size()) break;
                choice_idx[i] = 0;
                if (i == 0) {
                    done = true;
                }
            }
        }
        if (options.max_cases > 0 && result.size() >= options.max_cases) break;
    }

    if (result.empty()) {
        auto fallback = tableau;
        ncf_fusion(fallback, options);
        result.push_back(std::move(fallback));
    }
    spdlog::info("NCF enumerate: generated {} candidate merge cases.", result.size());
    return result;
}

}  // namespace experimental

}  // namespace qsyn
