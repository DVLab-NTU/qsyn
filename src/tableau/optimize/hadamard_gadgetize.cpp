/**
 * @file hadamard_gadgetize.cpp
 * @brief Implementation of Hadamard gate gadgetization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "../tableau_optimization.hpp"
#include "../classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"
#include <spdlog/spdlog.h>

namespace qsyn::experimental {

namespace {

/**
 * @brief Add ancilla qubit to all subtableaux in a vector
 */
void add_ancilla_to_subtableaux(std::vector<SubTableau>& subtableaux) {
    for (auto& subtableau : subtableaux) {
        std::visit(
            dvlab::overloaded{
                [](StabilizerTableau& st) {
                    st.add_ancilla_qubit();
                },
                [](std::vector<PauliRotation>& pr) {
                    for (auto& rotation : pr) {
                        rotation.add_ancilla_qubit();
                    }
                },
                [](ClassicalControlTableau& cct) {
                    cct.add_ancilla_qubit();
                }},
            subtableau);
    }
}

/**
 * @brief Gadgetize a single Hadamard gate by modifying the StabilizerTableau in-place.
 * Adds an ancilla qubit to the stabilizer tableau and applies gadget operations.
 * Returns a ClassicalControlTableau for the conditional X gate.
 *
 * @param st The StabilizerTableau to modify (will have ancilla added and gates applied)
 * @param qubit The qubit index where the H gate originally was
 * @return ClassicalControlTableau The classical control tableau for the conditional X gate
 */
std::pair<StabilizerTableau, ClassicalControlTableau> gadgetize_hadamard(StabilizerTableau& st, size_t qubit) {
    size_t n_qubits = st.n_qubits();
    
    // Step 1: Create CCT of the same size as the stabilizer tableau
    // The control qubit will be the ancilla (at index n_qubits after adding)
    size_t ancilla_index = n_qubits - 1;
    ClassicalControlTableau cct(ancilla_index, n_qubits);
    
    
    // Step 3: Apply gadget operations to the stabilizer tableau
    st.s(ancilla_index);
    st.s(qubit);
    st.cx(qubit, ancilla_index);
    st.sdg(ancilla_index);
    st.cx(ancilla_index, qubit);
    st.cx(qubit, ancilla_index);
    
    // Step 4: Add the conditional X gate to the CCT
    cct.add_gate({CliffordOperatorType::x, std::array<size_t, 2>{qubit, 0}});
    
    return {st, cct};
}

  // namespace





/**
 * @brief Process tableau and gadgetize H gates in StabilizerTableau when no PauliRotations follow.
 * Builds a new tableau incrementally, processing each subtableau and gadgetizing H gates.
 *
 * @param tableau
 */
void gadgetize_tableau(Tableau& tableau) {
    // Record the original qubit count
    size_t qubit_n = tableau.n_qubits();
    size_t ancilla_index = qubit_n;  // Start tracking ancilla_index from n_qubits
    
    // Create a new tableau to store processed circuits (start with empty, will build incrementally)
    std::vector<SubTableau> new_subtableaux;
    std::vector<std::pair<size_t, AncillaInitialState>> new_ancilla_states;

    size_t max_pr = 0;
    for (auto check_it = tableau.begin(); check_it != tableau.end(); ++check_it) {
        if (std::holds_alternative<std::vector<PauliRotation>>(*check_it)) {
            max_pr++;
        }
    }

    
    // Iterate through each subtableau without using indices
    for (auto it = tableau.begin(); it != tableau.end(); ++it) {
        // Check if there are rotations after this position
        size_t pr_count = 0;
        for (auto check_it = std::next(it); check_it != tableau.end(); ++check_it) {
            if (std::holds_alternative<std::vector<PauliRotation>>(*check_it)) {
                pr_count++;
            }
        }
        
        std::visit(
            dvlab::overloaded{
                [&](StabilizerTableau const& st) {
                    if(max_pr == pr_count || pr_count == 0) {
                        StabilizerTableau new_st(qubit_n);
                        for (auto const& op : extract_clifford_operators(st)) {
                            new_st.apply(op);
                        }
                        new_subtableaux.push_back(new_st);
                    }
                    else if (pr_count > 0) {
                        // Extract clifford operators
                        auto clifford_ops = extract_clifford_operators(st);
                        
                        // Process operators one by one
                        std::vector<CliffordOperator> ops_before_h;
                        
                        for (auto const& op : clifford_ops) {
                            if (op.first == CliffordOperatorType::h) {
                                // Found an H gate - gadgetize it
                                size_t qubit = op.second[0];
                                
                                // Step 1: Add ancilla for all subtableaux in the new tableau
                                add_ancilla_to_subtableaux(new_subtableaux);
                                
                                // Step 2: qubit_n++
                                qubit_n++;
                                
                                // Step 3: Create a new ST and apply the clifford operators before H to it
                                StabilizerTableau gad_st(qubit_n);
                                if (!ops_before_h.empty()) {
                                    for (auto const& op : ops_before_h) {
                                        gad_st.apply(op);
                                    }
                                }
                                
                                // Step 4: Apply gadgetize_hadamard to it and get the CCT
                                std::pair<StabilizerTableau, ClassicalControlTableau> result = gadgetize_hadamard(gad_st, qubit);
                                gad_st = result.first;
                                ClassicalControlTableau cct = result.second;
                                new_subtableaux.push_back(gad_st);
                                new_subtableaux.push_back(cct);
                                
                                // Add the ancilla initial state
                                new_ancilla_states.push_back({ancilla_index, AncillaInitialState::PLUS});
                                ancilla_index++;
                                
                                // Clear ops_before_h for next H gate
                                ops_before_h.clear();
                            } else {
                                // Collect operators before H
                                ops_before_h.push_back(op);
                            }
                        }
                        
                        // Step 5: Deal with remaining clifford operators after all H gates
                        if (!ops_before_h.empty()) {
                            StabilizerTableau remaining_st(qubit_n);
                            for (auto const& op : ops_before_h) {
                                remaining_st.apply(op);
                            }
                            new_subtableaux.push_back(remaining_st);
                        }
                    }
                },
                [&](std::vector<PauliRotation> const& rotations) {
                    // Ensure rotations have the correct qubit count by adding ancilla if needed
                    pr_count--;
                    std::vector<PauliRotation> copied_rotations;
                    for (auto const& rotation : rotations) {
                        PauliRotation copied_rotation = rotation;
                        // Add ancilla qubits if needed to match qubit_n
                        while (copied_rotation.n_qubits() < qubit_n) {
                            copied_rotation.add_ancilla_qubit();
                        }
                        copied_rotations.push_back(copied_rotation);
                    }
                    new_subtableaux.push_back(copied_rotations);
                },
                [&](ClassicalControlTableau const& cct) {
                    // Copy CCT (it should already have the correct size)
                    new_subtableaux.push_back(cct);
                }},
            *it);
    }
    
    // Build the new tableau from the collected subtableaux
    tableau = Tableau(qubit_n);
    // Clear the initial ST by erasing it
    if (!tableau.is_empty()) {
        tableau.erase(tableau.begin(), tableau.end());
    }
    // Add all collected subtableaux
    for (auto& subtableau : new_subtableaux) {
        tableau.push_back(subtableau);
    }
    
    // Add all ancilla states
    for (auto const& [anc_idx, state] : new_ancilla_states) {
        tableau.add_ancilla_state(anc_idx, state);
    }
}

}  // namespace

/**
 * @brief Minimize internal Hadamards, gadgetize H gates, commute classical operations, and optimize.
 *
 * @param tableau
 */

void minimize_internal_hadamards_n_gadgetize(Tableau& tableau) {
    size_t count              = 0;
    size_t non_clifford_count = tableau.n_pauli_rotations();
    spdlog::debug("TMerge");
    collapse(tableau);
    merge_rotations(tableau);
    properize(tableau);
    minimize_internal_hadamards(tableau);
    spdlog::trace("Tableau after internal hadamard minimization:\n{:b}", tableau);
    gadgetize_tableau(tableau);
    tableau.commute_classical();
    collapse_with_classical(tableau);
    spdlog::debug("Phase polynomial optimization");
    optimize_phase_polynomial(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    spdlog::info("Reduced the number of non-Clifford gates from {} to {}, at the cost of {} ancilla qubits", non_clifford_count, tableau.n_pauli_rotations(), tableau.ancilla_initial_states().size());

}

}


