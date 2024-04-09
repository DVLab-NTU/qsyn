/**
 * @file
 * @brief implementation of the internal-H-opt optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "../tableau_optimization.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"

namespace qsyn::experimental {

namespace {

void apply_clifford(Tableau& tableau, CliffordOperatorString const& clifford, size_t n_qubits) {
    // if the stabilizer is not commutative with the clifford, we need to add the clifford to the tableau first
    if (clifford.empty()) {
        return;
    }

    if (tableau.size() == 0) {
        auto st = StabilizerTableau{n_qubits}.apply(clifford);
        tableau.push_back(std::move(st));
        return;
    }

    dvlab::match(
        tableau.back(),
        [&](StabilizerTableau& subtableau) {
            subtableau.apply(clifford);
        },
        [&](std::vector<PauliRotation>& subtableau) {
            auto st = StabilizerTableau{n_qubits}.apply(clifford);
            if (!std::ranges::all_of(subtableau, [&](PauliRotation const& rotation) { return st.is_commutative(rotation.pauli_product()); })) {
                tableau.push_back(std::move(st));
                return;
            }

            if (tableau.size() > 1 && std::holds_alternative<StabilizerTableau>(tableau[tableau.size() - 2])) {
                // if the second-to-last element is a clifford, we can merge the clifford into it
                std::get<StabilizerTableau>(tableau[tableau.size() - 2]).apply(clifford);
                return;
            }

            tableau.insert(tableau.end() - 1, std::move(st));
            return;
        });
}

void implement_into_tableau(Tableau& tableau, StabilizerTableau& context, size_t qubit, dvlab::Phase const& phase) {
    auto const qubit_range = std::views::iota(0ul, context.n_qubits());
    CliffordOperatorString clifford{};

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
                clifford.emplace_back(CliffordOperatorType::cx, std::array{ctrl, targ});
            }
        }

        if (stabilizer.get().is_z_set(ctrl)) {
            context.s(ctrl);
            clifford.emplace_back(CliffordOperatorType::s, std::array{ctrl, 0ul});
        }

        context.h(ctrl);
        clifford.emplace_back(CliffordOperatorType::h, std::array{ctrl, 0ul});
    }

    apply_clifford(tableau, clifford, context.n_qubits());

    dvlab::match(
        tableau.back(),
        [&](StabilizerTableau& /* subtableau */) {
            tableau.push_back(std::vector{PauliRotation{stabilizer, phase}});
        },
        [&](std::vector<PauliRotation>& subtableau) {
            subtableau.push_back(PauliRotation{stabilizer, phase});
        });
}

}  // namespace

std::pair<Tableau, StabilizerTableau> minimize_hadamards(Tableau tableau, StabilizerTableau context) {
    if (tableau.is_empty()) {
        return {Tableau{context.n_qubits()}, context};
    }
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

void minimize_internal_hadamards(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }
    collapse(tableau);

    auto context        = StabilizerTableau{tableau.n_qubits()};
    auto final_clifford = StabilizerTableau{tableau.n_qubits()};

    std::ranges::for_each(
        extract_clifford_operators(std::get<StabilizerTableau>(tableau.front())),
        [&context](CliffordOperator const& op) {
            context.prepend(adjoint(op));
        });

    auto [_, initial_clifford]        = minimize_hadamards(Tableau{adjoint(tableau.front())}, context);
    std::tie(tableau, final_clifford) = minimize_hadamards(tableau, initial_clifford);

    tableau.insert(tableau.begin(), initial_clifford);
    tableau.push_back(adjoint(final_clifford));

    remove_identities(tableau);
}

}  // namespace qsyn::experimental
