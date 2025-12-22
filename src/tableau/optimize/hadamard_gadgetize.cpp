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
 * @brief Gadgetize a single Hadamard gate by creating two paired CCTs
 * 
 * Creates:
 * 1. CCC (Classical Control Clifford): Pre-measurement Clifford operations
 * 2. PMC (Post-Measurement Clifford): Conditional operations after measurement
 * 
 * The measurement of ancilla_index is implicit between CCC and PMC.
 * 
 * @param reference_qubit The qubit where H gate originally was (a)
 * @param ancilla_index The ancilla qubit for the gadget (b)
 * @param n_qubits Total number of qubits
 * @return std::pair<ClassicalControlTableau, ClassicalControlTableau> 
 *         First: CCC (pre-measurement), Second: PMC (post-measurement)
 */
std::pair<ClassicalControlTableau, ClassicalControlTableau> 
gadgetize_hadamard(size_t reference_qubit, size_t ancilla_index, size_t n_qubits) {
    
    // Create CCC: Pre-measurement Clifford operations
    ClassicalControlTableau ccc(ancilla_index, reference_qubit, n_qubits, CCTType::CCC);
    ccc.operations().s(ancilla_index);
    ccc.operations().s(reference_qubit);
    ccc.operations().cx(reference_qubit, ancilla_index);
    ccc.operations().sdg(ancilla_index);
    ccc.operations().cx(ancilla_index, reference_qubit);
    ccc.operations().cx(reference_qubit, ancilla_index);
    
    // Create PMC: Post-measurement conditional operations
    ClassicalControlTableau pmc(ancilla_index, reference_qubit, n_qubits, CCTType::PMC);
    pmc.add_gate({CliffordOperatorType::x, std::array<size_t, 2>{reference_qubit, 0}});
    
    // NOTE: Pairing will be done after insertion into Tableau
    // because we need stable addresses (can't pair before copying into vector)
    
    return {std::move(ccc), std::move(pmc)};
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
    std::vector<std::pair<size_t, size_t>> gadget_pairs;  // Track (CCC_idx, PMC_idx) for pairing

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
                                
                                // Step 3: Apply clifford operators before H
                                if (!ops_before_h.empty()) {
                                    StabilizerTableau st_before(qubit_n);
                                    for (auto const& op : ops_before_h) {
                                        st_before.apply(op);
                                    }
                                    new_subtableaux.push_back(st_before);
                                }
                                
                                // Step 4: Create paired CCTs for the Hadamard gadget
                                auto [ccc, pmc] = gadgetize_hadamard(qubit, ancilla_index, qubit_n);
                                
                                size_t ccc_idx = new_subtableaux.size();
                                new_subtableaux.push_back(std::move(ccc));
                                
                                size_t pmc_idx = new_subtableaux.size();
                                new_subtableaux.push_back(std::move(pmc));
                                
                                // Remember to pair them after tableau is built (need stable addresses)
                                gadget_pairs.push_back({ccc_idx, pmc_idx});
                                
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
        tableau.push_back(std::move(subtableau));
    }
    
    // Add all ancilla states
    for (auto const& [anc_idx, state] : new_ancilla_states) {
        tableau.add_ancilla_state(anc_idx, state);
    }
    
    // Now establish pairing between CCC and PMC (stable addresses in tableau)
    for (auto const& [ccc_idx, pmc_idx] : gadget_pairs) {
        auto* ccc_ptr = std::get_if<ClassicalControlTableau>(&tableau[ccc_idx]);
        auto* pmc_ptr = std::get_if<ClassicalControlTableau>(&tableau[pmc_idx]);
        
        if (ccc_ptr && pmc_ptr) {
            ccc_ptr->set_paired_cct(pmc_ptr);
            pmc_ptr->set_paired_cct(ccc_ptr);
            spdlog::debug("Paired CCC at index {} with PMC at index {} for ancilla {} and reference {}",
                         ccc_idx, pmc_idx, ccc_ptr->ancilla_qubit(), 
                         ccc_ptr->reference_qubit().value_or(0));
        }
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
    gadgetize_tableau(tableau);
    spdlog::trace("Tableau after internal hadamard minimization:\n{:b}", tableau);
}


}  // namespace qsyn::experimental


