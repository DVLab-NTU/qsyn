/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <fmt/core.h>

#include <functional>
#include <gsl/narrow>
#include <ranges>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <variant>
#include <vector>

#include "tableau/tableau.hpp"

namespace qsyn {

namespace experimental {

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
        size_t upto) : clifford{clifford}, rotations{rotations}, upto{upto} {}

    ConjugationView& h(size_t qubit) override {
        clifford.get().h(qubit);
        for (size_t i = 0; i < upto; ++i) {
            rotations.get()[i].h(qubit);
        }
        return *this;
    }

    ConjugationView& s(size_t qubit) override {
        clifford.get().s(qubit);
        for (size_t i = 0; i < upto; ++i) {
            rotations.get()[i].s(qubit);
        }
        return *this;
    }

    ConjugationView& cx(size_t control, size_t target) override {
        clifford.get().cx(control, target);
        for (size_t i = 0; i < upto; ++i) {
            rotations.get()[i].cx(control, target);
        }
        return *this;
    }

private:
    std::reference_wrapper<StabilizerTableau> clifford;
    std::reference_wrapper<std::vector<PauliRotation>> rotations;
    size_t upto;
};

}  // namespace

/**
 * @brief Pushing all the Clifford operatos to the first sub-tableau and merging all the Pauli rotations.
 *
 * @param tableau
 */
void collapse(Tableau& tableau) {
    size_t const n_qubits = tableau.n_qubits();

    if (tableau.size() <= 1) return;

    // prepend a stabilizer tableau to the front if the first sub-tableau is a list of PauliRotations
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{n_qubits});
    }

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
                return std::visit(
                    dvlab::overloaded{
                        [](const StabilizerTableau& subtableau) { return subtableau.is_identity(); },
                        [](const std::vector<PauliRotation>& subtableau) { return subtableau.empty(); }},
                    subtableau);
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
    assert(std::ranges::all_of(rotations, [&rotations](PauliRotation const& rotation) { return rotation.n_qubits() == rotations.front().n_qubits(); }));

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
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *        If a rotation becomes Clifford, absorb it into the initial Clifford operator.
 *        This algorithm is inspired by the paper [[1903.12456] Optimizing T gates in Clifford+T circuit as $π/4$ rotations around Paulis](https://arxiv.org/abs/1903.12456)
 *
 * @param clifford
 * @param rotations
 */
void merge_rotations(Tableau& tableau) {
    collapse(tableau);

    if (tableau.size() == 1) {
        return;
    }

    auto& clifford  = std::get<StabilizerTableau>(tableau.front());
    auto& rotations = std::get<std::vector<PauliRotation>>(tableau.back());

    merge_rotations(rotations);

    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].phase() != dvlab::Phase(1, 2) &&
            rotations[i].phase() != dvlab::Phase(-1, 2) &&
            rotations[i].phase() != dvlab::Phase(1)) continue;

        ConjugationView conjugation_view{clifford, rotations, i};

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

void implement_into_tableau(Tableau& tableau, StabilizerTableau& context, size_t qubit, dvlab::Phase const& phase) {
    auto const qubit_range = std::views::iota(0ul, context.n_qubits());
    StabilizerTableau clifford{context.n_qubits()};

    auto stabilizer = std::ref(context.stabilizer(qubit));

    auto ctrl = gsl::narrow<size_t>(
        std::ranges::distance(
            qubit_range.begin(),
            std::ranges::find_if(qubit_range, [&stabilizer](size_t i) {
                return stabilizer.get().is_x_set(i);
            })));

    if (ctrl < context.n_qubits()) {
        for (size_t targ = ctrl + 1; targ < context.n_qubits(); ++targ) {
            if (stabilizer.get().is_x_set(targ)) {
                context.cx(ctrl, targ);
                clifford.cx(ctrl, targ);
            }
        }

        if (stabilizer.get().is_z_set(ctrl)) {
            context.s(ctrl);
            clifford.s(ctrl);
        }

        context.h(ctrl);
        clifford.h(ctrl);
    }

    if (!clifford.is_identity()) {
        tableau.push_back(clifford);
    }

    std::visit(
        dvlab::overloaded(
            [&](StabilizerTableau& /* unused */) {
                tableau.push_back(std::vector{PauliRotation{stabilizer, phase}});
            },
            [&](std::vector<PauliRotation>& subtableau) {
                subtableau.push_back(PauliRotation{stabilizer, phase});
            }),
        tableau.back());
}

std::pair<Tableau, StabilizerTableau> minimize_hadamards(Tableau tableau, StabilizerTableau context) {
    collapse(tableau);

    auto const& initial_clifford = std::get<StabilizerTableau>(tableau.front());
    std::ranges::for_each(
        extract_clifford_operators(initial_clifford),
        [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

    if (tableau.size() == 1) {
        return {Tableau{context.n_qubits()}, context};
    }

    auto const& rotations = std::get<std::vector<PauliRotation>>(tableau.back());

    auto new_tableau = Tableau{context.n_qubits()};
    for (auto const& rotation : rotations) {
        auto const [ops, qubit] = extract_clifford_operators(rotation);

        std::ranges::for_each(ops, [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

        implement_into_tableau(new_tableau, context, qubit, rotation.phase());

        std::ranges::for_each(adjoint(ops), [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });
    }

    return {new_tableau, context};
}

Tableau minimize_internal_hadamards(Tableau tableau) {
    collapse(tableau);

    auto context = StabilizerTableau{tableau.n_qubits()};

    std::ranges::for_each(
        extract_clifford_operators(std::get<StabilizerTableau>(tableau.front())),
        [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

    auto [_, initial_clifford]         = minimize_hadamards(Tableau{adjoint(tableau.front())}, context);
    auto [out_tableau, final_clifford] = minimize_hadamards(tableau, initial_clifford);

    out_tableau.insert(out_tableau.begin(), initial_clifford);
    out_tableau.push_back(adjoint(final_clifford));

    remove_identities(out_tableau);

    out_tableau.set_filename(tableau.get_filename());
    out_tableau.add_procedures(tableau.get_procedures());

    return out_tableau;
}

// matroid partitioning

/**
 * @brief split the phase polynomial into matroids. The matroids are represented by a list of PauliRotations, which must be all-diagonal.
 *
 * @param polynomial
 * @return std::vector<std::vector<PauliRotation>>
 */
std::vector<std::vector<PauliRotation>> matroid_partition(std::vector<PauliRotation> const& polynomial, size_t num_ancillae, MatroidPartitionStrategy const& strategy) {
    DVLAB_ASSERT(is_phase_polynomial(polynomial), "The input pauli rotations a phase polynomial.");
    return strategy.partition(polynomial, num_ancillae);
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

    // equivalent to the independence oracle lemma:
    //     dim(V) - rank(S) <= n - |S|
    // in the literature, where n is the number of qubits = polynomial dimension + num_ancillae
    // ref: [Polynomial-time T-depth Optimization of Clifford+T circuits via Matroid Partitioning](https://arxiv.org/pdf/1303.2042.pdf)
    return polynomial.size() - matrix_rank(polynomial) <= num_ancillae;
};

MatroidPartitionStrategy::Partitions NaiveMatroidPartitionStrategy::partition(MatroidPartitionStrategy::Polynomial const& polynomial, size_t num_ancillae) const {
    auto matroids = std::vector(1, std::vector<PauliRotation>{});  // starts with an empty matroid

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

}  // namespace experimental

}  // namespace qsyn
