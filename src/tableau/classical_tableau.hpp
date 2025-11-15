/**
 * @file classical_tableau.hpp
 * @brief Define classical-related operation classes for tableau
 * 
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <cstddef>
#include <cassert>
#include <stdexcept>
#include "./stabilizer_tableau.hpp"
#include "./pauli_rotation.hpp"
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

/**
 * @brief Represents a quantum operation controlled by a qubit.
 * Contains a single stabilizer tableau for Clifford operations.
 * Only certain gate types are allowed (S, SDG, CX, H, X, Y, Z).
 */
class ClassicalControlTableau {
public:
    static bool is_feasible_gate_type(CliffordOperatorType gate_type) {
        switch (gate_type) {
            case CliffordOperatorType::s:
            case CliffordOperatorType::sdg:
            case CliffordOperatorType::cx:
            case CliffordOperatorType::h:
            case CliffordOperatorType::x:
            case CliffordOperatorType::y:
            case CliffordOperatorType::z:
                return true;
            default:
                return false;
        }
    }

    ClassicalControlTableau(size_t control_qubit, size_t n_qubits)
        : _control_qubit(control_qubit),
          _operations(n_qubits) {}
    
    size_t control_qubit() const { return _control_qubit; }
    
    StabilizerTableau& operations() { return _operations; }
    StabilizerTableau const& operations() const { return _operations; }

    void add_gate(CliffordOperator const& op) {
        auto const& [type, qubits] = op;
        if (!is_feasible_gate_type(type)) {
            throw std::invalid_argument("Gate type is not feasible for ClassicalControlTableau");
        }
        _operations.prepend(op);
    }
    void add_ancilla_qubit() {
        _operations.add_ancilla_qubit();
    }

private:
    size_t _control_qubit;                    // The qubit controlling the operation
    StabilizerTableau _operations;            // Tableau for all Clifford operations
};

StabilizerTableau commutation_through_clifford(StabilizerTableau const& classical_clifford, 
                                               StabilizerTableau const& clifford_block);

void commute_through_stabilizer(ClassicalControlTableau& cct, StabilizerTableau& st);
void commute_through_pauli_rotation(ClassicalControlTableau& cct, std::vector<PauliRotation>& pauli_rotations);

void commute_through_T(CliffordOperatorString& operations, size_t qubit_n);
void commute_through_Tdg(CliffordOperatorString& operations, size_t qubit_n);
void commute_through_S_Sdg(CliffordOperatorString& operations, size_t qubit_n);
void commute_through_CX(CliffordOperatorString& operations, size_t control_qubit, size_t target_qubit);
std::pair<CliffordOperatorString, size_t> pauli_to_CXT(PauliRotation pauli_rotation);

bool test_classical_equivalence(ClassicalControlTableau const& cct_old, StabilizerTableau const& tableau, ClassicalControlTableau const& cct_new);
bool test_classical_equivalence(ClassicalControlTableau const& cct_old, std::vector<PauliRotation> const& tableau, ClassicalControlTableau const& cct_new);

}  // namespace experimental

}  // namespace qsyn

