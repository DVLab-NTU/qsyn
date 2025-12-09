/**
 * @file classical_tableau.cpp
 * @brief Implementation of classical-related operation functions for tableau
 * 
 * @copyright Copyright (c) 2024
 */

#include <cassert>
#include <vector>
#include <variant>
#include "./classical_tableau.hpp"
#include "./stabilizer_tableau.hpp"
#include "./pauli_rotation.hpp"
#include "./tableau_optimization.hpp"
#include "./tableau.hpp"
#include "util/phase.hpp"
#include <numbers>
#include "spdlog/spdlog.h"


namespace qsyn::experimental {

StabilizerTableau reverse_n_prepend(CliffordOperatorString const& operations, size_t n_qubits) {
    StabilizerTableau result = StabilizerTableau(n_qubits);
    for (auto it = operations.rbegin(); it != operations.rend(); ++it) {
        result.prepend(*it);
    }
    return result;
}


StabilizerTableau commutation_through_clifford(StabilizerTableau const& classical_clifford, 
    StabilizerTableau const& clifford_block) {

    // Step 1: Create adjoint of clifford_block
    Tableau result_tableau = Tableau(clifford_block.n_qubits());

    result_tableau.push_back(adjoint(clifford_block));
    result_tableau.push_back(classical_clifford);
    result_tableau.push_back(clifford_block);

    collapse(result_tableau);
    remove_identities(result_tableau);
    assert(result_tableau.size() == 1 && std::holds_alternative<StabilizerTableau>(result_tableau.front()) && "Result tableau should have only one element and be a stabilizer");

    return std::get<StabilizerTableau>(result_tableau.front());
}
/**
 * @brief Commute a ClassicalControlTableau through a StabilizerTableau.
 *
 * @param cct
 * @param st
 */
void commute_through_stabilizer(ClassicalControlTableau& cct, StabilizerTableau& st) {
    // Get the sizes of the tableaus

    auto old_cct = cct;
    size_t cct_n_qubits = cct.operations().n_qubits();
    size_t st_n_qubits = st.n_qubits();
    
    // Assert that st has the same size as cct's operations
    assert(st_n_qubits == cct_n_qubits &&
           "StabilizerTableau must have the same size as ClassicalControlTableau's operations");
    
    // Commute operations through st
    // commutation_through_clifford(classical_clifford, clifford_block) commutes clifford_block through classical_clifford
    StabilizerTableau commuted_ops = commutation_through_clifford(cct.operations(), st);
    cct.operations() = commuted_ops;
    // bool is_equivalent = test_classical_equivalence(old_cct, st, cct);
    // if (!is_equivalent) {
    //     spdlog::error("Commutation through stabilizer failed");
    // } else {
    //     spdlog::info("Commutation through stabilizer succeeded");
    // }
}

/**
 * @brief Convert a PauliRotation to CX gates and target qubit index.
 *
 * @param pauli_rotation
 */
std::pair<CliffordOperatorString, size_t> pauli_to_CXT(PauliRotation pauli_rotation) {
    // Assert every bit is Z or I and keep track of Z qubits
    std::vector<size_t> c;

    for (size_t i = 0; i < pauli_rotation.n_qubits(); ++i) {
        assert((pauli_rotation.is_z(i) || pauli_rotation.is_i(i)) &&
               "All qubits must be Z or I");
        if (pauli_rotation.is_z(i)) {
            c.push_back(i);
        }
    }
    assert(!c.empty() && "No Z qubits found");

    CliffordOperatorString result;
    
    // From the second to last item in vector c, apply CX(c[i], c[0])
    for (size_t i = 1; i < c.size(); ++i) {
        result.push_back({CliffordOperatorType::cx, {c[i], c[0]}});
    }
    
    return {result, c[0]};
}

/**
 * @brief Commute through T gate by inserting S gates after X/Y gates on the specified qubit.
 *
 * @param operations
 * @param qubit_n
 */
void commute_through_T(CliffordOperatorString& operations, size_t qubit_n) {
    CliffordOperatorString result;
    
    for (auto const& op : operations) {
        auto const& [type, qubits] = op;
        
        // Add the current operation
        result.push_back(op);
        
        // Check if this is an X or Y gate on qubit n
        if ((type == CliffordOperatorType::x && qubits[0] == qubit_n) || (type == CliffordOperatorType::y && qubits[0] == qubit_n)) {
            // Insert S gate after X or Y gate on qubit n
            result.push_back({CliffordOperatorType::s, {qubit_n, 0}});
        } 
    }
    
    operations = result;
}

/**
 * @brief Commute through Tdg gate by inserting SDG gates after X/Y gates on the specified qubit.
 *
 * @param operations
 * @param qubit_n
 */
void commute_through_Tdg(CliffordOperatorString& operations, size_t qubit_n) {
    CliffordOperatorString result;
    
    for (auto const& op : operations) {
        auto const& [type, qubits] = op;
        
        // Add the current operation

        result.push_back(op);
        
        // Check if this is an X or Y gate on qubit n
        if ((type == CliffordOperatorType::x && qubits[0] == qubit_n) || (type == CliffordOperatorType::y && qubits[0] == qubit_n)) {
            // Insert SDG gate after X or Y gate on qubit n
            result.push_back({CliffordOperatorType::sdg, {qubit_n, 0}});
        } 
    }
    
    operations = result;
}

/**
 * @brief Commute through CX gate by wrapping operations with CX gates at the start and end.
 *
 * @param operations
 * @param control_qubit
 * @param target_qubit
 */

void commute_through_CX(CliffordOperatorString& operations, size_t control_qubit, size_t target_qubit) {
    CliffordOperatorString result;
    // Add CX gate at the start
    // Add all the original operations
    for (auto const& op : operations) {
        auto const& [type, qubits] = op;
        if (((type == CliffordOperatorType::s || type == CliffordOperatorType::sdg) && qubits[0] == target_qubit)
            || ((type == CliffordOperatorType::cx) && qubits[0] == target_qubit && qubits[1] == control_qubit)) {
            result.push_back({CliffordOperatorType::cx, {control_qubit, target_qubit}});
            result.push_back(op);
            result.push_back({CliffordOperatorType::cx, {control_qubit, target_qubit}});
        }
        else{
            result.push_back(op);
            // [c,t] = [X,I]
            if (type == CliffordOperatorType::x && qubits[0] == control_qubit) {
                result.push_back({CliffordOperatorType::x, {target_qubit, 0}});
            }
            // [c,t] = [Y,I]
            if (type == CliffordOperatorType::y && qubits[0] == control_qubit) {
                result.push_back({CliffordOperatorType::x, {target_qubit, 0}});
            }
            // [c,t] = [I,Y]
            if (type == CliffordOperatorType::y && qubits[0] == target_qubit) {
                result.push_back({CliffordOperatorType::z, {control_qubit, 0}});
            }
            // [c,t] = [I,Z]
            if (type == CliffordOperatorType::z && qubits[0] == target_qubit) {
                result.push_back({CliffordOperatorType::z, {control_qubit, 0}});
            }
        }
    }
    // Add CX gate at the end
    operations = result;
}

/**
 * @brief Commute a ClassicalControlTableau through a vector of PauliRotations.
 *
 * @param cct
 * @param pauli_rotations
 */
void commute_through_pauli_rotation(ClassicalControlTableau& cct, std::vector<PauliRotation>& pauli_rotations) {
    
    auto old_cct = cct;
    size_t cct_n_qubits = cct.operations().n_qubits();
    size_t pauli_n_qubits = pauli_rotations[0].n_qubits();
    assert(cct_n_qubits == pauli_n_qubits &&
           "ClassicalControlTableau and PauliRotations must have the same number of qubits, cct_n_qubits: ");

    for (auto const& pauli_rotation : pauli_rotations) {
        // Decompose pauli_rotation into: CXs, T, reverse(CXs)
        auto const [cxs, qubit] = pauli_to_CXT(pauli_rotation);
        
        // Step 1: Commute through CXs (forward)
        // Convert CXs to StabilizerTableau (prepend in reverse order to get correct sequence)
        StabilizerTableau cx_stabilizer = StabilizerTableau(cct_n_qubits);
        for (auto it = cxs.rbegin(); it != cxs.rend(); ++it) {
            auto const& [cx_type, cx_qubits] = *it;
            cx_stabilizer.prepend_cx(cx_qubits[0], cx_qubits[1]);
        }

        // Commute cct through the CX stabilizer
        commute_through_stabilizer(cct, cx_stabilizer);
        // Step 2: Commute through T/Tdg
        // Convert cct.operations() to operations, apply T/Tdg transformations, then convert back
        CliffordOperatorString classical_operations = extract_clifford_operators(cct.operations());
        
        if(pauli_rotation.phase() == dvlab::Phase(std::numbers::pi_v<double>/4.0)){
            commute_through_T(classical_operations, qubit);
        }
        else{
            assert(pauli_rotation.phase() == dvlab::Phase(-std::numbers::pi_v<double>/4.0) && "Phase must be pi/4 or -pi/4");
            commute_through_Tdg(classical_operations, qubit);
        }
       
        cct.operations() = reverse_n_prepend(classical_operations, cct_n_qubits);
        cx_stabilizer = adjoint(cx_stabilizer);
        commute_through_stabilizer(cct, cx_stabilizer);
    }
    
    // bool is_equivalent = test_classical_equivalence(old_cct, pauli_rotations, cct);
    // if (!is_equivalent) {
    //     CliffordOperatorString classical_opration = extract_clifford_operators(cct.operations());
    //     spdlog::error("Commutation through Pauli rotation failed");
    // }
    // else{
    //     spdlog::info("Commutation through Pauli rotation succeeded");
    // }
}

template<typename TableauType>
bool test_classical_equivalence_impl(
    ClassicalControlTableau const& cct_old,
    TableauType const& ta,
    ClassicalControlTableau const& cct_new) {
    
    // Extract stabilizers from CCTs
    StabilizerTableau cct_old_st = cct_old.operations();
    StabilizerTableau cct_new_st = cct_new.operations();
    
    // Create old_tableau: [cct_old.operations(), ta]
    Tableau old_tableau{cct_old_st.n_qubits()};
    old_tableau.push_back(cct_old_st);
    old_tableau.push_back(ta);
    
    // Create new_tableau: [ta, cct_new.operations()] (reversed order)
    Tableau new_tableau{cct_new_st.n_qubits()};
    new_tableau.push_back(ta);
    new_tableau.push_back(cct_new_st);

    // Adjoint the new_tableau
    Tableau adjoint_new_tableau = adjoint(new_tableau);
    
    // Concatenate: [cct_old, ta, adjoint(ta, cct_new)]
    Tableau combined_tableau{cct_old_st.n_qubits()};
    for (auto const& subtableau : old_tableau) {
        combined_tableau.push_back(subtableau);
    }
    for (auto const& subtableau : adjoint_new_tableau) {
        combined_tableau.push_back(subtableau);
    }
    
    // Apply full optimization
    full_optimize(combined_tableau);
    remove_identities(combined_tableau);
    if(combined_tableau.is_empty()){
        spdlog::info("Combined tableau is empty");
    }
    else{
        print_clifford_operator_string(extract_clifford_operators(cct_old.operations()));
        print_clifford_operator_string(extract_clifford_operators(cct_new.operations()));
    }
    // If the result is empty (identity), the circuits are equivalent
    return combined_tableau.is_empty();
}



bool test_classical_equivalence(
    ClassicalControlTableau const& cct_old,
    StabilizerTableau const& ta,
    ClassicalControlTableau const& cct_new) {
    return test_classical_equivalence_impl(cct_old, ta, cct_new);
}

bool test_classical_equivalence(
    ClassicalControlTableau const& cct_old,
    std::vector<PauliRotation> const& ta,
    ClassicalControlTableau const& cct_new) {
    return test_classical_equivalence_impl(cct_old, ta, cct_new);
}

}  // namespace experimental


