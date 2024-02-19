/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <bits/ranges_algo.h>
#include <fmt/core.h>

#include <functional>
#include <gsl/narrow>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"

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
    // ensures that all tableaux have the same number of qubits
    size_t const n_qubits = tableau.n_qubits();

    assert(std::ranges::all_of(tableau, [n_qubits](SubTableau const& sub_tableau) {
        return sub_tableau.clifford.n_qubits() == n_qubits &&
               std::ranges::all_of(sub_tableau.pauli_rotations, [n_qubits](PauliRotation const& rotation) {
                   return rotation.n_qubits() == n_qubits;
               });
    }));

    if (tableau.size() <= 1) return;

    // make all clifford operators to be the identity except the first one
    for (auto&& [next_tabl, this_tabl] : tl::views::adjacent<2>(tableau | std::views::reverse)) {
        auto& next_clifford = next_tabl.clifford;

        this_tabl.apply(extract_clifford_operators(next_clifford));

        next_clifford = StabilizerTableau{n_qubits};
    }

    // merge all rotations into the first tableau
    for (auto& [_, pauli_rotations] : tableau | std::views::drop(1)) {
        tableau.front().pauli_rotations.insert(
            tableau.front().pauli_rotations.end(),
            pauli_rotations.begin(),
            pauli_rotations.end());
    }

    tableau.erase(std::next(tableau.begin()), tableau.end());
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
        remove_identities(subtableau.pauli_rotations);
    });

    // remove redundant clifford operators and merge adjacent pauli rotations if possible
    for (auto const& [this_tabl, next_tabl] : tl::views::adjacent<2>(tableau)) {
        if (this_tabl.pauli_rotations.empty()) {
            this_tabl.clifford.apply(extract_clifford_operators(next_tabl.clifford));
            next_tabl.clifford = StabilizerTableau{this_tabl.clifford.n_qubits()};
        }
        if (next_tabl.clifford.is_identity()) {
            this_tabl.pauli_rotations.insert(
                this_tabl.pauli_rotations.end(),
                next_tabl.pauli_rotations.begin(),
                next_tabl.pauli_rotations.end());
            next_tabl.pauli_rotations.clear();
        }
    }

    tableau.erase(
        std::remove_if(
            tableau.begin(),
            tableau.end(),
            [](SubTableau const& subtableau) {
                return subtableau.clifford.is_identity() && subtableau.pauli_rotations.empty();
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

    assert(tableau.size() == 1);

    auto& clifford  = tableau.front().clifford;
    auto& rotations = tableau.front().pauli_rotations;

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

SubTableau implement_subtableau(StabilizerTableau& context, size_t qubit, dvlab::Phase const& phase) {
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

    return {clifford, {PauliRotation{stabilizer, phase}}};
}

std::pair<Tableau, StabilizerTableau> minimize_hadamards(Tableau tableau, StabilizerTableau context) {
    collapse(tableau);
    assert(tableau.size() == 1);

    auto const& [initial_clifford, rotations] = tableau.front();

    std::ranges::for_each(
        extract_clifford_operators(initial_clifford),
        [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

    auto new_tableau = Tableau{};
    for (auto const& rotation : rotations) {
        auto const [ops, qubit] = extract_clifford_operators(rotation);

        std::ranges::for_each(ops, [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

        auto const subtableau = implement_subtableau(context, qubit, rotation.phase());

        if (subtableau.clifford.is_identity() && new_tableau.size()) {
            new_tableau.back().pauli_rotations.push_back(subtableau.pauli_rotations.front());
        } else {
            new_tableau.push_back(subtableau);
        }

        std::ranges::for_each(adjoint(ops), [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });
    }

    return {new_tableau, context};
}

Tableau minimize_internal_hadamards(Tableau tableau) {
    collapse(tableau);
    assert(tableau.size() == 1);

    auto context = StabilizerTableau{tableau.front().clifford.n_qubits()};

    std::ranges::for_each(
        extract_clifford_operators(tableau.front().clifford),
        [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

    auto [_, initial_clifford]         = minimize_hadamards(Tableau{adjoint(tableau.front())}, context);
    auto [out_tableau, final_clifford] = minimize_hadamards(tableau, initial_clifford);

    for (auto const& [clifford, rotations] : out_tableau) {
        auto h_count = std::ranges::count_if(extract_clifford_operators(clifford), [](CliffordOperator const& op) { return op.first == CliffordOperatorType::h; });
        assert(h_count <= 1);
        assert(std::ranges::all_of(rotations, [](PauliRotation const& rotation) { return rotation.is_diagonal(); }));
    }

    out_tableau.insert(out_tableau.begin(), {initial_clifford, {}});
    out_tableau.push_back({adjoint(final_clifford), {}});

    remove_identities(out_tableau);

    out_tableau.set_filename(tableau.get_filename());
    out_tableau.add_procedures(tableau.get_procedures());

    return out_tableau;
}

}  // namespace experimental

}  // namespace qsyn
