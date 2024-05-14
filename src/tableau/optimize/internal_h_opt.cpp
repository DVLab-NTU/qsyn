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
    if (clifford.empty()) {
        return;
    }

    if (tableau.is_empty()) {
        tableau.push_back(StabilizerTableau{n_qubits}.apply(clifford));
        return;
    }

    dvlab::match(
        tableau.back(),
        [&](StabilizerTableau& subtableau) {
            subtableau.apply(clifford);
        },
        [&](std::vector<PauliRotation>& subtableau) {
            // check if the clifford can be inserted before the rotations

            // suppose the Pauli rotation is R_P(θ), and the clifford is C, then we have
            // C R_P(θ) = R_P(θ) C if and only if CPC^† = P
            auto copy_rotations = subtableau;
            for (auto& rotation : copy_rotations) {
                rotation.apply(clifford);
            }

            // Case I: some rotations does not commute with the clifford
            if (copy_rotations != subtableau) {
                tableau.push_back(StabilizerTableau{n_qubits}.apply(clifford));
                return;
            }

            // Case II: all rotations commute with the clifford, and the second-to-last element is a clifford
            if (tableau.size() > 1 && std::holds_alternative<StabilizerTableau>(tableau[tableau.size() - 2])) {
                std::get<StabilizerTableau>(tableau[tableau.size() - 2]).apply(clifford);
                return;
            }

            // Case III: all rotations commute with the clifford, and the second-to-last element is a list of rotations
            assert(tableau.size() > 0);
            tableau.insert(std::prev(tableau.end()), StabilizerTableau{n_qubits}.apply(clifford));
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
