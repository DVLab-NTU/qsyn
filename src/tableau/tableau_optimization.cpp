/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <fmt/core.h>
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

/**
 * @brief Partition rotations into single-qubit NCF groups: anticommuting pairs
 *        (and their product if present), then remaining as singletons.
 */
std::vector<std::vector<size_t>> ncf_partition_single_qubit_groups(std::vector<PauliRotation> const& rotations) {
    size_t const n = rotations.size();
    std::vector<std::vector<size_t>> groups;
    std::unordered_set<size_t> used;

    auto const product_matches = [&](size_t i, size_t j, size_t& out_k) -> bool {
        PauliProduct p = rotations[i].pauli_product() * rotations[j].pauli_product();
        for (size_t k = 0; k < n; ++k) {
            if (k == i || k == j) continue;
            if (p == rotations[k].pauli_product()) {
                out_k = k;
                return true;
            }
            p.negate();
            if (p == rotations[k].pauli_product()) {
                out_k = k;
                return true;
            }
            p.negate();
        }
        return false;
    };

    for (size_t i = 0; i < n; ++i) {
        if (used.contains(i)) continue;
        bool grouped = false;
        for (size_t j = i + 1; j < n; ++j) {
            if (used.contains(j)) continue;
            if (!rotations[i].is_commutative(rotations[j])) {
                size_t k = 0;
                if (product_matches(i, j, k) && !used.contains(k)) {
                    groups.push_back({i, j, k});
                    used.insert(i);
                    used.insert(j);
                    used.insert(k);
                } else {
                    groups.push_back({i, j});
                    used.insert(i);
                    used.insert(j);
                }
                grouped = true;
                break;
            }
        }
        if (!grouped) {
            groups.push_back({i});
            used.insert(i);
        }
    }
    return groups;
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

void ncf_fusion(Tableau& tableau) {
    if (tableau.is_empty()) return;
    collapse(tableau);
    if (tableau.size() < 2) return;

    DVLAB_ASSERT(std::holds_alternative<StabilizerTableau>(tableau.front()), "after collapse front is Clifford");
    DVLAB_ASSERT(std::holds_alternative<std::vector<PauliRotation>>(tableau.back()), "after collapse back is rotations");

    auto const n_qubits = tableau.n_qubits();
    StabilizerTableau front_clifford = std::get<StabilizerTableau>(tableau.front());
    std::vector<PauliRotation> all_rotations = std::get<std::vector<PauliRotation>>(tableau.back());

    if (all_rotations.empty()) return;

    std::vector<std::vector<size_t>> const groups = ncf_partition_single_qubit_groups(all_rotations);

    Tableau new_tableau(n_qubits);
    new_tableau.erase(new_tableau.begin(), new_tableau.end());
    new_tableau.push_back(SubTableau{std::move(front_clifford)});

    size_t group_number = 0;
    for (auto const& indices : groups) {
        auto const label_orig = [&]() {
            std::string s;
            for (size_t k = 0; k < indices.size(); ++k) {
                if (k > 0) s += ", ";
                s += "#" + std::to_string(indices[k]);
            }
            return s;
        }();

        std::vector<PauliRotation> group_rotations;
        group_rotations.reserve(indices.size());
        for (size_t idx : indices) {
            group_rotations.push_back(all_rotations[idx]);
        }

        // Even for singleton groups, wrap them as [C†][R’][C] so that -r ncf can always emit RZ
        // on one qubit (instead of falling back to naive synthesis, which emits p(...) / z).
        CliffordOperatorString ops = (indices.size() == 1) ? ncf_build_single_rotation_conjugation(group_rotations)
                                                         : ncf_build_single_qubit_conjugation(group_rotations);

        StabilizerTableau c_dagger(n_qubits);
        c_dagger.apply(adjoint(ops));
        new_tableau.push_back(SubTableau{std::move(c_dagger)});
        new_tableau.set_block_ops(new_tableau.size() - 1, adjoint(ops));

        new_tableau.push_back(SubTableau{std::move(group_rotations)});
        new_tableau.set_block_label(new_tableau.size() - 1, "NCF group " + std::to_string(group_number) + " (original " + label_orig + ")");

        StabilizerTableau c_tableau(n_qubits);
        c_tableau.apply(ops);
        new_tableau.push_back(SubTableau{std::move(c_tableau)});
        new_tableau.set_block_ops(new_tableau.size() - 1, ops);

        ++group_number;
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
        std::vector<PauliRotation> group_rotations{};
        group_rotations.reserve(indices.size());
        for (size_t idx : indices) {
            group_rotations.push_back(all_rotations[idx]);
        }
        auto ops_opt = ncf_try_build_conjugation(group_rotations);
        if (!ops_opt) {
            continue;
        }
        auto const& ops = *ops_opt;

        auto const label_orig = [&]() {
            std::string s;
            for (size_t k = 0; k < indices.size(); ++k) {
                if (k > 0) s += ", ";
                s += "#" + std::to_string(indices[k]);
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
            "NCF case " + std::to_string(case_number) + " block " + std::to_string(block_number) + " (original " + label_orig + ")");

        StabilizerTableau c_tableau(n_qubits);
        c_tableau.apply(ops);
        out.push_back(SubTableau{std::move(c_tableau)});
        out.set_block_ops(out.size() - 1, ops);
        ++block_number;
    }
    return out;
}

}  // namespace

std::vector<Tableau> ncf_fusion_all(Tableau const& input_tableau, size_t max_cases) {
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

    auto const base_groups = ncf_partition_single_qubit_groups(all_rotations);
    std::vector<std::vector<std::vector<std::vector<size_t>>>> per_group_choices{};
    per_group_choices.reserve(base_groups.size());
    for (auto const& g : base_groups) {
        per_group_choices.push_back(ncf_group_merge_choices(g));
    }

    std::vector<size_t> choice_idx(per_group_choices.size(), 0);
    bool done = per_group_choices.empty();
    size_t case_number = 0;
    while (!done) {
        std::vector<std::vector<size_t>> final_groups{};
        bool valid_case = true;
        for (size_t gi = 0; gi < per_group_choices.size(); ++gi) {
            auto const& choice = per_group_choices[gi][choice_idx[gi]];
            for (auto const& block : choice) {
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
            if (max_cases > 0 && result.size() >= max_cases) break;
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

    if (result.empty()) {
        auto fallback = tableau;
        ncf_fusion(fallback);
        result.push_back(std::move(fallback));
    }
    spdlog::info("NCF enumerate: generated {} candidate merge cases.", result.size());
    return result;
}

}  // namespace experimental

}  // namespace qsyn
