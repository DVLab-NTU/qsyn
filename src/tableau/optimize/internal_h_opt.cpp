/**
 * @file
 * @brief implementation of the internal-H-opt optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "../tableau_optimization.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"
#include <array>
#include <spdlog/spdlog.h>

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

void implement_into_tableau_n_gadgetize(Tableau& tableau, StabilizerTableau& context, size_t qubit, dvlab::Phase const& phase) {
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

        // H Gadget Implementation:
        // Instead of applying H gate directly, replace it with a gadget
        // The H gadget uses an ancilla qubit and measurement to eliminate the H gate        
        // Step 1: Add ancilla qubit to the tableau

        size_t ancilla_qubit = tableau.add_ancilla_qubit(AncillaInitialState::PLUS);

        context.add_ancilla_qubit();
        stabilizer = std::ref(context.stabilizer(qubit));
        context.h(ctrl);

        // Step 3: Apply controlled operation (this replaces the H gate effect)
        // The H gadget circuit: S 。     X 。
        //                        S X Sdg 。 X

        clifford.emplace_back(CliffordOperatorType::s, std::array{ancilla_qubit, 0ul});
        clifford.emplace_back(CliffordOperatorType::s, std::array{ctrl, 0ul});

        clifford.emplace_back(CliffordOperatorType::cx, std::array{ctrl, ancilla_qubit});
        clifford.emplace_back(CliffordOperatorType::sdg, std::array{ancilla_qubit, 0ul});
       
        clifford.emplace_back(CliffordOperatorType::cx, std::array{ancilla_qubit, ctrl});
        clifford.emplace_back(CliffordOperatorType::cx, std::array{ctrl, ancilla_qubit});

        

        
        // Step 4: Mark ancilla as dirty since it's been used
        tableau.mark_ancilla_dirty(ancilla_qubit);
        
        // Step 5: Add measurement from ancilla qubit to classical bit
        tableau.add_measurement(ancilla_qubit, ancilla_qubit);
        
        // Step 6: Add if-else operation: if(c[ancilla_qubit]==1) x ctrl
        tableau.add_if_else_operation(ancilla_qubit, 1, "X", {ctrl});
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

void minimize_internal_hadamards_n_gadgetize(Tableau& tableau) {

    minimize_internal_hadamards(tableau);
    auto new_tableau = Tableau{tableau.n_qubits()};

    // Process first subtableau from tableau until it is empty
    while (!tableau.is_empty()) {
        auto subtableau = std::move(tableau.front());
        tableau.erase(tableau.begin());
        
        dvlab::match(
            subtableau,
            [&](StabilizerTableau const& st) {
                // Check if there are any PauliRotation tableaux in new_tableau
                bool has_pauli_rotations = std::ranges::any_of(new_tableau, [](auto const& subtableau) {
                    return std::holds_alternative<std::vector<PauliRotation>>(subtableau);
                });

                bool remain_pauli_rotations = std::ranges::any_of(tableau, [](auto const& subtableau) {
                    return std::holds_alternative<std::vector<PauliRotation>>(subtableau);
                });
                
                if (!has_pauli_rotations || !remain_pauli_rotations) {
                    // If no PauliRotation tableaux in new_tableau, add directly
                    new_tableau.push_back(st);
                } else {
                    // If new_tableau is not empty, extract clifford operations
                    auto clifford_ops = extract_clifford_operators(st);                    
                    // Apply clifford operations one by one to the tableau
                    auto stabilizer = StabilizerTableau{new_tableau.n_qubits()};
                    for (auto const& op : clifford_ops) {
                        if (op.first == CliffordOperatorType::h) {
                            // Handle H gates with gadgetization
                            size_t qubit = op.second[0];
                            // Add ancilla and apply gadgetization
                            size_t ancilla_qubit = new_tableau.add_ancilla_qubit(AncillaInitialState::PLUS);
                            stabilizer.add_ancilla_qubit();
                            tableau.add_ancilla_qubit();
                            // Apply H gadget: S 。     X 。
                            //                S X Sdg 。 X
                            // Apply S gates
                            
                            stabilizer.s(ancilla_qubit);
                            stabilizer.s(qubit);
                            stabilizer.cx(qubit, ancilla_qubit);
                            stabilizer.sdg(ancilla_qubit);
                            stabilizer.cx(ancilla_qubit, qubit);
                            stabilizer.cx(qubit, ancilla_qubit);
                            
                            // Mark ancilla as dirty and add measurement
                            new_tableau.mark_ancilla_dirty(ancilla_qubit);
                            new_tableau.add_measurement(ancilla_qubit, ancilla_qubit);
                            new_tableau.add_if_else_operation(ancilla_qubit, 1, "X", {qubit});
                        } else {
                            // Apply non-H operations directly to tableau
                            stabilizer.apply(op);
                        }
                    }
                    new_tableau.push_back(stabilizer);
                }
            },
            [&](std::vector<PauliRotation> const& rotations) {
                // When see a rotation tableau, also append it
                new_tableau.push_back(rotations);
            });
    }

    remove_identities(new_tableau);
    // Replace the original tableau with the new one
    tableau = std::move(new_tableau);
    spdlog::info("new_tableau: {:b}", tableau);
    collapse(tableau);

}

}  // namespace qsyn::experimental
