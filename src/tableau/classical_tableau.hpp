/**
 * @file classical_tableau.hpp
 * @brief Define classical-related operation classes for tableau
 *
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cassert>
#include <stdexcept>
#include <optional>
#include <variant>
#include "./stabilizer_tableau.hpp"
#include "./pauli_rotation.hpp"
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

/**
 * @brief Kind of ClassicalControlTableau: Hadamard gadget (pre-measurement) vs classical-controlled Cliffords.
 */
enum class CCTType {
    Gadget,            // Pre-measurement gadget Clifford (fixed canonical form)
    ClassicalControl,  // Post-measurement / classically controlled Cliffords on data qubits only
};

class ClassicalControlTableau;
class Tableau;
void initialize_gadget(ClassicalControlTableau& cct);
void initialize_classical_control(ClassicalControlTableau& cct);

/**
 * @brief Represents a quantum operation controlled by a qubit.
 * Contains a single stabilizer tableau for Clifford operations.
 * Only certain gate types are allowed (S, SDG, CX, H, X, Y, Z).
 *
 * Paired gadget + classical-control blocks form a Hadamard gadget.
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

    /** Minimum StabilizerTableau width for given ancilla and reference indices. */
    static size_t min_qubit_width(size_t ancilla_qubit, size_t reference_qubit) {
        return std::max(ancilla_qubit, reference_qubit) + 1;
    }

    /** Primary constructor: width = max(ancilla, ref) + 1; dispatches to initializers. */
    ClassicalControlTableau(CCTType type, size_t ancilla_qubit, size_t reference_qubit);

    ClassicalControlTableau(size_t ancilla_qubit, size_t n_qubits)
        : _ancilla_qubit(ancilla_qubit),
          _reference_qubit(0),
          _operations(n_qubits),
          _type(CCTType::ClassicalControl),
          _measurement_type(MeasurementType::none),
          _classical_bit_id(std::nullopt),
          _span_start_index(std::nullopt) {
        initialize_classical_control(*this);
    }

    ClassicalControlTableau(size_t ancilla_qubit, size_t reference_qubit, size_t n_qubits)
        : _ancilla_qubit(ancilla_qubit),
          _reference_qubit(reference_qubit),
          _operations(n_qubits),
          _type(CCTType::ClassicalControl),
          _measurement_type(MeasurementType::none),
          _classical_bit_id(std::nullopt),
          _span_start_index(std::nullopt) {
        initialize_classical_control(*this);
    }

    ClassicalControlTableau(size_t ancilla_qubit, size_t reference_qubit, size_t n_qubits, CCTType type)
        : _ancilla_qubit(ancilla_qubit),
          _reference_qubit(reference_qubit),
          _operations(n_qubits),
          _type(type),
          _measurement_type(MeasurementType::none),
          _classical_bit_id(std::nullopt),
          _span_start_index(std::nullopt) {
        if (type == CCTType::Gadget) {
            initialize_gadget(*this);
        } else {
            initialize_classical_control(*this);
        }
    }

    size_t ancilla_qubit() const { return _ancilla_qubit; }
    size_t reference_qubit() const { return _reference_qubit; }
    CCTType type() const { return _type; }
    void set_ancilla_qubit(size_t ancilla_qubit);
    void set_reference_qubit(size_t reference_qubit);
    void set_qubits(size_t ancilla_qubit, size_t reference_qubit);

    StabilizerTableau& operations() { return _operations; }
    StabilizerTableau const& operations() const { return _operations; }

    bool is_gadget() const { return _type == CCTType::Gadget; }
    bool is_classical_control() const { return _type == CCTType::ClassicalControl; }


    MeasurementType measurement_type() const { return _measurement_type; }
    void set_measurement_type(MeasurementType t) { _measurement_type = t; }
    bool has_classical_bit_id() const { return _classical_bit_id.has_value(); }
    size_t classical_bit_id() const { return _classical_bit_id.value(); }
    void set_classical_bit_id(size_t id) { _classical_bit_id = id; }
    void clear_classical_bit_id() { _classical_bit_id.reset(); }

    bool has_span_start_index() const { return _span_start_index.has_value(); }
    size_t span_start_index() const { return _span_start_index.value(); }
    void set_span_start_index(size_t idx) { _span_start_index = idx; }
    void clear_span_start_index() { _span_start_index.reset(); }

    void add_gate(CliffordOperator const& op);

    void add_ancilla_qubit() {
        _operations.add_ancilla_qubit();
    }
    void remove_ancilla_qubit(size_t qubit) {
        _operations.remove_ancilla_qubit(qubit);
        if (qubit < _ancilla_qubit) {
            _ancilla_qubit--;
        }
    }

    /** True if the gate acts on ancilla (single-qubit or CX touching ancilla). */
    static bool clifford_touches_ancilla(CliffordOperator const& op, size_t ancilla_qubit);

    /** For ClassicalControlTableau::apply_tableau_gate — throws if classical-control and gate hits ancilla. */
    void check_gate_allowed_for_classical_control(CliffordOperatorType type,
                                                  size_t q0,
                                                  size_t q1 = 0) const;

private:
    size_t _ancilla_qubit;
    size_t _reference_qubit;
    StabilizerTableau _operations;
    CCTType _type;
    MeasurementType _measurement_type;
    std::optional<size_t> _classical_bit_id;
    std::optional<size_t> _span_start_index;
};

void swap(ClassicalControlTableau& cct, StabilizerTableau& st);
void swap(StabilizerTableau& st, ClassicalControlTableau& cct);
void swap(ClassicalControlTableau& cct, std::vector<PauliRotation>& pr);
void swap(std::vector<PauliRotation>& pr, ClassicalControlTableau& cct);
void swap(std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>& left,
          std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>& right);
void swap_along(std::vector<std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>>& tableau_vector, size_t from_idx, size_t to_idx);
void swap_along(Tableau& tableau, size_t from_idx, size_t to_idx);
std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau> swap_along_test(
    std::vector<std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>> const& tableau_vector,
    size_t from_idx,
    size_t to_idx);
std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau> swap_along_test(
    Tableau const& tableau,
    size_t from_idx,
    size_t to_idx);

bool check_swap(ClassicalControlTableau const& left, ClassicalControlTableau const& right);

void swap(ClassicalControlTableau& left, ClassicalControlTableau& right);

StabilizerTableau commutation_through_clifford(StabilizerTableau const& classical_clifford,
                                               StabilizerTableau const& clifford_block);
StabilizerTableau reverse_n_prepend(CliffordOperatorString const& operations, size_t n_qubits);

/** Swap Z-support between reference and ancilla on a diagonal PR (commute past Hadamard gadget). */
void swap_gadget_phase_slots(PauliRotation& r, size_t reference, size_t ancilla);

void commute_through_pauli_rotation(StabilizerTableau& st, PauliRotation const& pauli_rotation, bool from_front);
void commute_through_pauli_rotations(StabilizerTableau& st, std::vector<PauliRotation> const& pauli_rotations, bool from_front);

void commute_through_T(CliffordOperatorString& operations, size_t qubit_n);
void commute_through_Tdg(CliffordOperatorString& operations, size_t qubit_n);
std::pair<CliffordOperatorString, size_t> pauli_to_CXT(PauliRotation pauli_rotation);

bool test_classical_equivalence(ClassicalControlTableau const& cct_old, StabilizerTableau const& tableau, ClassicalControlTableau const& cct_new);
bool test_classical_equivalence(ClassicalControlTableau const& cct_old, std::vector<PauliRotation> const& tableau, ClassicalControlTableau const& cct_new);
bool test_classical_equivalence(ClassicalControlTableau const& cct_old, StabilizerTableau const& old_tableau, StabilizerTableau const& new_tableau);
bool test_classical_equivalence(ClassicalControlTableau const& cct_old, std::vector<PauliRotation> const& old_tableau, std::vector<PauliRotation> const& new_tableau);
bool test_classical_equivalence_reverse(ClassicalControlTableau const& cct_old, StabilizerTableau const& tableau, ClassicalControlTableau const& cct_new);
bool test_classical_equivalence_reverse(ClassicalControlTableau const& cct_old, std::vector<PauliRotation> const& tableau, ClassicalControlTableau const& cct_new);

}  // namespace experimental

}  // namespace qsyn
