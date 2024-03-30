/**
 * @file
 * @brief implementation of the internal-H-opt optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "../tableau_optimization.hpp"

namespace qsyn::experimental {

namespace {

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

}  // namespace

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

}  // namespace qsyn::experimental
