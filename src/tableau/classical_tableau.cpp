/**
 * @file classical_tableau.cpp
 * @brief Implementation of classical-related operation functions for tableau
 *
 * @copyright Copyright (c) 2024
 */

#include <cassert>
#include <ranges>
#include <stdexcept>
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

namespace {

bool pr_all_diagonal(std::vector<PauliRotation> const& pr) {
    return std::ranges::all_of(pr, [](PauliRotation const& r) { return r.is_diagonal(); });
}

void refresh_cz_flag(PauliRotation& rotation) {
    if (rotation.phase() != dvlab::Phase(0)) {
        rotation.set_is_CZ(false);
    } else {
        size_t z_count = 0;
        for (size_t q = 0; q < rotation.n_qubits(); ++q) {
            if (rotation.get_pauli_type(q) == Pauli::z) {
                ++z_count;
            }
        }
        rotation.set_is_CZ(z_count == 2);
    }
}

/** Swap Z/I (or I) support between qubits a and b on a diagonal PauliRotation. */
void swap_gadget_z_slots(PauliRotation& r, size_t a, size_t b) {
    if (a == b) {
        return;
    }
    std::vector<Pauli> pv(r.n_qubits(), Pauli::i);
    for (size_t q = 0; q < r.n_qubits(); ++q) {
        pv[q] = r.get_pauli_type(q);
    }
    std::swap(pv[a], pv[b]);
    dvlab::Phase const ph = r.phase();
    r      = PauliRotation(pv.begin(), pv.end(), ph);
    refresh_cz_flag(r);
}

void gadget_conjugate_pr_with_ops(std::vector<PauliRotation>& pr, CliffordOperatorString const& ops) {
    for (auto& rotation : pr) {
        rotation.apply(ops);
        refresh_cz_flag(rotation);
    }
}

}  // namespace

ClassicalControlTableau::ClassicalControlTableau(CCTType type, size_t ancilla_qubit, size_t reference_qubit)
    : _ancilla_qubit(ancilla_qubit),
      _reference_qubit(reference_qubit),
      _operations(min_qubit_width(ancilla_qubit, reference_qubit)),
      _type(type),
      _measurement_type(MeasurementType::none) {
    if (type == CCTType::Gadget) {
        initialize_gadget(*this);
    } else {
        initialize_classical_control(*this);
    }
}

bool ClassicalControlTableau::clifford_touches_ancilla(CliffordOperator const& op, size_t ancilla_qubit) {
    auto const& [t, q] = op;
    if (t == CliffordOperatorType::cx) {
        return q[0] == ancilla_qubit || q[1] == ancilla_qubit;
    }
    return q[0] == ancilla_qubit;
}

void ClassicalControlTableau::check_gate_allowed_for_classical_control(CliffordOperatorType type,
                                                                       size_t q0,
                                                                       size_t q1) const {
    if (!is_classical_control()) {
        return;
    }
    if (type == CliffordOperatorType::cx) {
        if (q0 == _ancilla_qubit || q1 == _ancilla_qubit) {
            throw std::invalid_argument(
                "ClassicalControlTableau: Clifford must not act on ancilla qubit (CX touches ancilla)");
        }
    } else {
        if (q0 == _ancilla_qubit) {
            throw std::invalid_argument(
                "ClassicalControlTableau: Clifford must not act on ancilla qubit");
        }
    }
}

void ClassicalControlTableau::add_gate(CliffordOperator const& op) {
    if (is_gadget()) {
        throw std::invalid_argument(
            "ClassicalControlTableau: cannot add_gate on Gadget (fixed Clifford)");
    }
    auto const& [type, qubits] = op;
    if (!is_feasible_gate_type(type)) {
        throw std::invalid_argument("Gate type is not feasible for ClassicalControlTableau");
    }
    if (clifford_touches_ancilla(op, _ancilla_qubit)) {
        throw std::invalid_argument(
            "ClassicalControlTableau: gate must not act on ancilla qubit");
    }
    _operations.prepend(op);
}

void initialize_gadget(ClassicalControlTableau& cct) {
    assert(cct.is_gadget());
    size_t const n  = cct.operations().n_qubits();
    size_t const anc = cct.ancilla_qubit();
    size_t const ref = cct.reference_qubit();
    auto& st         = cct.operations();
    st               = StabilizerTableau{n};
    st.s(anc);
    st.s(ref);
    st.cx(ref, anc);
    st.sdg(anc);
    st.cx(anc, ref);
    st.cx(ref, anc);
    cct.set_measurement_type(MeasurementType::X);
}

void initialize_classical_control(ClassicalControlTableau& cct) {
    assert(cct.is_classical_control());
    size_t const n = cct.operations().n_qubits();
    cct.operations() = StabilizerTableau{n};
}

void swap_forward(ClassicalControlTableau& cct, StabilizerTableau& st) {
    assert(cct.operations().n_qubits() == st.n_qubits());
    if (cct.is_gadget()) {
        st = commutation_through_clifford(st, adjoint(cct.operations()));
    } else {
        cct.operations() = commutation_through_clifford(cct.operations(), st);
    }
}

void swap_back(StabilizerTableau& st, ClassicalControlTableau& cct) {
    assert(cct.operations().n_qubits() == st.n_qubits());
    if (cct.is_gadget()) {
        st = commutation_through_clifford(st, cct.operations());
    } else {
        cct.operations() = commutation_through_clifford(cct.operations(), adjoint(st));
    }
}

void swap_forward(ClassicalControlTableau& cct, std::vector<PauliRotation>& pr) {
    if (pr.empty()) {
        return;
    }
    assert(cct.operations().n_qubits() == pr.front().n_qubits());
    if (cct.is_gadget()) {
        if (pr_all_diagonal(pr)) {
            size_t const a = cct.reference_qubit();
            size_t const b = cct.ancilla_qubit();
            for (auto& r : pr) {
                if (r.is_diagonal()) {
                    swap_gadget_z_slots(r, a, b);
                }
            }
        } else {
            gadget_conjugate_pr_with_ops(pr, extract_clifford_operators(cct.operations()));
        }
    } else {
        commute_through_pauli_rotations(cct.operations(), pr);
    }
}

void swap_back(std::vector<PauliRotation>& pr, ClassicalControlTableau& cct) {
    if (pr.empty()) {
        return;
    }
    assert(cct.operations().n_qubits() == pr.front().n_qubits());
    if (!cct.is_gadget()) {
        throw std::logic_error("swap_back(PR, CCT): only Gadget + PR is supported in v1");
    }
    if (pr_all_diagonal(pr)) {
        size_t const a = cct.reference_qubit();
        size_t const b = cct.ancilla_qubit();
        for (auto& r : pr) {
            if (r.is_diagonal()) {
                swap_gadget_z_slots(r, a, b);
            }
        }
    } else {
        gadget_conjugate_pr_with_ops(pr, adjoint(extract_clifford_operators(cct.operations())));
    }
}

void swap_forward_cct_cct(ClassicalControlTableau& /*left*/, ClassicalControlTableau& /*right*/) {
    throw std::logic_error("swap_forward_cct_cct: not implemented (TODO: adjacent CCT reordering)");
}

void swap_back_cct_cct(ClassicalControlTableau& /*left*/, ClassicalControlTableau& /*right*/) {
    throw std::logic_error("swap_back_cct_cct: not implemented (TODO: adjacent CCT reordering)");
}

StabilizerTableau reverse_n_prepend(CliffordOperatorString const& operations, size_t n_qubits) {
    StabilizerTableau result = StabilizerTableau(n_qubits);
    for (auto it = operations.rbegin(); it != operations.rend(); ++it) {
        result.prepend(*it);
    }
    return result;
}

StabilizerTableau commutation_through_clifford(StabilizerTableau const& classical_clifford,
                                               StabilizerTableau const& clifford_block) {

    Tableau result_tableau = Tableau(clifford_block.n_qubits());

    result_tableau.push_back(adjoint(clifford_block));
    result_tableau.push_back(classical_clifford);
    result_tableau.push_back(clifford_block);

    collapse(result_tableau);
    remove_identities(result_tableau);
    assert(result_tableau.size() == 1 && std::holds_alternative<StabilizerTableau>(result_tableau.front()) &&
           "Result tableau should have only one element and be a stabilizer");

    return std::get<StabilizerTableau>(result_tableau.front());
}

void commute_through_stabilizer(ClassicalControlTableau& cct, StabilizerTableau& st) {
    swap_forward(cct, st);
}

std::pair<CliffordOperatorString, size_t> pauli_to_CXT(PauliRotation pauli_rotation) {
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

    for (size_t i = 1; i < c.size(); ++i) {
        result.push_back({CliffordOperatorType::cx, {c[i], c[0]}});
    }

    return {result, c[0]};
}

void commute_through_T(CliffordOperatorString& operations, size_t qubit_n) {
    CliffordOperatorString result;

    for (auto const& op : operations) {
        auto const& [type, qubits] = op;

        result.push_back(op);

        if ((type == CliffordOperatorType::x && qubits[0] == qubit_n) ||
            (type == CliffordOperatorType::y && qubits[0] == qubit_n)) {
            result.push_back({CliffordOperatorType::s, {qubit_n, 0}});
        }
    }

    operations = result;
}

void commute_through_Tdg(CliffordOperatorString& operations, size_t qubit_n) {
    CliffordOperatorString result;

    for (auto const& op : operations) {
        auto const& [type, qubits] = op;

        result.push_back(op);

        if ((type == CliffordOperatorType::x && qubits[0] == qubit_n) ||
            (type == CliffordOperatorType::y && qubits[0] == qubit_n)) {
            result.push_back({CliffordOperatorType::sdg, {qubit_n, 0}});
        }
    }

    operations = result;
}

void commute_through_CX(CliffordOperatorString& operations, size_t control_qubit, size_t target_qubit) {
    CliffordOperatorString result;
    for (auto const& op : operations) {
        auto const& [type, qubits] = op;
        if (((type == CliffordOperatorType::s || type == CliffordOperatorType::sdg) && qubits[0] == target_qubit) ||
            ((type == CliffordOperatorType::cx) && qubits[0] == target_qubit && qubits[1] == control_qubit)) {
            result.push_back({CliffordOperatorType::cx, {control_qubit, target_qubit}});
            result.push_back(op);
            result.push_back({CliffordOperatorType::cx, {control_qubit, target_qubit}});
        } else {
            result.push_back(op);
            if (type == CliffordOperatorType::x && qubits[0] == control_qubit) {
                result.push_back({CliffordOperatorType::x, {target_qubit, 0}});
            }
            if (type == CliffordOperatorType::y && qubits[0] == control_qubit) {
                result.push_back({CliffordOperatorType::x, {target_qubit, 0}});
            }
            if (type == CliffordOperatorType::y && qubits[0] == target_qubit) {
                result.push_back({CliffordOperatorType::z, {control_qubit, 0}});
            }
            if (type == CliffordOperatorType::z && qubits[0] == target_qubit) {
                result.push_back({CliffordOperatorType::z, {control_qubit, 0}});
            }
        }
    }
    operations = result;
}

void commute_through_pauli_rotation(StabilizerTableau& st, PauliRotation const& pauli_rotation) {
    size_t const n_qubits = st.n_qubits();
    assert(n_qubits == pauli_rotation.n_qubits() &&
           "StabilizerTableau and PauliRotation must have the same number of qubits");

    auto const [cxs, qubit] = pauli_to_CXT(pauli_rotation);

    StabilizerTableau cx_stabilizer(n_qubits);
    for (auto it = cxs.rbegin(); it != cxs.rend(); ++it) {
        cx_stabilizer.prepend_cx(it->second[0], it->second[1]);
    }

    st = commutation_through_clifford(st, cx_stabilizer);

    CliffordOperatorString ops = extract_clifford_operators(st);
    if (pauli_rotation.phase() == dvlab::Phase(std::numbers::pi_v<double> / 4.0)) {
        commute_through_T(ops, qubit);
    } else {
        assert(pauli_rotation.phase() == dvlab::Phase(-std::numbers::pi_v<double> / 4.0) &&
               "Phase must be pi/4 or -pi/4");
        commute_through_Tdg(ops, qubit);
    }
    st = reverse_n_prepend(ops, n_qubits);

    st = commutation_through_clifford(st, adjoint(cx_stabilizer));
}

void commute_through_pauli_rotation(ClassicalControlTableau& cct, PauliRotation const& pauli_rotation) {
    assert(cct.operations().n_qubits() == pauli_rotation.n_qubits() &&
           "ClassicalControlTableau and PauliRotation must have the same number of qubits");
    if (cct.is_gadget()) {
        throw std::logic_error(
            "commute_through_pauli_rotation(CCT, PR): use swap_forward(CCT, pr_vector) for Gadget");
    }
    commute_through_pauli_rotation(cct.operations(), pauli_rotation);
}

void commute_through_pauli_rotations(StabilizerTableau& st, std::vector<PauliRotation> const& pauli_rotations) {
    for (auto const& pauli_rotation : pauli_rotations) {
        commute_through_pauli_rotation(st, pauli_rotation);
    }
}

void commute_through_pauli_rotations(ClassicalControlTableau& cct, std::vector<PauliRotation>& pauli_rotations) {
    if (cct.is_gadget()) {
        swap_forward(cct, pauli_rotations);
        return;
    }
    commute_through_pauli_rotations(cct.operations(), pauli_rotations);
}

template <typename TableauType>
bool test_classical_equivalence_impl(ClassicalControlTableau const& cct_old,
                                     TableauType const& ta,
                                     ClassicalControlTableau const& cct_new) {

    StabilizerTableau cct_old_st = cct_old.operations();
    StabilizerTableau cct_new_st = cct_new.operations();

    Tableau old_tableau{cct_old_st.n_qubits()};
    old_tableau.push_back(cct_old_st);
    old_tableau.push_back(ta);

    Tableau new_tableau{cct_new_st.n_qubits()};
    new_tableau.push_back(ta);
    new_tableau.push_back(cct_new_st);

    Tableau adjoint_new_tableau = adjoint(new_tableau);

    Tableau combined_tableau{cct_old_st.n_qubits()};
    for (auto const& subtableau : old_tableau) {
        combined_tableau.push_back(subtableau);
    }
    for (auto const& subtableau : adjoint_new_tableau) {
        combined_tableau.push_back(subtableau);
    }

    full_optimize(combined_tableau);
    remove_identities(combined_tableau);
    if (combined_tableau.is_empty()) {
        spdlog::info("Combined tableau is empty");
    } else {
        print_clifford_operator_string(extract_clifford_operators(cct_old.operations()));
        print_clifford_operator_string(extract_clifford_operators(cct_new.operations()));
    }
    return combined_tableau.is_empty();
}

bool test_classical_equivalence(ClassicalControlTableau const& cct_old,
                                StabilizerTableau const& ta,
                                ClassicalControlTableau const& cct_new) {
    return test_classical_equivalence_impl(cct_old, ta, cct_new);
}

bool test_classical_equivalence(ClassicalControlTableau const& cct_old,
                                std::vector<PauliRotation> const& ta,
                                ClassicalControlTableau const& cct_new) {
    return test_classical_equivalence_impl(cct_old, ta, cct_new);
}

}  // namespace qsyn::experimental
