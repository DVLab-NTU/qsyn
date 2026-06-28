/**
 * @file classical_tableau.cpp
 * @brief Implementation of classical-related operation functions for tableau
 *
 * @copyright Copyright (c) 2024
 */

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stdexcept>
#include <string_view>
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
    return std::ranges::all_of(pr, [](PauliRotation const& r) { return r.is_diagonal() || r.is_CZ(); });
}

bool pr_has_cz(std::vector<PauliRotation> const& pr) {
    return std::ranges::any_of(pr, [](PauliRotation const& r) { return r.is_CZ(); });
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

StabilizerTableau compose_stabilizer_tableau(StabilizerTableau const& first, StabilizerTableau const& second) {
    StabilizerTableau result = first;
    result.apply(extract_clifford_operators(second));
    return result;
}

}  // namespace

void swap_gadget_phase_slots(PauliRotation& r, size_t reference, size_t ancilla) {
    if (reference == ancilla) {
        return;
    }
    if (!r.is_diagonal()) {
        return;
    }
    std::vector<Pauli> pv(r.n_qubits(), Pauli::i);
    for (size_t q = 0; q < r.n_qubits(); ++q) {
        pv[q] = r.get_pauli_type(q);
    }
    std::swap(pv[reference], pv[ancilla]);
    dvlab::Phase const ph = r.phase();
    r                    = PauliRotation(pv.begin(), pv.end(), ph);
    refresh_cz_flag(r);
}

bool check_swap(ClassicalControlTableau const& left, ClassicalControlTableau const& right) {
    if (left.is_gadget() || right.is_gadget()) {
        return false;
    }
    if (!left.is_classical_control() || !right.is_classical_control()) {
        return false;
    }
    if (left.operations().n_qubits() != right.operations().n_qubits()) {
        return false;
    }
    StabilizerTableau lr = left.operations();
    lr.apply(extract_clifford_operators(right.operations()));
    StabilizerTableau rl = right.operations();
    rl.apply(extract_clifford_operators(left.operations()));
    return (lr == rl);
}



ClassicalControlTableau::ClassicalControlTableau(CCTType type, size_t ancilla_qubit, size_t reference_qubit)
    : _ancilla_qubit(ancilla_qubit),
      _reference_qubit(reference_qubit),
      _operations(min_qubit_width(ancilla_qubit, reference_qubit)),
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

void ClassicalControlTableau::set_ancilla_qubit(size_t ancilla_qubit) {
    set_qubits(ancilla_qubit, _reference_qubit);
}

void ClassicalControlTableau::set_reference_qubit(size_t reference_qubit) {
    set_qubits(_ancilla_qubit, reference_qubit);
}

void ClassicalControlTableau::set_qubits(size_t ancilla_qubit, size_t reference_qubit) {
    if (_ancilla_qubit == ancilla_qubit && _reference_qubit == reference_qubit) {
        return;
    }

    size_t const required_n_qubits = std::max(_operations.n_qubits(), min_qubit_width(ancilla_qubit, reference_qubit));
    _ancilla_qubit   = ancilla_qubit;
    _reference_qubit = reference_qubit;
    _operations = StabilizerTableau{required_n_qubits};
    _operations.s(_ancilla_qubit);
    _operations.s(_reference_qubit);
    _operations.cx(_reference_qubit, _ancilla_qubit);
    _operations.sdg(_ancilla_qubit);
    _operations.cx(_ancilla_qubit, _reference_qubit);
    _operations.cx(_reference_qubit, _ancilla_qubit);
    _measurement_type = MeasurementType::X;
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

void swap(ClassicalControlTableau& cct, StabilizerTableau& st) {
    assert(cct.operations().n_qubits() == st.n_qubits());
    if (cct.is_gadget()) {
        st = commutation_through_clifford(st, adjoint(cct.operations()));
    } else {
        cct.operations() = commutation_through_clifford(cct.operations(), st);
    }
}

void swap(StabilizerTableau& st, ClassicalControlTableau& cct) {
    assert(cct.operations().n_qubits() == st.n_qubits());
    if (cct.is_gadget()) {
        st = commutation_through_clifford(st, cct.operations());
    } else {
        cct.operations() = commutation_through_clifford(cct.operations(), adjoint(st));
    }
}

void swap(ClassicalControlTableau& cct, std::vector<PauliRotation>& pr) {
    if (pr.empty()) {
        return;
    }
    assert(cct.operations().n_qubits() == pr.front().n_qubits());
    if (pr_has_cz(pr)) {
        spdlog::error(
            "swap(CCT,PR): export error: CZ-marked PR is not expected in current pipeline; lower CZ to S/Z/Sdg first");
        throw std::logic_error(
            "swap(CCT,PR): export error: CZ-marked PR is not expected in current pipeline; lower CZ to S/Z/Sdg first");
    }
    if (cct.is_gadget()) {
        if (pr_all_diagonal(pr)) {
            size_t const a = cct.reference_qubit();
            size_t const b = cct.ancilla_qubit();
            for (auto& r : pr) {
                if (r.is_diagonal()) {
                    swap_gadget_phase_slots(r, a, b);
                }
            }
        } else {
            spdlog::error(
                "swap(CCT,PR): non-diagonal PR with gadget CCT is unsupported; export should reject this case");
            throw std::logic_error(
                "swap(CCT,PR): non-diagonal PR with gadget CCT is unsupported; export should reject this case");
        }
    } else {
        commute_through_pauli_rotations(cct.operations(), pr, false);        
    }
}

void swap(std::vector<PauliRotation>& pr, ClassicalControlTableau& cct) {
    if (pr.empty()) {
        return;
    }
    assert(cct.operations().n_qubits() == pr.front().n_qubits());
    if (pr_has_cz(pr)) {
        spdlog::error(
            "swap(PR,CCT): export error: CZ-marked PR is not expected in current pipeline; lower CZ to S/Z/Sdg first");
        throw std::logic_error(
            "swap(PR,CCT): export error: CZ-marked PR is not expected in current pipeline; lower CZ to S/Z/Sdg first");
    }
    if (!cct.is_gadget()) {
        commute_through_pauli_rotations(cct.operations(), pr, true);
    } else if (pr_all_diagonal(pr)) {
        size_t const a = cct.reference_qubit();
        size_t const b = cct.ancilla_qubit();
        for (auto& r : pr) {
            if (r.is_diagonal()) {
                swap_gadget_phase_slots(r, a, b);
            }
        }
    }
    else {
        spdlog::error(
            "swap(PR,CCT): non-diagonal PR with gadget CCT is unsupported; export should reject this case");
        throw std::logic_error(
            "swap(PR,CCT): non-diagonal PR with gadget CCT is unsupported; export should reject this case");
    }
}

void swap(ClassicalControlTableau& left, ClassicalControlTableau& right) {
    if (left.operations().n_qubits() != right.operations().n_qubits()) {
        spdlog::error("swap(CCT,CCT): left and right must have matching StabilizerTableau width");
        throw std::logic_error("swap(CCT,CCT): left and right must have matching StabilizerTableau width");
    }
    bool const left_gadget  = left.is_gadget();
    bool const right_gadget = right.is_gadget();

    if (left_gadget && right_gadget) {
        if (left.reference_qubit() == right.reference_qubit()) {
            spdlog::error("swap(CCT,CCT): cannot swap two Hadamard gadgets with the same reference qubit");
            throw std::logic_error(
                "swap(CCT,CCT): cannot swap two Hadamard gadgets with the same reference qubit");
        }
        return;
    }

    if (!left_gadget && !right_gadget) {
        if (!check_swap(left, right)) {
            spdlog::error(
                "swap(CCT,CCT): adjacent PMCs do not commute (compose(left,right) != compose(right,left))");
            throw std::logic_error(
                "swap(CCT,CCT): adjacent PMCs do not commute (compose(left,right) != compose(right,left))");
        }
        return;
    }

    if (left_gadget && !right_gadget) {
        // Tableau order [CCC][PMC]: mutate PMC only; gadget operations unchanged.
        swap(left.operations(), right);
        return;
    }

    // [PMC][CCC]
    assert(!left_gadget && right_gadget);
    swap(left, right.operations());
}

void swap(std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>& left,
          std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>& right) {
    std::visit(
        dvlab::overloaded{
            [](ClassicalControlTableau& cct, StabilizerTableau& st) {
                swap(cct, st);
            },
            [](StabilizerTableau& st, ClassicalControlTableau& cct) {
                swap(st, cct);
            },
            [](ClassicalControlTableau& cct, std::vector<PauliRotation>& pr) {
                swap(cct, pr);                
            },
            [](std::vector<PauliRotation>& pr, ClassicalControlTableau& cct) {
                swap(pr, cct);
            },
            [](ClassicalControlTableau& left_cct, ClassicalControlTableau& right_cct) {
                swap(left_cct, right_cct);
            },
            [](std::vector<PauliRotation>& pr, StabilizerTableau& st) {
                auto const clifford_ops = extract_clifford_operators(st);
                for (auto& rotation : pr) {
                    rotation.apply(clifford_ops);
                }
            },
            [](StabilizerTableau& st, std::vector<PauliRotation>& pr) {
                auto const clifford_ops = extract_clifford_operators(adjoint(st));
                for (auto& rotation : pr) {
                    rotation.apply(clifford_ops);
                }
            },
            [](std::vector<PauliRotation>& left_pr, std::vector<PauliRotation>& right_pr) {
                left_pr.insert(left_pr.end(), right_pr.begin(), right_pr.end());
                right_pr.clear();
            },
            [](auto&, auto&) {
                throw std::logic_error("swap(variant,variant): unsupported swap pair");
            }},
        left,
        right);
}

void swap_along(std::vector<std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>>& tableau_vector,
                size_t from_idx,
                size_t to_idx) {
    if (from_idx >= tableau_vector.size() || to_idx >= tableau_vector.size()) {
        throw std::out_of_range("swap_along(indexed): from_idx/to_idx out of range");
    }
    if (from_idx == to_idx) {
        return;
    }
    if (from_idx < to_idx) {
        // target is on the left, swap through [from_idx + 1, to_idx]
        for (size_t k = from_idx + 1; k <= to_idx; ++k) {
            swap(tableau_vector[from_idx], tableau_vector[k]);
        }
    } else {
        // target is on the right, swap through [to_idx, from_idx - 1]
        for (size_t k = from_idx - 1; k >= to_idx; --k) {
            swap(tableau_vector[k], tableau_vector[from_idx]);
        }
    }

    auto moved = std::move(tableau_vector[from_idx]);
    tableau_vector.erase(tableau_vector.begin() + static_cast<std::ptrdiff_t>(from_idx));
    tableau_vector.insert(tableau_vector.begin() + static_cast<std::ptrdiff_t>(to_idx), std::move(moved));
}

void swap_along(Tableau& tableau, size_t from_idx, size_t to_idx) {
    std::vector<SubTableau> tableau_vector(tableau.begin(), tableau.end());
    swap_along(tableau_vector, from_idx, to_idx);
    tableau.erase(tableau.begin(), tableau.end());
    tableau.insert(tableau.begin(), tableau_vector.begin(), tableau_vector.end());
}

std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau> swap_along_test(
    std::vector<std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau>> const& tableau_vector,
    size_t from_idx,
    size_t to_idx) {
    if (from_idx >= tableau_vector.size() || to_idx >= tableau_vector.size()) {
        throw std::out_of_range("swap_along_test(indexed): from_idx/to_idx out of range");
    }

    SubTableau target = tableau_vector[from_idx];
    if (from_idx == to_idx) {
        return target;
    }

    if (from_idx < to_idx) {
        for (size_t k = from_idx + 1; k <= to_idx; ++k) {
            SubTableau neighbor = tableau_vector[k];
            swap(target, neighbor);
        }
    } else {
        for (size_t k = from_idx; k-- > to_idx;) {
            SubTableau neighbor = tableau_vector[k];
            swap(neighbor, target);
        }
    }

    return target;
}

std::variant<StabilizerTableau, std::vector<PauliRotation>, ClassicalControlTableau> swap_along_test(
    Tableau const& tableau,
    size_t from_idx,
    size_t to_idx) {
    std::vector<SubTableau> tableau_vector(tableau.begin(), tableau.end());
    return swap_along_test(tableau_vector, from_idx, to_idx);
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
    if (result_tableau.is_empty()) {
        return StabilizerTableau{clifford_block.n_qubits()};
    }
    assert(result_tableau.size() == 1 && std::holds_alternative<StabilizerTableau>(result_tableau.front()) &&
           "Result tableau should have only one element and be a stabilizer");

    return std::get<StabilizerTableau>(result_tableau.front());
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

void commute_through_pauli_rotation(StabilizerTableau& st, PauliRotation const& pauli_rotation, bool from_front) {
    size_t const n_qubits = st.n_qubits();
    assert(n_qubits == pauli_rotation.n_qubits() &&
           "StabilizerTableau and PauliRotation must have the same number of qubits");

    if (pauli_rotation.is_CZ()) {
        size_t a = n_qubits;
        size_t b = n_qubits;
        for (size_t i = 0; i < n_qubits; ++i) {
            if (pauli_rotation.is_z(i)) {
                if (a == n_qubits) {
                    a = i;
                } else if (b == n_qubits) {
                    b = i;
                } else {
                    spdlog::error(
                        "commute_through_pauli_rotation(ST, PR): CZ-marked rotation has more than two Z supports");
                    throw std::logic_error(
                        "commute_through_pauli_rotation(ST, PR): CZ-marked rotation has more than two Z supports");
                }
            }
        }
        if (a == n_qubits || b == n_qubits) {
            spdlog::error(
                "commute_through_pauli_rotation(ST, PR): CZ-marked rotation must have exactly two Z supports");
            throw std::logic_error(
                "commute_through_pauli_rotation(ST, PR): CZ-marked rotation must have exactly two Z supports");
        }
        StabilizerTableau cz_stabilizer(n_qubits);
        CliffordOperatorString cz_ops{{CliffordOperatorType::cz, {a, b}}};
        if (from_front) {
            cz_stabilizer.apply(adjoint(cz_ops));
        } else {
            cz_stabilizer.apply(cz_ops);
        }
        st = commutation_through_clifford(st, cz_stabilizer);
        return;
    }

    if (4 % pauli_rotation.phase().denominator() != 0) {
        spdlog::error(
            "commute_through_pauli_rotation(ST, PR): export error: non-CZ phase must be a multiple of pi/4");
        throw std::logic_error(
            "commute_through_pauli_rotation(ST, PR): export error: non-CZ phase must be a multiple of pi/4");
    }

    dvlab::Phase clifford_phase(0);
    dvlab::Phase t_phase(0);
    if (pauli_rotation.phase() == dvlab::Phase(0)) {
        return;
    } else if (pauli_rotation.phase() == dvlab::Phase(1, 4) ||
               pauli_rotation.phase() == dvlab::Phase(-1, 4)) {
        t_phase = pauli_rotation.phase();
    } else if (pauli_rotation.phase() == dvlab::Phase(1, 2) ||
               pauli_rotation.phase() == dvlab::Phase(-1, 2)) {
        clifford_phase = pauli_rotation.phase();
    } else if (pauli_rotation.phase() == dvlab::Phase(3, 4)) {
        clifford_phase = dvlab::Phase(1, 2);
        t_phase        = dvlab::Phase(1, 4);
    } else if (pauli_rotation.phase() == dvlab::Phase(-3, 4)) {
        clifford_phase = dvlab::Phase(-1, 2);
        t_phase        = dvlab::Phase(-1, 4);
    } else if (pauli_rotation.phase() == dvlab::Phase(1) ||
               pauli_rotation.phase() == dvlab::Phase(-1)) {
        clifford_phase = dvlab::Phase(1);
    } else {
        spdlog::error(
            "commute_through_pauli_rotation(ST, PR): export error: unsupported non-CZ phase {}; expected multiples of pi/4 with Clifford split",
            pauli_rotation.phase());
        throw std::logic_error(
            "commute_through_pauli_rotation(ST, PR): export error: unsupported non-CZ phase");
    }

    auto const [cxs, qubit] = pauli_to_CXT(pauli_rotation);

    StabilizerTableau cx_stabilizer(n_qubits);
    if (from_front) {
        cx_stabilizer.apply(adjoint(cxs));
    } else {
        cx_stabilizer.apply(cxs);
    }

    st = commutation_through_clifford(st, cx_stabilizer);

    if (clifford_phase != dvlab::Phase(0)) {
        CliffordOperatorString clifford_phase_ops;
        if (clifford_phase == dvlab::Phase(1, 2)) {
            clifford_phase_ops.emplace_back(CliffordOperatorType::s, std::array<size_t, 2>{qubit, 0});
        } else if (clifford_phase == dvlab::Phase(-1, 2)) {
            clifford_phase_ops.emplace_back(CliffordOperatorType::sdg, std::array<size_t, 2>{qubit, 0});
        } else {
            assert(clifford_phase == dvlab::Phase(1));
            clifford_phase_ops.emplace_back(CliffordOperatorType::z, std::array<size_t, 2>{qubit, 0});
        }
        StabilizerTableau clifford_phase_st(n_qubits);
        if (from_front) {
            clifford_phase_st.apply(adjoint(clifford_phase_ops));
        } else {
            clifford_phase_st.apply(clifford_phase_ops);
        }
        st = commutation_through_clifford(st, clifford_phase_st);
    }

    if (t_phase != dvlab::Phase(0)) {
        CliffordOperatorString ops = extract_clifford_operators(st);
        if (t_phase == dvlab::Phase(1, 4)) {
            if (from_front) {
                commute_through_Tdg(ops, qubit);
            } else {
                commute_through_T(ops, qubit);
            }
        } else {
            if (from_front) {
                commute_through_T(ops, qubit);
            } else {
                commute_through_Tdg(ops, qubit);
            }
        }

        st = reverse_n_prepend(ops, n_qubits);
    }

    st = commutation_through_clifford(st, adjoint(cx_stabilizer));
}


void commute_through_pauli_rotations(StabilizerTableau& st, std::vector<PauliRotation> const& pauli_rotations, bool from_front) {
    if (from_front) {
        for (auto it = pauli_rotations.rbegin(); it != pauli_rotations.rend(); ++it) {
            commute_through_pauli_rotation(st, *it, true);
            // spdlog::info(
            //     "passing PR: {}",
            //     it->to_bit_string());
            // commute_through_pauli_rotation(st, *it, true);
            // spdlog::info(clifford_ops_to_string(extract_clifford_operators(st)));
        }
    } else {
        for (auto const& pauli_rotation : pauli_rotations) {
            commute_through_pauli_rotation(st, pauli_rotation, false);
            // spdlog::info(
            //     "passing PR: {}",
            //     pauli_rotation.to_bit_string());
            // commute_through_pauli_rotation(st, pauli_rotation, false);
            // spdlog::info(clifford_ops_to_string(extract_clifford_operators(st)));
        }
    }
}

namespace {

void append_operand_with_cz_normalization(Tableau& tableau, SubTableau const& operand) {
    std::visit(
        dvlab::overloaded{
            [&tableau](StabilizerTableau const& st) {
                tableau.push_back(st);
            },
            [&tableau](ClassicalControlTableau const& cct) {
                tableau.push_back(cct.operations());
            },
            [&tableau](std::vector<PauliRotation> const& cols) {
                for (auto const& col : cols) {
                    if (!col.is_CZ()) {
                        tableau.push_back(std::vector<PauliRotation>{col});
                        continue;
                    }
                    size_t a = col.n_qubits();
                    size_t b = col.n_qubits();
                    for (size_t q = 0; q < col.n_qubits(); ++q) {
                        if (!col.is_z(q)) {
                            continue;
                        }
                        if (a == col.n_qubits()) {
                            a = q;
                        } else if (b == col.n_qubits()) {
                            b = q;
                        } else {
                            spdlog::error("test_eq: CZ column has more than two Z supports");
                            throw std::logic_error("test_eq: CZ column has more than two Z supports");
                        }
                    }
                    if (a == col.n_qubits() || b == col.n_qubits()) {
                        spdlog::error("test_eq: CZ column must have exactly two Z supports");
                        throw std::logic_error("test_eq: CZ column must have exactly two Z supports");
                    }
                    StabilizerTableau cz_st(col.n_qubits());
                    cz_st.apply(CliffordOperatorString{{CliffordOperatorType::cz, {a, b}}});
                    tableau.push_back(cz_st);
                }
            }},
        operand);
}

std::optional<size_t> operand_n_qubits(SubTableau const& operand) {
    return std::visit(
        dvlab::overloaded{
            [](StabilizerTableau const& st) -> std::optional<size_t> {
                return st.n_qubits();
            },
            [](ClassicalControlTableau const& cct) -> std::optional<size_t> {
                return cct.operations().n_qubits();
            },
            [](std::vector<PauliRotation> const& cols) -> std::optional<size_t> {
                if (cols.empty()) {
                    return std::nullopt;
                }
                return cols.front().n_qubits();
            }},
        operand);
}

bool test_eq(SubTableau const& old_a, SubTableau const& old_b, SubTableau const& b, bool reversed) {
    auto const n_old_a = operand_n_qubits(old_a);
    auto const n_old_b = operand_n_qubits(old_b);
    auto const n_b     = operand_n_qubits(b);
    size_t const n_qubits =
        n_old_a.value_or(n_old_b.value_or(n_b.value_or(0)));
    if (n_qubits == 0) {
        return true;
    }

    bool const b_matches_old_a = (b.index() == old_a.index());
    bool const b_matches_old_b = (b.index() == old_b.index());
    if (!b_matches_old_a && !b_matches_old_b) {
        spdlog::error("test_eq: type mismatch; b does not match old_a or old_b");
        throw std::logic_error("test_eq: type mismatch; b does not match old_a or old_b");
    }

    Tableau old_tableau{n_qubits};
    if (reversed) {
        append_operand_with_cz_normalization(old_tableau, old_b);
        append_operand_with_cz_normalization(old_tableau, old_a);
    } else {
        append_operand_with_cz_normalization(old_tableau, old_a);
        append_operand_with_cz_normalization(old_tableau, old_b);
    }

    Tableau new_tableau{n_qubits};
    if (reversed) {
        if (b_matches_old_a) {
            append_operand_with_cz_normalization(new_tableau, b);
            append_operand_with_cz_normalization(new_tableau, old_b);
        } else {
            append_operand_with_cz_normalization(new_tableau, old_a);
            append_operand_with_cz_normalization(new_tableau, b);
        }
    } else {
        if (b_matches_old_a) {
            append_operand_with_cz_normalization(new_tableau, old_b);
            append_operand_with_cz_normalization(new_tableau, b);
        } else {
            append_operand_with_cz_normalization(new_tableau, b);
            append_operand_with_cz_normalization(new_tableau, old_a);
        }
    }

    Tableau combined_tableau{n_qubits};
    for (auto const& subtableau : old_tableau) {
        combined_tableau.push_back(subtableau);
    }
    for (auto const& subtableau : adjoint(new_tableau)) {
        combined_tableau.push_back(subtableau);
    }

    full_optimize(combined_tableau);
    remove_identities(combined_tableau);
    return combined_tableau.is_empty();
}

}  // namespace

bool test_classical_equivalence(ClassicalControlTableau const& cct_old,
                                StabilizerTableau const& ta,
                                ClassicalControlTableau const& cct_new) {
    return test_eq(SubTableau{cct_old}, SubTableau{ta}, SubTableau{cct_new}, false);
}

bool test_classical_equivalence(ClassicalControlTableau const& cct_old,
                                std::vector<PauliRotation> const& ta,
                                ClassicalControlTableau const& cct_new) {
    return test_eq(SubTableau{cct_old}, SubTableau{ta}, SubTableau{cct_new}, false);
}

bool test_classical_equivalence(ClassicalControlTableau const& cct_old,
                                StabilizerTableau const& old_tableau,
                                StabilizerTableau const& new_tableau) {
    return test_eq(SubTableau{cct_old}, SubTableau{old_tableau}, SubTableau{new_tableau}, false);
}

bool test_classical_equivalence(ClassicalControlTableau const& cct_old,
                                std::vector<PauliRotation> const& old_tableau,
                                std::vector<PauliRotation> const& new_tableau) {
    if (old_tableau.empty() || new_tableau.empty()) {
        return old_tableau.empty() && new_tableau.empty();
    }
    return test_eq(SubTableau{cct_old}, SubTableau{old_tableau}, SubTableau{new_tableau}, false);
}

bool test_classical_equivalence_reverse(ClassicalControlTableau const& cct_old,
                                        StabilizerTableau const& ta,
                                        ClassicalControlTableau const& cct_new) {
    return test_eq(SubTableau{cct_old}, SubTableau{ta}, SubTableau{cct_new}, true);
}

bool test_classical_equivalence_reverse(ClassicalControlTableau const& cct_old,
                                        std::vector<PauliRotation> const& ta,
                                        ClassicalControlTableau const& cct_new) {
    return test_eq(SubTableau{cct_old}, SubTableau{ta}, SubTableau{cct_new}, true);
}

}  // namespace qsyn::experimental
