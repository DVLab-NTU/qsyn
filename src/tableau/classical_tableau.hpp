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
#include <optional>
#include "./stabilizer_tableau.hpp"
#include "./pauli_rotation.hpp"
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

/**
 * @brief Type classification for ClassicalControlTableau
 * CCC: Classical Control Clifford (pre-measurement setup for Hadamard gadget)
 * PMC: Post-Measurement Clifford (conditional operations after measurement)
 */
enum class CCTType {
    CCC,  // Pre-measurement Clifford operations
    PMC   // Post-measurement conditional operations
};

/**
 * @brief Represents a quantum operation controlled by a qubit.
 * Contains a single stabilizer tableau for Clifford operations.
 * Only certain gate types are allowed (S, SDG, CX, H, X, Y, Z).
 * 
 * Can be paired with another CCT to form a Hadamard gadget:
 * - CCC (pre-measurement) + measurement + PMC (post-measurement)
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

    ClassicalControlTableau(size_t ancilla_qubit, size_t n_qubits)
        : _ancilla_qubit(ancilla_qubit),
          _reference_qubit(std::nullopt),
          _operations(n_qubits),
          _type(CCTType::PMC),
          _paired_cct(nullptr) {}
    
    ClassicalControlTableau(size_t ancilla_qubit, size_t reference_qubit, size_t n_qubits)
        : _ancilla_qubit(ancilla_qubit),
          _reference_qubit(reference_qubit),
          _operations(n_qubits),
          _type(CCTType::PMC),
          _paired_cct(nullptr) {}
    
    // Constructor with type specification for Hadamard gadgets
    ClassicalControlTableau(size_t ancilla_qubit, size_t reference_qubit, size_t n_qubits, CCTType type)
        : _ancilla_qubit(ancilla_qubit),
          _reference_qubit(reference_qubit),
          _operations(n_qubits),
          _type(type),
          _paired_cct(nullptr) {}
    
    size_t ancilla_qubit() const { return _ancilla_qubit; }
    std::optional<size_t> reference_qubit() const { return _reference_qubit; }
    CCTType type() const { return _type; }
    
    StabilizerTableau& operations() { return _operations; }
    StabilizerTableau const& operations() const { return _operations; }
    
    // Pairing methods for Hadamard gadgets
    void set_paired_cct(ClassicalControlTableau* paired) { _paired_cct = paired; }
    ClassicalControlTableau* get_paired_cct() { return _paired_cct; }
    ClassicalControlTableau const* get_paired_cct() const { return _paired_cct; }
    
    bool is_part_of_hadamard_gadget() const { return _paired_cct != nullptr; }
    bool is_ccc() const { return _type == CCTType::CCC; }
    bool is_pmc() const { return _type == CCTType::PMC; }

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
    size_t _ancilla_qubit;                    // The ancilla qubit that controls the operation (b)
    std::optional<size_t> _reference_qubit;   // The reference qubit where H gate was applied (a)
    StabilizerTableau _operations;            // Tableau for all Clifford operations
    CCTType _type;                            // CCC (pre-measurement) or PMC (post-measurement)
    ClassicalControlTableau* _paired_cct;     // Pointer to paired CCT in Hadamard gadget
};

StabilizerTableau commutation_through_clifford(StabilizerTableau const& classical_clifford, 
                                               StabilizerTableau const& clifford_block);
StabilizerTableau reverse_n_prepend(CliffordOperatorString const& operations, size_t n_qubits);

void commute_through_stabilizer(ClassicalControlTableau& cct, StabilizerTableau& st);
void commute_through_pauli_rotation(ClassicalControlTableau& cct, std::vector<PauliRotation>& pauli_rotations);

void commute_through_T(CliffordOperatorString& operations, size_t qubit_n);
void commute_through_Tdg(CliffordOperatorString& operations, size_t qubit_n);
void commute_through_CX(CliffordOperatorString& operations, size_t control_qubit, size_t target_qubit);
std::pair<CliffordOperatorString, size_t> pauli_to_CXT(PauliRotation pauli_rotation);

bool test_classical_equivalence(ClassicalControlTableau const& cct_old, StabilizerTableau const& tableau, ClassicalControlTableau const& cct_new);
bool test_classical_equivalence(ClassicalControlTableau const& cct_old, std::vector<PauliRotation> const& tableau, ClassicalControlTableau const& cct_new);

// H-gadget pair structure for degadgetization
struct HadamardGadgetPair {
    size_t ccc_index;              // Index of CCC in tableau
    size_t pmc_index;              // Index of PMC in tableau
    size_t ancilla_qubit;          // Ancilla qubit (b)
    std::optional<size_t> reference_qubit;  // Reference qubit (a)
    bool is_paired;                // Whether CCC and PMC are properly paired
};

}  // namespace experimental

}  // namespace qsyn

