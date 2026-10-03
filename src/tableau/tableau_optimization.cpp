/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gsl/narrow>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <unordered_map>
#include <variant>
#include <vector>

#include "convert/qcir_to_tableau.hpp"
#include "qcir/qcir.hpp"
#include "tableau/classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

namespace qsyn {

namespace tableau {

namespace {

// Convert a stabilizer block that consists of {CX, S/Z/Sdg} into:
//   {cx-only StabilizerTableau}{PauliRotation tableau (phase columns)}
std::pair<StabilizerTableau, std::vector<PauliRotation>> stabilizers_to_pauli(StabilizerTableau const& st) {
    using COT = CliffordOperatorType;

    auto const ops = extract_clifford_operators(st);
    std::vector<PauliRotation> phase_columns;
    phase_columns.reserve(ops.size());
    CliffordOperatorString cx_ops;
    cx_ops.reserve(ops.size());

    for (auto const& op : ops) {
        auto const& [type, qubits] = op;
        if (type == COT::cx) {
            // Commute this CX to the left of all collected phase columns.
            for (auto& rot : phase_columns) {
                rot.apply(op);
            }
            cx_ops.push_back(op);
            continue;
        }

        if (type != COT::s && type != COT::z && type != COT::sdg) {
            spdlog::error("stabilizers_to_pauli: unexpected non-(CX/S/Z/Sdg) op {}", to_string(type));
            continue;
        }

        size_t const q = qubits[0];
        dvlab::Phase phase;
        if (type == COT::s) {
            phase = dvlab::Phase(1, 2);
        } else if (type == COT::z) {
            phase = dvlab::Phase(1, 1);
        } else {
            phase = dvlab::Phase(-1, 2);
        }
        phase_columns.push_back(PauliRotation::make_linear(st.n_qubits(), q, phase));
    }

    StabilizerTableau cx_only{st.n_qubits()};
    if (!cx_ops.empty()) {
        cx_only.apply(cx_ops);
    }
    return {std::move(cx_only), std::move(phase_columns)};
}

// Strict converter for Clifford sequences in the canonical form:
//   {CX-prefix}{S/Sdg/Z}{same CX-prefix}
// Repeats this pattern until exhaustion and converts each block to one PR column.
// Any deviation from the pattern is treated as an error.
std::vector<PauliRotation> stabilizers_to_pauli_identical_cx_conjugation(StabilizerTableau const& st) {
    using COT = CliffordOperatorType;

    auto const ops = extract_clifford_operators(
        st,
        HOptSynthesisStrategy{HOptSynthesisStrategy::Mode::staircase});

    auto const to_phase = [](CliffordOperatorType type) -> dvlab::Phase {
        switch (type) {
            case COT::s:
                return dvlab::Phase(1, 2);
            case COT::sdg:
                return dvlab::Phase(-1, 2);
            case COT::z:
                return dvlab::Phase(1, 1);
            default:
                throw std::logic_error("stabilizers_to_pauli_identical_cx_conjugation: non-phase gate");
        }
    };

    std::vector<PauliRotation> phase_columns;
    phase_columns.reserve(ops.size());

    size_t i = 0;
    while (i < ops.size()) {
        CliffordOperatorString prefix;
        while (i < ops.size() && std::get<0>(ops[i]) == COT::cx) {
            prefix.push_back(ops[i]);
            ++i;
        }

        if (i >= ops.size()) {
            spdlog::error(
                "stabilizers_to_pauli_identical_cx_conjugation: trailing CX-prefix without phase gate");
            throw std::logic_error(
                "stabilizers_to_pauli_identical_cx_conjugation: malformed block (missing phase gate)");
        }

        auto const& [phase_type, phase_qubits] = ops[i];
        if (phase_type != COT::s && phase_type != COT::sdg && phase_type != COT::z) {
            spdlog::error(
                "stabilizers_to_pauli_identical_cx_conjugation: expected S/Sdg/Z, got {}",
                to_string(phase_type));
            throw std::logic_error(
                "stabilizers_to_pauli_identical_cx_conjugation: malformed block (unexpected phase gate type)");
        }
        size_t const phase_qubit = phase_qubits[0];
        ++i;

        if (i + prefix.size() > ops.size()) {
            spdlog::error(
                "stabilizers_to_pauli_identical_cx_conjugation: missing suffix CXs for phase gate on q[{}]",
                phase_qubit);
            throw std::logic_error(
                "stabilizers_to_pauli_identical_cx_conjugation: malformed block (suffix too short)");
        }
        for (size_t k = 0; k < prefix.size(); ++k) {
            if (ops[i + k] != prefix[k]) {
                auto const& [expect_t, expect_q] = prefix[k];
                auto const& [actual_t, actual_q] = ops[i + k];
                spdlog::error(
                    "stabilizers_to_pauli_identical_cx_conjugation: suffix CX mismatch at k={} (expected {} q[{}],q[{}], got {} q[{}],q[{}])",
                    k,
                    to_string(expect_t),
                    expect_q[0],
                    expect_q[1],
                    to_string(actual_t),
                    actual_q[0],
                    actual_q[1]);
                throw std::logic_error(
                    "stabilizers_to_pauli_identical_cx_conjugation: malformed block (prefix/suffix mismatch)");
            }
        }
        i += prefix.size();

        auto rotation = PauliRotation::make_linear(st.n_qubits(), phase_qubit, to_phase(phase_type));
        for (auto const& cx : prefix) {
            rotation.apply(cx);
        }
        phase_columns.push_back(std::move(rotation));
    }

    return phase_columns;
}

}  // namespace

/**
 * @brief Perform the best-known optimization routine on the tableau. The strategy may change in the future.
 *
 * @param tableau
 */
void full_optimize(Tableau& tableau) {
    size_t non_clifford_count = SIZE_MAX;
    size_t count              = 0;
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        non_clifford_count = tableau.n_pauli_rotations();
        spdlog::debug("TMerge");
        merge_rotations(tableau);
        spdlog::debug("Internal-H-opt");
        minimize_internal_hadamards(tableau);
        spdlog::debug("Phase polynomial optimization");
        optimize_phase_polynomial(tableau, FastToddPhasePolynomialOptimizationStrategy{});
        spdlog::debug("{}: Reduced the number of non-Clifford gates from {} to {}.", ++count, non_clifford_count, tableau.n_pauli_rotations());
    } while (non_clifford_count > tableau.n_pauli_rotations());
    minimize_internal_hadamards(tableau);
}

namespace {
/**
 * @brief A view of the conjugation of a Clifford and a list of PauliRotations.
 *
 */
class ConjugationView : public PauliProductTrait<ConjugationView> {
public:
    ConjugationView(
        StabilizerTableau& clifford,
        std::vector<PauliRotation>& rotations,
        size_t upto) : _clifford{clifford}, _rotations{rotations}, _upto{upto} {}

    ConjugationView& h(size_t qubit) noexcept override {
        _clifford.get().h(qubit);
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].h(qubit);
        }
        return *this;
    }

    ConjugationView& s(size_t qubit) noexcept override {
        _clifford.get().s(qubit);
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].s(qubit);
        }
        return *this;
    }

    ConjugationView& cx(size_t control, size_t target) noexcept override {
        _clifford.get().cx(control, target);
        for (size_t i = 0; i < _upto; ++i) {
            _rotations.get()[i].cx(control, target);
        }
        return *this;
    }

private:
    std::reference_wrapper<StabilizerTableau> _clifford;
    std::reference_wrapper<std::vector<PauliRotation>> _rotations;
    size_t _upto;
};

}  // namespace

/**
 * @brief Pushing all the Clifford operators to the first sub-tableau and merging all the Pauli rotations.
 *
 * @param tableau
 */
void collapse(Tableau& tableau) {
    size_t const n_qubits = tableau.n_qubits();

    if (tableau.is_empty()) {
        return;
    }

    // prepend a stabilizer tableau to the front if the first sub-tableau is a list of PauliRotations
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{n_qubits});
    }

    if (tableau.size() <= 1) return;

    // make all clifford operators to be the identity except the first one
    auto clifford_string = CliffordOperatorString{};
    for (auto& subtableau : tableau | std::views::reverse) {
        std::visit(
            dvlab::overloaded(
                [&clifford_string](StabilizerTableau& st) {
                    st.apply(clifford_string);
                    clifford_string = extract_clifford_operators(st);
                },
                [&clifford_string](std::vector<PauliRotation>& pr) {
                    for (auto& rotation : pr) {
                        rotation.apply(clifford_string);
                    }
                },
                [&clifford_string](ClassicalControlTableau& cct) {
                    spdlog::error("Commute ClassicalControlTableau to the end first");
                    assert(false);
                }),
            subtableau);
    }

    // remove all clifford operators except the first one
    tableau.erase(
        std::remove_if(
            std::next(tableau.begin()),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                return std::holds_alternative<StabilizerTableau>(subtableau);
            }),
        tableau.end());

    if (tableau.size() == 1) {
        return;
    }

    // merge all rotations into the first rotation list
    auto& pauli_rotations = std::get<std::vector<PauliRotation>>(tableau[1]);

    for (auto& subtableau : tableau | std::views::drop(2)) {
        auto const& rotations = std::get<std::vector<PauliRotation>>(subtableau);
        pauli_rotations.insert(pauli_rotations.end(), rotations.begin(), rotations.end());
    }

    tableau.erase(dvlab::iterator::next(tableau.begin(), 2), tableau.end());

    DVLAB_ASSERT(tableau.size() == 2, "The tableau must have at most 2 sub-tableaux");
    DVLAB_ASSERT(std::holds_alternative<StabilizerTableau>(tableau.front()), "The first sub-tableau must be a StabilizerTableau");
    DVLAB_ASSERT(std::holds_alternative<std::vector<PauliRotation>>(tableau.back()), "The second sub-tableau must be a list of PauliRotations");
}

/**
 * @brief Collapse a classical tableau into canonical {ST}{PR}{PMC} form.
 *
 * @param tableau
 */
void collapse_with_classical(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }

    size_t const n_qubits = tableau.n_qubits();

    // Step 1: Normalize to strict zipper output {CCC & ST}{PR}{PMC}
    commute_and_merge_rotations(tableau);

    // Step 2: Extract PMCs from the end
    std::vector<SubTableau> pmc_ccts;
    while (!tableau.is_empty() && std::holds_alternative<ClassicalControlTableau>(tableau.back())) {
        auto* cct = std::get_if<ClassicalControlTableau>(&tableau.back());
        if (cct && cct->is_classical_control()) {
            pmc_ccts.insert(pmc_ccts.begin(), tableau.back());
            auto it = tableau.end();
            tableau.erase(std::prev(it), it);
        } else {
            // Found a CCC or non-CCT, stop extracting
            break;
        }
    }
    
    // Step 3: If tableau is empty after extracting PMCs, just return PMCs
    if (tableau.is_empty()) {
        tableau = Tableau{n_qubits};
        for (auto& cct : pmc_ccts) {
            tableau.push_back(cct);
        }
        return;
    }
    
    // Step 4: Convert CCCs to STs (treat CCCs as stabilizers for collapse)
    // Replace each CCC with its internal StabilizerTableau
    for (auto& subtableau : tableau) {
        if (auto* cct = std::get_if<ClassicalControlTableau>(&subtableau)) {
            if (cct->is_gadget()) {
                // Replace gadget block with its internal StabilizerTableau
                StabilizerTableau ccc_st = cct->operations();
                subtableau = std::move(ccc_st);
            }
        }
    }
    
    // Step 5: Apply normal collapse to the non-PMC part
    collapse(tableau);
    
    // Step 6: Add PMCs back at the end
    for (auto& cct : pmc_ccts) {
        tableau.push_back(std::move(cct));
    }
}



/**
 * @brief remove the Pauli rotations that evaluate to identity.
 *
 * @param rotations
 */
void remove_identities(std::vector<PauliRotation>& rotations) {
    rotations.erase(
        std::remove_if(
            rotations.begin(),
            rotations.end(),
            [](PauliRotation const& rotation) {
                if (rotation.is_CZ()) return false;  // CZ columns have phase 0 but are not identity
                return rotation.phase() == dvlab::Phase(0) ||
                       rotation.pauli_product().is_identity();
            }),
        rotations.end());
}

/**
 * @brief remove the tableau by removing the identity clifford operators and the identity Pauli rotations.
 *
 * @param tableau
 */
void remove_identities(Tableau& tableau) {
    // remove redundant pauli rotations
    std::ranges::for_each(tableau, [](SubTableau& subtableau) {
        if (auto pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            remove_identities(*pr);
        }
    });

    tableau.erase(
        std::remove_if(
            tableau.begin(),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                return dvlab::match(
                    subtableau,
                    [](StabilizerTableau const& subtableau) { return subtableau.is_identity(); },
                    [](std::vector<PauliRotation> const& subtableau) { return subtableau.empty(); },
                    [](ClassicalControlTableau const& cct) { return cct.operations().is_identity(); });
            }),
        tableau.end());

    // remove redundant clifford operators and merge adjacent pauli rotations if possible
    for (auto const& [this_tabl, next_tabl] : tl::views::adjacent<2>(tableau)) {
        auto this_clifford = std::get_if<StabilizerTableau>(&this_tabl);
        auto next_clifford = std::get_if<StabilizerTableau>(&next_tabl);
        if (!this_clifford || !next_clifford) continue;

        this_clifford->apply(extract_clifford_operators(*next_clifford));
        *next_clifford = StabilizerTableau{this_clifford->n_qubits()};
    }

    tableau.erase(
        std::remove_if(
            tableau.begin(),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                auto st = std::get_if<StabilizerTableau>(&subtableau);
                return st && st->is_identity();
            }),
        tableau.end());
}

/**
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *
 * @param rotations
 */
void merge_rotations(std::vector<PauliRotation>& rotations) {
    // merge two rotations if they are commutative and have the same underlying pauli product
    for (size_t i = 0; i < rotations.size(); ++i) {
        for (size_t j = i + 1; j < rotations.size(); ++j) {
            if (!is_commutative(rotations[i], rotations[j])) break;
            if (rotations[i].pauli_product() == rotations[j].pauli_product()) {
                rotations[i].phase() += rotations[j].phase();
                rotations[j].phase() = dvlab::Phase(0);
            }
        }
    }

    // remove all rotations with zero phase
    remove_identities(rotations);
}

/**
 * @brief Absorb the Clifford rotations in `rotations` into the `clifford` tableau.
 *
 * @param clifford
 * @param rotations
 */
void absorb_clifford_rotations(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations) {
    for (size_t const i : std::views::iota(0ul, rotations.size())) {
        if (rotations[i].phase() != dvlab::Phase(1, 2) &&
            rotations[i].phase() != dvlab::Phase(-1, 2) &&
            rotations[i].phase() != dvlab::Phase(1)) continue;

        auto conjugation_view = ConjugationView{clifford, rotations, i};

        auto [ops, qubit] = extract_clifford_operators(rotations[i]);

        conjugation_view.apply(ops);

        if (rotations[i].phase() == dvlab::Phase(1, 2)) {
            conjugation_view.s(qubit);
        } else if (rotations[i].phase() == dvlab::Phase(-1, 2)) {
            conjugation_view.sdg(qubit);
        } else {
            assert(rotations[i].phase() == dvlab::Phase(1));
            conjugation_view.z(qubit);
        }
        rotations[i].phase() = dvlab::Phase(0);

        adjoint_inplace(ops);

        conjugation_view.apply(ops);
    }

    // remove all rotations with zero phase
    remove_identities(rotations);
}

/**
 * @brief make all rotations proper by absorbing the Clifford effect into the initial Clifford operator.
 *
 * @param clifford
 * @param rotations
 */
void properize(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations) {
    merge_rotations(rotations);

    // checks if the phase is in the range [0, π/2)
    auto const is_proper_phase = [](dvlab::Phase const& phase) {
        auto const numerator   = phase.numerator();
        auto const denominator = phase.denominator();
        return 0 <= numerator && 2 * numerator < denominator;
    };

    // properize the rotations from the last to the first
    // the order is important because absorbing a rotation may change the phase of the preceding rotations
    for (size_t const i : std::views::iota(0ul, rotations.size()) | std::views::reverse) {
        if (rotations[i].is_CZ()) continue;  // CZ columns are already proper (phase 0), skip

        auto complement_phase = dvlab::Phase(0);
        while (!is_proper_phase(rotations[i].phase())) {
            rotations[i].phase() -= dvlab::Phase(1, 2);
            complement_phase += dvlab::Phase(1, 2);
        }
        if (complement_phase == dvlab::Phase(0)) continue;

        auto [ops, qubit] = extract_clifford_operators(rotations[i]);

        auto conjugation_view = ConjugationView{clifford, rotations, i};

        conjugation_view.apply(ops);

        if (complement_phase == dvlab::Phase(1, 2)) {
            conjugation_view.s(qubit);
        } else if (complement_phase == dvlab::Phase(-1, 2)) {
            conjugation_view.sdg(qubit);
        } else {
            assert(complement_phase == dvlab::Phase(1));
            conjugation_view.z(qubit);
        }

        adjoint_inplace(ops);

        conjugation_view.apply(ops);
    }

    remove_identities(rotations);
}

void properize(Tableau& tableau) {
    
    for (auto& subtableau : tableau) {
        if( std::holds_alternative<ClassicalControlTableau>(subtableau)) {
            assert(false && "Classical related circuits should not be using this method");
        }
    }
    if (tableau.is_empty()) {
        return;
    }
    // ensures that the first sub-tableau is a stabilizer tableau
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{tableau.n_qubits()});
    }

    // merge consecutive pauli rotation tableaux into one
    auto new_tableau = Tableau{tableau.n_qubits()};
    new_tableau.push_back(tableau.front());
    for (auto const& subtableau : tableau | std::views::drop(1)) {
        std::visit(
            dvlab::overloaded(
                [](StabilizerTableau& st1, StabilizerTableau const& st2) {
                    st1.apply(extract_clifford_operators(st2));
                },
                [&](StabilizerTableau& /* st1 */, std::vector<PauliRotation> const& pr2) {
                    new_tableau.push_back(pr2);
                },
                [&](StabilizerTableau& /* st1 */, ClassicalControlTableau const& /* cct2 */) {
                    assert(false && "Classical related circuits should not be using this method");
                },
                [&](std::vector<PauliRotation>& /* pr1 */, StabilizerTableau const& st2) {
                    new_tableau.push_back(st2);
                },
                [&](std::vector<PauliRotation>& pr1, std::vector<PauliRotation> const& pr2) {
                    pr1.insert(pr1.end(), pr2.begin(), pr2.end());
                },
                [&](std::vector<PauliRotation>& /* pr1 */, ClassicalControlTableau const& /* cct2 */) {
                    assert(false && "Classical related circuits should not be using this method");
                },
                [&](ClassicalControlTableau& /* cct1 */, StabilizerTableau const& /* st2 */) {
                    assert(false && "Classical related circuits should not be using this method");
                },
                [&](ClassicalControlTableau& /* cct1 */, std::vector<PauliRotation> const& /* pr2 */) {
                    assert(false && "Classical related circuits should not be using this method");
                },
                [&](ClassicalControlTableau& /* cct1 */, ClassicalControlTableau const& /* cct2 */) {
                    assert(false && "Classical related circuits should not be using this method");
                }),
            new_tableau.back(), subtableau);
    }

    tableau = new_tableau;

    auto clifford = std::ref(std::get<StabilizerTableau>(tableau.front()));
    for (auto& subtableau : tableau | std::views::drop(1)) {
        std::visit(
            dvlab::overloaded(
                [&clifford](StabilizerTableau& st) {
                    clifford = std::ref(st);
                },
                [&clifford](std::vector<PauliRotation>& pr) {
                    properize(clifford.get(), pr);
                },
                [](ClassicalControlTableau& /* cct */) {
                    assert(false && "Classical related circuits should not be using this method");
                }),
            subtableau);
    }

    remove_identities(tableau);
}

/**
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *        If a rotation becomes Clifford, absorb it into the initial Clifford operator.
 *        This algorithm is inspired by the paper [[1903.12456] Optimizing T gates in Clifford+T circuit as $π/4$ rotations around Paulis](https://arxiv.org/abs/1903.12456)
 *
 * @param clifford
 * @param rotations
 */
void merge_rotations(Tableau& tableau) {
    collapse(tableau);

    if (tableau.size() <= 1) {
        return;
    }

    auto& clifford   = std::get<StabilizerTableau>(tableau.front());
    auto& rotations  = std::get<std::vector<PauliRotation>>(tableau.back());
    auto n_rotations = SIZE_MAX;
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        n_rotations = rotations.size();
        merge_rotations(rotations);
        absorb_clifford_rotations(clifford, rotations);
    } while (rotations.size() < n_rotations);
}

// phase polynomial optimization

/**
 * @brief Reduce the number of terms for the phase polynomial. If the polynomial is not a phase polynomial, do nothing.
 *
 * @param polynomial
 * @param strategy
 */
void optimize_phase_polynomial(StabilizerTableau& clifford, std::vector<PauliRotation>& polynomial, PhasePolynomialOptimizationStrategy const& strategy) {
    if (!is_phase_polynomial(polynomial)) {
        return;
    }



    std::tie(clifford, polynomial) = strategy.optimize(clifford, polynomial);
}

/**
 * @brief Reduce the number of terms for all phase polynomials in the tableau.
 *
 * @param tableau
 * @param strategy
 */
void optimize_phase_polynomial(Tableau& tableau, PhasePolynomialOptimizationStrategy const& strategy) {
    if (tableau.is_empty()) {
        return;
    }

    // if the first sub-tableau is a list of PauliRotations, prepend a stabilizer tableau to the front
    if (std::holds_alternative<std::vector<PauliRotation>>(tableau.front())) {
        tableau.insert(tableau.begin(), StabilizerTableau{tableau.n_qubits()});
    }

    auto last_clifford = std::ref(std::get<StabilizerTableau>(tableau.front()));
    for (auto& subtableau : tableau) {
        if (auto pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            optimize_phase_polynomial(last_clifford.get(), *pr, strategy);
        } else if (auto cct = std::get_if<ClassicalControlTableau>(&subtableau)) {
            break;
        }
        else {
            last_clifford = std::get<StabilizerTableau>(subtableau);
        }
    }
    remove_identities(tableau);
}


/**
 * @brief Reduce the number of terms for all phase polynomials in the tableau.
 *
 * @param tableau
 * @param strategy
 */
void optimize_phase_polynomial_with_classical(Tableau& tableau,
                                              PhasePolynomialOptimizationStrategy const& strategy,
                                              size_t* fasttodd_t_count) {
    // One or more PR blocks: always run phase-polynomial optimization (e.g. FastTODD) on the *last* PR block.
    // With a single PR, that is the only block; with two (PR1, PR2), that is PR2.
    std::vector<size_t> pr_indices;
    pr_indices.reserve(4);
    for (size_t i = 0; i < tableau.size(); ++i) {
        if (std::holds_alternative<std::vector<PauliRotation>>(tableau[i])) {
            pr_indices.push_back(i);
        }
    }
    if (pr_indices.empty()) {
        remove_identities(tableau);
        return;
    }

    size_t const target_pr_index = pr_indices.back();

    if (target_pr_index >= tableau.size()) {
        remove_identities(tableau);
        return;
    }

    {
        // Ensure there is a stabilizer tableau right before the target PR.
        // If we insert before the PR, the PR index shifts by +1 (PR1,PR2 -> PR1,ST,PR2).
        size_t pr_row = target_pr_index;
        if (pr_row == 0 || !std::holds_alternative<StabilizerTableau>(tableau[pr_row - 1])) {
            tableau.insert(tableau.begin() + pr_row, StabilizerTableau{tableau.n_qubits()});
            ++pr_row;
        }

        auto& clifford_ref = std::get<StabilizerTableau>(tableau[pr_row - 1]);
        auto& pr           = std::get<std::vector<PauliRotation>>(tableau[pr_row]);
        optimize_phase_polynomial(clifford_ref, pr, strategy);
        if (fasttodd_t_count != nullptr) {
            *fasttodd_t_count = pr.size();
        }
        auto phase_columns = stabilizers_to_pauli_identical_cx_conjugation(clifford_ref);
        pr.insert(pr.end(), phase_columns.begin(), phase_columns.end());
        clifford_ref = StabilizerTableau{clifford_ref.n_qubits()};

    }
    remove_identities(tableau);

}

namespace {

// Prepare circuit structure for T-optimization:
//   - Commute/merge STs through CCCs up to PR2
//   - Rewrite pending stabilizers into {PR1, CXs}
//   - Output canonical form: {ST0, CCCs, PR1, PR2, CXs, PMCs, ST_back}
void properize_for_t_optimization(Tableau& tableau) {
    if (tableau.is_empty()) return;
    if (!std::holds_alternative<StabilizerTableau>(tableau[0])) return;

    auto const get_subtableau_kind = [&](SubTableau const& subtableau) -> std::string_view {
        if (std::holds_alternative<StabilizerTableau>(subtableau)) return "StabilizerTableau";
        if (std::holds_alternative<std::vector<PauliRotation>>(subtableau)) return "PauliRotationTableau";
        if (std::holds_alternative<ClassicalControlTableau>(subtableau)) return "ClassicalControlTableau";
        return "Unknown";
    };

    size_t pr2_idx    = tableau.size();
    StabilizerTableau pending_st{tableau.n_qubits()};
    bool has_pending = false;

    std::vector<SubTableau> new_prefix;
    new_prefix.reserve(tableau.size());
    new_prefix.push_back(std::move(tableau[0]));  // ST0

    for (size_t j = 1; j < tableau.size(); ++j) {
        if (std::holds_alternative<std::vector<PauliRotation>>(tableau[j])) {
            pr2_idx = j;
            break;
        }
        if (auto* st = std::get_if<StabilizerTableau>(&tableau[j])) {
            pending_st.apply(extract_clifford_operators(*st));
            has_pending = true;
            continue;
        }
        if (auto* cct = std::get_if<ClassicalControlTableau>(&tableau[j]);
            cct != nullptr && cct->is_gadget()) {
            if (has_pending) {
                swap(*cct, pending_st);
            }
            new_prefix.push_back(std::move(*cct));
            continue;
        }
        spdlog::error("properize_for_t_optimization: invalid element at index {} before PR2 (got {})",
                      j, get_subtableau_kind(tableau[j]));
        return;
    }

    if (pr2_idx == tableau.size()) {
        spdlog::error("properize_for_t_optimization: no PR2 block found");
        return;
    }

    std::vector<PauliRotation> pr2 = std::move(std::get<std::vector<PauliRotation>>(tableau[pr2_idx]));
    std::vector<PauliRotation> pr1;
    std::optional<StabilizerTableau> cx_only;

    if (has_pending) {
        auto [st_prime, phase_cols] = stabilizers_to_pauli(pending_st);  // CXs, PR1
        cx_only = std::move(st_prime);
        pr1     = std::move(phase_cols);

        // Move CXs to after {PR1, PR2} by conjugating PR1 and PR2 with adjoint(CXs).
        auto const st_ops  = extract_clifford_operators(*cx_only);
        auto const adj_ops = adjoint(st_ops);
        for (auto& rot : pr1) rot.apply(adj_ops);
        for (auto& rot : pr2) rot.apply(adj_ops);
    }

    std::vector<SubTableau> new_subs;
    new_subs.reserve(tableau.size() + 2);
    new_subs.insert(new_subs.end(),
                    std::make_move_iterator(new_prefix.begin()),
                    std::make_move_iterator(new_prefix.end()));
    new_subs.push_back(std::move(pr1));
    new_subs.push_back(std::move(pr2));
    if (cx_only.has_value()) {
        new_subs.push_back(std::move(*cx_only));
    }
    for (size_t j = pr2_idx + 1; j < tableau.size(); ++j) {
        new_subs.push_back(std::move(tableau[j]));
    }

    tableau.erase(tableau.begin(), tableau.end());
    for (auto& sub : new_subs) {
        tableau.push_back(std::move(sub));
    }
    remove_identities(tableau);
}

/**
 * @brief Build a debug tableau by reverse-commuting each PMC to its matching gadget.
 */
[[nodiscard]] Tableau reverse_commute_pmcs_to_gadgets_for_test(Tableau const& tableau) {
    Tableau new_tableau = tableau;
    if (new_tableau.size() < 2) {
        return new_tableau;
    }

    // Collect PMC ancilla IDs first, then locate each PMC dynamically during moves.
    std::vector<size_t> pmc_ancillae;
    pmc_ancillae.reserve(new_tableau.size());
    for (auto const& sub : new_tableau) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&sub);
        if (cct != nullptr && cct->is_classical_control()) {
            pmc_ancillae.push_back(cct->ancilla_qubit());
        }
    }

    auto const find_pmc_index = [&](size_t ancilla) -> std::optional<size_t> {
        for (size_t i = 0; i < new_tableau.size(); ++i) {
            auto const* cct = std::get_if<ClassicalControlTableau>(&new_tableau[i]);
            if (cct != nullptr && cct->is_classical_control() && cct->ancilla_qubit() == ancilla) {
                return i;
            }
        }
        return std::nullopt;
    };

    auto const find_gadget_index = [&](size_t ancilla) -> std::optional<size_t> {
        for (size_t i = 0; i < new_tableau.size(); ++i) {
            auto const* cct = std::get_if<ClassicalControlTableau>(&new_tableau[i]);
            if (cct != nullptr && cct->is_gadget() && cct->ancilla_qubit() == ancilla) {
                return i;
            }
        }
        return std::nullopt;
    };

    for (size_t const ancilla : pmc_ancillae) {
        auto pmc_idx_opt    = find_pmc_index(ancilla);
        auto gadget_idx_opt = find_gadget_index(ancilla);
        if (!pmc_idx_opt.has_value() || !gadget_idx_opt.has_value()) {
            spdlog::warn(
                "reverse_commute_pmcs_to_gadgets_for_test: missing pair for ancilla {}",
                ancilla);
            continue;
        }
        size_t const pmc_idx    = *pmc_idx_opt;
        size_t const gadget_idx = *gadget_idx_opt;
        if (pmc_idx <= gadget_idx + 1) {
            continue;
        }

        // Commute+move PMC to right after its gadget counterpart.
        swap_along(new_tableau, pmc_idx, gadget_idx + 1);
    }

    remove_identities(new_tableau);
    return new_tableau;
}

}  // namespace

// Structural checker/merger used by degadgetization:
// Check circuit is under the form {ST, (CCCs|intermediate STs)*, PR1, (optional PR2), CXs, PMCs, ST}.
// If PR2 exists, merge {PR1, PR2}; otherwise PR1 alone is fine.
CircuitStructureInfo properize_for_degadgetization(Tableau& tableau) {
    CircuitStructureInfo info = {0, 0, false};

    if (tableau.is_empty()) {
        spdlog::warn("Tableau is empty");
        return info;
    }

    auto const get_subtableau_kind = [&](SubTableau const& subtableau) -> std::string_view {
        if (std::holds_alternative<StabilizerTableau>(subtableau)) return "StabilizerTableau";
        if (std::holds_alternative<std::vector<PauliRotation>>(subtableau)) return "PauliRotationTableau";
        if (std::holds_alternative<ClassicalControlTableau>(subtableau)) return "ClassicalControlTableau";
        return "Unknown";
    };

    // 1) Check that CCTs start from index 1 (index 0 must be ST).
    if (!std::holds_alternative<StabilizerTableau>(tableau[0])) {
        spdlog::error(
            "properize_for_degadgetization: expected front StabilizerTableau at index 0, got {}",
            get_subtableau_kind(tableau[0]));
        return info;
    }

    // Expected structure:
    //   {ST0, (CCCs|intermediate STs)..., PR1, (optional PR2), CXs(ST), PMCs..., ST_back}
    size_t idx = 1;
    size_t ccc_count = 0;
    while (idx < tableau.size()) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct != nullptr && cct->is_gadget()) {
            ++ccc_count;
            ++idx;
            continue;
        }
        if (std::holds_alternative<StabilizerTableau>(tableau[idx])) {
            ++idx;
            continue;
        }
        break;
    }

    if (idx >= tableau.size() || !std::holds_alternative<std::vector<PauliRotation>>(tableau[idx])) {
        spdlog::error("properize_for_degadgetization: expected PR1 after gadget/intermediate-Clifford prefix at index {}, got {}",
                      idx, idx < tableau.size() ? get_subtableau_kind(tableau[idx]) : "OutOfRange");
        return info;
    }
    size_t const pr1_idx = idx++;

    bool has_pr2 = false;
    size_t pr2_idx = std::numeric_limits<size_t>::max();
    if (idx < tableau.size() && std::holds_alternative<std::vector<PauliRotation>>(tableau[idx])) {
        has_pr2 = true;
        pr2_idx = idx++;
    }

    // Optional CX-only stabilizer block. Some flows have no explicit CX block here:
    //   {ST0, CCCs, PR, PMCs, ST_back}
    bool has_cx_block = false;
    if (idx < tableau.size() && std::holds_alternative<StabilizerTableau>(tableau[idx])) {
        has_cx_block = true;
        ++idx;
    }

    // Expect exactly ccc_count PMCs next, then a final back ST.
    for (size_t k = 0; k < ccc_count; ++k) {
        if (idx >= tableau.size()) {
            spdlog::error("properize_for_degadgetization: expected {} PMCs after PR{}, but tableau ends early",
                          ccc_count, has_cx_block ? "+CX block" : "");
            return info;
        }
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct == nullptr || !cct->is_classical_control()) {
            spdlog::error("properize_for_degadgetization: expected PMC at index {}, got {}",
                          idx, get_subtableau_kind(tableau[idx]));
            return info;
        }
        ++idx;
    }

    if (idx >= tableau.size() || !std::holds_alternative<StabilizerTableau>(tableau[idx]) || idx != tableau.size() - 1) {
        spdlog::error("properize_for_degadgetization: expected final back StabilizerTableau at last index {}, got {}",
                      tableau.size() - 1, idx < tableau.size() ? get_subtableau_kind(tableau[idx]) : "OutOfRange");
        return info;
    }

    auto& pr1 = std::get<std::vector<PauliRotation>>(tableau[pr1_idx]);
    if (has_pr2) {
        // Merge PR1 into PR2 (append PR2 to PR1, then keep a single PR block).
        auto& pr2 = std::get<std::vector<PauliRotation>>(tableau[pr2_idx]);
        pr1.insert(pr1.end(),
                   std::make_move_iterator(pr2.begin()),
                   std::make_move_iterator(pr2.end()));

        // Remove the now-empty PR2 block.
        tableau.erase(tableau.begin() + static_cast<std::ptrdiff_t>(pr2_idx));
    }

    info.ccc_count       = ccc_count;
    info.pr_column_count = pr1.size();
    info.is_valid        = true;

    remove_identities(tableau);
    return info;
}

CircuitStructureInfo inspect_degadgetization_structure(Tableau const& tableau) {
    auto copy = tableau;
    return properize_for_degadgetization(copy);
}

std::string TableauPreprocessConfig::id() const {
    char const decomp_ch = decomp == qcir::CcDecomposition::Rust ? 'r' : 'c';
    return fmt::format(
        "mr{}_pr{}_{}",
        merge_rotations ? 1 : 0,
        properize ? 1 : 0,
        decomp_ch);
}

std::vector<TableauPreprocessConfig> all_tableau_preprocess_configs() {
    std::vector<TableauPreprocessConfig> configs;
    configs.reserve(8);
    for (bool const mr : {false, true}) {
        for (bool const pr : {false, true}) {
            for (qcir::CcDecomposition const decomp :
                 {qcir::CcDecomposition::Cpp, qcir::CcDecomposition::Rust}) {
                configs.push_back(TableauPreprocessConfig{
                    .decomp = decomp,
                    .merge_rotations = mr,
                    .properize = pr,
                });
            }
        }
    }
    return configs;
}

std::optional<Tableau> prepare_gadgetized_tableau(qcir::QCir const& source,
                                                  TableauPreprocessConfig const& cfg) {
    auto const basic = qcir::to_basic_gates(source, cfg.decomp);
    if (!basic.has_value()) {
        spdlog::error("tie search: to_basic_gates failed for config {}", cfg.id());
        return std::nullopt;
    }
    auto tab = to_tableau(*basic);
    if (!tab.has_value()) {
        spdlog::error("tie search: to_tableau failed for config {}", cfg.id());
        return std::nullopt;
    }
    minimize_internal_hadamards_n_gadgetize(*tab, cfg.merge_rotations, cfg.properize);
    return tab;
}

namespace {

bool tie_search_enabled_from_env() {
    if (char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH")) {
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y';
    }
    return false;
}

size_t read_size_t_env(char const* key, size_t fallback) {
    if (char const* value = std::getenv(key)) {
        try {
            return static_cast<size_t>(std::stoull(value));
        } catch (...) {
            spdlog::warn("{}='{}' invalid, fallback={}", key, value, fallback);
        }
    }
    return fallback;
}

size_t auto_tie_search_patience(size_t ancilla) {
    return std::max<size_t>(60, (ancilla * 5 + 1) / 2);
}

bool read_truthy_env(char const* key) {
    if (char const* value = std::getenv(key)) {
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' ||
               value[0] == 'T';
    }
    return false;
}

/** PHASE2 env: "previous_best", "all_random", or "previous_best,all_random" (default). */
struct Phase2Kinds {
    bool previous_best = true;
    bool all_random    = true;
};

Phase2Kinds read_phase2_kinds_env() {
    char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH_PHASE2");
    if (value == nullptr || value[0] == '\0') {
        return {};
    }
    std::string const s{value};
    if (s == "none" || s == "off" || s == "0" || s == "phase1") {
        return Phase2Kinds{.previous_best = false, .all_random = false};
    }
    Phase2Kinds out{.previous_best = false, .all_random = false};
    if (s.find("previous_best") != std::string::npos || s == "B" || s == "b") {
        out.previous_best = true;
    }
    if (s.find("all_random") != std::string::npos || s == "C" || s == "c") {
        out.all_random = true;
    }
    // Allow "BC" / "B,C" short forms without matching letters inside longer tokens.
    if (s == "BC" || s == "B,C" || s == "b,c") {
        out.previous_best = true;
        out.all_random    = true;
    }
    if (!out.previous_best && !out.all_random) {
        spdlog::warn(
            "QSYN_FASTTODD_TIE_SEARCH_PHASE2='{}' unrecognized; using previous_best,all_random",
            value);
        return {};
    }
    return out;
}

/** Phase-2 path family: previous_best ≈ old B; all_random ≈ old C. */
enum class TiePathKind : std::uint8_t { all_random, previous_best };

char const* tie_path_kind_label(TiePathKind kind) {
    switch (kind) {
        case TiePathKind::all_random:
            return "all_random";
        case TiePathKind::previous_best:
            return "previous_best";
    }
    return "unknown";
}

struct TieSearchOutcome {
    bool                 ok = false;
    bool                 early_unsat = false;
    size_t               t_count = 0;
    size_t               sat_width = 0;
    size_t               pre_t = 0;  // Pauli-term count before FastTODD (size proxy)
    size_t               pre_a = 0;  // ancilla count before FastTODD
    Tableau              tableau{0};
    std::string          preprocess_id;
    FastToddTieRunReport report;
    TiePathKind          path_kind = TiePathKind::all_random;
};

struct TieStepRef {
    FastToddTieLevel level      = FastToddTieLevel::tohpe;
    size_t           step_index = 0;
    size_t           chosen_index = 0;
};

struct PreprocessTieCandidate {
    TableauPreprocessConfig config;
    Tableau                 gadgetized;
    TieSearchOutcome        baseline;
};

std::optional<size_t> solve_sat_min_width(Tableau const& tableau,
                                          std::optional<size_t> start_width,
                                          bool stop_if_start_unsat,
                                          bool* start_unsat,
                                          bool quiet) {
    if (start_unsat != nullptr) {
        *start_unsat = false;
    }
    SatSignatureExport const sig = compute_sat_signature_blocks(tableau);
    AncillaSmtInstance inst = build_ancilla_smt_instance(sig);
    AncillaScheduleResult const schedule = solve_ancilla_schedule(
        inst, AncillaScheduleSolveOptions{
                  .start_width = start_width,
                  .stop_if_start_unsat = stop_if_start_unsat,
                  .linear_search_below_start = start_width.has_value() && stop_if_start_unsat,
                  .quiet = quiet,
              });
    if (!schedule.ok) {
        if (start_width.has_value() && stop_if_start_unsat && start_unsat != nullptr) {
            *start_unsat = true;
        }
        return std::nullopt;
    }
    return schedule.width_w;
}

/**
 * A/T = A / (T_no_gadget - T_with_gadget), +inf if ΔT <= 0.
 *
 * T_no_gadget   = minimized T without H-gadgetization (full_optimize).
 * T_with_gadget = FastTODD T with H-gadgets, before degadgetization/SMT.
 * (Thesis table calls T_no_gadget "Block.T".)
 */
double at_score(size_t t_no_gadget, size_t t_with_gadget, size_t ancilla) {
    if (t_with_gadget >= t_no_gadget) {
        return std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(ancilla) /
           static_cast<double>(t_no_gadget - t_with_gadget);
}

/** Minimized T without H-gadgetization. Override: QSYN_TIE_SEARCH_BLOCK_T. */
size_t compute_t_without_gadget(qcir::QCir const& source) {
    if (char const* value = std::getenv("QSYN_TIE_SEARCH_BLOCK_T")) {
        try {
            return static_cast<size_t>(std::stoull(value));
        } catch (...) {
            spdlog::warn("QSYN_TIE_SEARCH_BLOCK_T='{}' invalid, computing full_optimize", value);
        }
    }
    size_t best = std::numeric_limits<size_t>::max();
    for (qcir::CcDecomposition const decomp :
         {qcir::CcDecomposition::Cpp, qcir::CcDecomposition::Rust}) {
        auto const basic = qcir::to_basic_gates(source, decomp);
        if (!basic.has_value()) {
            continue;
        }
        auto tab = to_tableau(*basic);
        if (!tab.has_value()) {
            continue;
        }
        full_optimize(*tab);  // tmerge + hopt + FastTODD; no H-gadgetize
        best = std::min(best, tab->n_pauli_rotations());
    }
    return best;
}

/**
 * Max peak ancilla A such that A/(T_ng - trial_T) <= best_A/(T_ng - best_T).
 * Integer form: A <= (best_A * ΔT_trial) / ΔT_best.
 */
std::optional<size_t> max_a_for_equal_or_better_at(size_t t_no_gadget,
                                                   size_t best_t,
                                                   size_t best_a,
                                                   size_t trial_t) {
    if (trial_t >= t_no_gadget || best_t >= t_no_gadget) {
        return std::nullopt;
    }
    size_t const d_best  = t_no_gadget - best_t;
    size_t const d_trial = t_no_gadget - trial_t;
    return (best_a * d_trial) / d_best;
}

bool is_improving_at(TieSearchOutcome const& trial, TieSearchOutcome const& best, size_t block_t) {
    double const trial_at = at_score(block_t, trial.t_count, trial.sat_width);
    double const best_at  = at_score(block_t, best.t_count, best.sat_width);
    if (trial_at < best_at) {
        return true;
    }
    if (trial_at > best_at) {
        return false;
    }
    // Equal A/T: prefer lower T, then lower A.
    if (trial.t_count < best.t_count) {
        return true;
    }
    if (trial.t_count == best.t_count && trial.sat_width < best.sat_width) {
        return true;
    }
    return false;
}

bool apply_sat_width(TieSearchOutcome& outcome,
                     std::optional<size_t> start_width,
                     bool stop_if_start_unsat) {
    if (!has_gadget_ancillae(outcome.tableau)) {
        outcome.sat_width = outcome.tableau.n_ancilla();
        outcome.ok        = true;
        return true;
    }
    bool start_unsat = false;
    auto const sat_width =
        solve_sat_min_width(outcome.tableau, start_width, stop_if_start_unsat, &start_unsat, true);
    if (!sat_width.has_value()) {
        outcome.early_unsat = start_unsat;
        outcome.ok          = false;
        return false;
    }
    outcome.sat_width = *sat_width;
    outcome.ok        = true;
    return true;
}

std::vector<TieStepRef> collect_tie_steps(FastToddTieRunReport const& report) {
    std::vector<TieStepRef> steps;
    auto push_multi = [&](FastToddTieLevel level, std::vector<FastToddTieStepDecision> const& decs) {
        for (auto const& d : decs) {
            if (d.tie_count > 1) {
                steps.push_back(TieStepRef{
                    .level = level, .step_index = d.step_index, .chosen_index = d.chosen_index});
            }
        }
    };
    push_multi(FastToddTieLevel::tohpe, report.tohpe_decisions);
    push_multi(FastToddTieLevel::outer, report.outer_decisions);
    return steps;
}

void force_tie_prefix(FastToddTieControl& control,
                      std::vector<TieStepRef> const& tie_steps,
                      size_t prefix_len) {
    control.forced_tohpe_choice_by_step.clear();
    control.forced_outer_choice_by_step.clear();
    prefix_len = std::min(prefix_len, tie_steps.size());
    for (size_t i = 0; i < prefix_len; ++i) {
        auto const& s = tie_steps[i];
        if (s.level == FastToddTieLevel::tohpe) {
            control.forced_tohpe_choice_by_step[s.step_index] = s.chosen_index;
        } else {
            control.forced_outer_choice_by_step[s.step_index] = s.chosen_index;
        }
    }
}

TieSearchOutcome run_det_fasttodd_only(Tableau const& gadgetized_tableau, std::string preprocess_id) {
    TieSearchOutcome outcome{
        .ok = false,
        .pre_t = gadgetized_tableau.n_pauli_rotations(),
        .pre_a = gadgetized_tableau.n_ancilla(),
        .tableau = gadgetized_tableau,
        .preprocess_id = std::move(preprocess_id),
        .path_kind = TiePathKind::all_random};
    // Capture original-path decisions for later previous_best forcing.
    FastToddTieControl control;
    control.enabled = true;
    control.mode    = FastToddTieSearchMode::original;
    set_fasttodd_tie_control(control);
    optimize_phase_polynomial_with_classical(
        outcome.tableau, FastToddPhasePolynomialOptimizationStrategy{}, &outcome.t_count);
    if (auto report = consume_fasttodd_tie_run_report()) {
        outcome.report = *report;
        // Path-tree size along this trajectory: ∏ tie_count over multi-way steps.
        unsigned __int128 prod = 1;
        size_t multi           = 0;
        std::string factors;
        auto acc = [&](char const* level, std::vector<FastToddTieStepDecision> const& decs) {
            for (auto const& d : decs) {
                if (d.tie_count <= 1) {
                    continue;
                }
                ++multi;
                prod *= d.tie_count;
                if (!factors.empty()) {
                    factors += " * ";
                }
                factors += fmt::format("{}[{}]={}", level, d.step_index, d.tie_count);
            }
        };
        acc("tohpe", report->tohpe_decisions);
        acc("outer", report->outer_decisions);
        // print __int128 decimal
        std::string prod_s;
        if (prod == 0) {
            prod_s = "0";
        } else {
            unsigned __int128 x = prod;
            while (x > 0) {
                prod_s.push_back(char('0' + static_cast<int>(x % 10)));
                x /= 10;
            }
            std::reverse(prod_s.begin(), prod_s.end());
        }
        spdlog::debug(
            "tie search path-tree: config={} multi_ties={} factors=({}) product={}",
            outcome.preprocess_id,
            multi,
            factors.empty() ? "1" : factors,
            prod_s);
    }
    set_fasttodd_tie_control(std::nullopt);
    return outcome;
}

TieSearchOutcome run_tie_search_trial_at(Tableau const& base_tableau,
                                         std::string const& preprocess_id,
                                         size_t block_t,
                                         size_t best_t,
                                         size_t best_a,
                                         size_t phase1_t_limit,
                                         std::uint64_t seed,
                                         TiePathKind path_kind,
                                         FastToddTieRunReport const* best_report,
                                         size_t force_prefix_len) {
    TieSearchOutcome outcome{
        .ok = false,
        .pre_t = base_tableau.n_pauli_rotations(),
        .pre_a = base_tableau.n_ancilla(),
        .tableau = base_tableau,
        .preprocess_id = preprocess_id,
        .path_kind = path_kind};
    FastToddTieControl control;
    control.enabled     = true;
    control.mode        = FastToddTieSearchMode::all_random;
    control.random_seed = seed;
    if (path_kind == TiePathKind::previous_best && best_report != nullptr) {
        auto const tie_steps = collect_tie_steps(*best_report);
        force_tie_prefix(control, tie_steps, force_prefix_len);
    }

    set_fasttodd_tie_control(control);
    optimize_phase_polynomial_with_classical(outcome.tableau, FastToddPhasePolynomialOptimizationStrategy{});
    auto report = consume_fasttodd_tie_run_report();
    set_fasttodd_tie_control(std::nullopt);

    if (!report.has_value()) {
        spdlog::error("tie search: missing FastTODD tie report");
        return outcome;
    }
    outcome.report  = *report;
    outcome.t_count = outcome.report.final_term_count > 0
        ? outcome.report.final_term_count
        : outcome.tableau.n_pauli_rotations();

    // Keep the phase-1 minimum as a fixed ceiling, even if later T improves.
    if (outcome.t_count > phase1_t_limit) {
        spdlog::debug("tie search shot: kind={} config={} T={} skip SMT (phase1_T_limit={})",
                     tie_path_kind_label(path_kind), preprocess_id, outcome.t_count, phase1_t_limit);
        return outcome;
    }

    auto const a_max = max_a_for_equal_or_better_at(block_t, best_t, best_a, outcome.t_count);
    if (!a_max.has_value()) {
        spdlog::debug(
            "tie search shot: kind={} config={} T={} skip SMT (ΔT<=0 vs Block.T={})",
            tie_path_kind_label(path_kind),
            preprocess_id,
            outcome.t_count,
            block_t);
        return outcome;
    }
    spdlog::debug(
        "tie search shot: kind={} config={} T={} A_max={} (equal-or-better A/T vs best T={} A={})",
        tie_path_kind_label(path_kind),
        preprocess_id,
        outcome.t_count,
        *a_max,
        best_t,
        best_a);

    // Start SMT at the largest A that still meets the A/T threshold; unsat ⇒ cannot beat/tie.
    if (!apply_sat_width(outcome, *a_max, /*stop_if_start_unsat=*/true)) {
        return outcome;
    }
    return outcome;
}

bool has_tie_decisions(FastToddTieRunReport const& report) {
    return !report.tohpe_decisions.empty() || !report.outer_decisions.empty();
}

void apply_decision_prefix(FastToddTieControl&             control,
                           FastToddTieRunReport const&     report,
                           FastToddTieLevel                level,
                           size_t                          prefix_len) {
    auto const& decisions =
        level == FastToddTieLevel::tohpe ? report.tohpe_decisions : report.outer_decisions;
    auto& forced = level == FastToddTieLevel::tohpe ? control.forced_tohpe_choice_by_step
                                                    : control.forced_outer_choice_by_step;
    prefix_len = std::min(prefix_len, decisions.size());
    for (size_t i = 0; i < prefix_len; ++i) {
        forced[decisions[i].step_index] = decisions[i].chosen_index;
    }
}

void apply_random_prefix_from_report(FastToddTieControl&     control,
                                     FastToddTieRunReport const& report,
                                     std::mt19937_64&         rng) {
    control.forced_tohpe_choice_by_step.clear();
    control.forced_outer_choice_by_step.clear();
    if (!report.tohpe_decisions.empty()) {
        std::uniform_int_distribution<size_t> dist(0, report.tohpe_decisions.size());
        apply_decision_prefix(control, report, FastToddTieLevel::tohpe, dist(rng));
    }
    if (!report.outer_decisions.empty()) {
        std::uniform_int_distribution<size_t> dist(0, report.outer_decisions.size());
        apply_decision_prefix(control, report, FastToddTieLevel::outer, dist(rng));
    }
}

std::optional<FastToddTieStepTarget> pick_random_target_step(FastToddTieRunReport const& report,
                                                             std::mt19937_64&            rng) {
    struct Candidate {
        FastToddTieLevel level;
        size_t           step_index;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(report.tohpe_decisions.size() + report.outer_decisions.size());
    for (auto const& d : report.tohpe_decisions) {
        if (d.tie_count > 1) {
            candidates.push_back({FastToddTieLevel::tohpe, d.step_index});
        }
    }
    for (auto const& d : report.outer_decisions) {
        if (d.tie_count > 1) {
            candidates.push_back({FastToddTieLevel::outer, d.step_index});
        }
    }
    if (candidates.empty()) {
        if (report.outer_step_count > 0) {
            return FastToddTieStepTarget{.level = FastToddTieLevel::outer, .step_index = 0};
        }
        if (report.tohpe_step_count > 0) {
            return FastToddTieStepTarget{.level = FastToddTieLevel::tohpe, .step_index = 0};
        }
        return std::nullopt;
    }
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    auto const& pick = candidates[dist(rng)];
    return FastToddTieStepTarget{.level = pick.level, .step_index = pick.step_index};
}

char const* tie_mode_label(FastToddTieSearchMode mode) {
    switch (mode) {
        case FastToddTieSearchMode::original:
            return "original";
        case FastToddTieSearchMode::all_random:
            return "all-random";
        case FastToddTieSearchMode::random_target_only:
            return "target-random";
        case FastToddTieSearchMode::force_prefix_random_target:
            return "force-prefix-target-random";
    }
    return "unknown";
}

void tie_search_preprocess_self_check() {
    auto const configs = all_tableau_preprocess_configs();
    assert(configs.size() == 8);
    std::unordered_set<std::string> ids;
    for (auto const& cfg : configs) {
        ids.insert(cfg.id());
    }
    assert(ids.size() == 8);
}

void tie_search_at_metric_self_check() {
    // A=4, ΔT_best=8 → AT=0.5; trial ΔT=10 → A_max = 4*10/8 = 5
    auto const a_max = max_a_for_equal_or_better_at(/*block*/ 20, /*best_t*/ 12, /*best_a*/ 4, /*trial_t*/ 10);
    assert(a_max.has_value() && *a_max == 5);
    assert(at_score(20, 12, 4) == 0.5);
    assert(!max_a_for_equal_or_better_at(20, 12, 4, 20).has_value());
    assert(auto_tie_search_patience(12) == 60);
    assert(auto_tie_search_patience(24) == 60);
    assert(auto_tie_search_patience(25) == 63);
}

std::optional<TieSearchOutcome> run_preprocess_aware_tie_search(qcir::QCir const& source) {
    tie_search_preprocess_self_check();
    tie_search_at_metric_self_check();

    // 0 = no hard shot cap; only early-stop N ends each phase.
    size_t const max_trials = read_size_t_env("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS", 0);
    Phase2Kinds const phase2 = read_phase2_kinds_env();
    bool const all_preprocess = read_truthy_env("QSYN_FASTTODD_TIE_SEARCH_ALL_PREPROCESS");
    char const* pin_preprocess = std::getenv("QSYN_FASTTODD_TIE_SEARCH_PREPROCESS");
    std::string const pin_id =
        (pin_preprocess != nullptr && pin_preprocess[0] != '\0') ? std::string{pin_preprocess}
                                                                : std::string{};
    // Fixed by default so experiments can replay the same tie-search sequence.
    auto const seed = read_size_t_env("QSYN_FASTTODD_TIE_SEARCH_SEED", 1);
    spdlog::debug("tie search seed: {}", seed);
    std::mt19937_64 rng{seed};

    size_t const t_no_gadget = compute_t_without_gadget(source);
    if (t_no_gadget == 0 || t_no_gadget == std::numeric_limits<size_t>::max()) {
        spdlog::error("tie search: failed to compute T without gadget (full_optimize)");
        return std::nullopt;
    }
    size_t const a_no_gadget = 0;

    // Phase 1a: all preprocesses, deterministic FastTODD only (find min-T pool).
    std::vector<PreprocessTieCandidate> phase1;
    phase1.reserve(8);
    for (auto const& cfg : all_tableau_preprocess_configs()) {
        if (!pin_id.empty() && cfg.id() != pin_id) {
            continue;
        }
        auto gadgetized = prepare_gadgetized_tableau(source, cfg);
        if (!gadgetized.has_value()) {
            continue;
        }
        auto baseline = run_det_fasttodd_only(*gadgetized, cfg.id());
        spdlog::debug(
            "tie search phase1: config={} pre_T={} pre_A={} T={} (T=after FastTODD, no degadget)",
            cfg.id(),
            baseline.pre_t,
            baseline.pre_a,
            baseline.t_count);
        phase1.push_back(PreprocessTieCandidate{
            .config     = cfg,
            .gadgetized = *gadgetized,
            .baseline   = std::move(baseline),
        });
    }
    if (phase1.empty()) {
        spdlog::error(
            "tie search: no successful preprocess baseline{}",
            pin_id.empty() ? "" : fmt::format(" (pin={})", pin_id));
        return std::nullopt;
    }

    size_t min_t = phase1.front().baseline.t_count;
    for (auto const& cand : phase1) {
        min_t = std::min(min_t, cand.baseline.t_count);
    }

    std::vector<PreprocessTieCandidate> pool;
    pool.reserve(phase1.size());
    for (auto& cand : phase1) {
        // Default: min-T pool. ALL_PREPROCESS / pin: keep every phase1 survivor.
        if (all_preprocess || !pin_id.empty() || cand.baseline.t_count == min_t) {
            pool.push_back(std::move(cand));
        }
    }
    assert(!pool.empty());
    // A/T = A / (T_no_gadget - T_with_gadget); thesis names T_no_gadget "Block.T".
    size_t const block_t = t_no_gadget;
    spdlog::debug(
        "tie search: T_no_gadget={} (=Block.T) min_T_with_gadget={} pool_size={} "
        "all_preprocess={} pin={} phase2={}{} "
        "A/T = A/(T_no_gadget - T_with_gadget)",
        t_no_gadget,
        min_t,
        pool.size(),
        all_preprocess ? 1 : 0,
        pin_id.empty() ? "-" : pin_id,
        phase2.previous_best ? "previous_best" : "",
        phase2.all_random ? (phase2.previous_best ? ",all_random" : "all_random") : "");

    spdlog::debug("tie search constraint: T <= phase1_T_limit={}", min_t);

    // Phase 1b: only feasible baselines may seed best; retain other configs for trials.
    for (auto& cand : pool) {
        if (cand.baseline.t_count > min_t) continue;
        if (!apply_sat_width(cand.baseline, /*start_width=*/std::nullopt, /*stop_if_start_unsat=*/false)) {
            spdlog::warn("tie search phase1: config={} SMT failed", cand.config.id());
            continue;
        }
        spdlog::debug(
            "tie search phase1: config={} T={} A={}",
            cand.config.id(),
            cand.baseline.t_count,
            cand.baseline.sat_width);
    }
    pool.erase(
        std::remove_if(pool.begin(), pool.end(), [](PreprocessTieCandidate const& c) { return !c.baseline.ok; }),
        pool.end());
    if (pool.empty()) {
        spdlog::error("tie search: no min-T preprocess survived SMT");
        return std::nullopt;
    }

    auto const initial_best = std::find_if(pool.begin(), pool.end(), [&](auto const& cand) {
        return cand.baseline.ok && cand.baseline.t_count <= min_t;
    });
    if (initial_best == pool.end()) {
        spdlog::error("tie search: no feasible baseline at phase1 T limit {}", min_t);
        return std::nullopt;
    }
    TieSearchOutcome global_best = initial_best->baseline;
    for (auto const& cand : pool) {
        if (cand.baseline.t_count <= min_t && is_improving_at(cand.baseline, global_best, block_t)) {
            global_best = cand.baseline;
        }
    }
    // Automatic patience follows the solver-minimized width of the current best.
    // An explicit patience override remains fixed throughout the search.
    auto refresh_patience = [&]() {
        return read_size_t_env("QSYN_FASTTODD_TIE_SEARCH_PATIENCE",
                               auto_tie_search_patience(global_best.sat_width));
    };
    size_t patience_n = refresh_patience();
    size_t all_random_patience = std::max<size_t>(1, patience_n / 2);
    auto log_patience = [&]() {
        spdlog::debug(
            "tie search patience: N={} all_random_N={} A_min={} source={}",
            patience_n,
            all_random_patience,
            global_best.sat_width,
            std::getenv("QSYN_FASTTODD_TIE_SEARCH_PATIENCE") == nullptr ? "auto" : "override");
    };
    log_patience();
    // Seed previous_best from phase1 det path; hybrid always starts with previous_best.
    global_best.path_kind = TiePathKind::previous_best;
    size_t const t_after_preprocess = global_best.t_count;
    size_t const a_after_preprocess = global_best.sat_width;
    log_topt_stage("tie-search", "preprocess selection",
                   t_no_gadget, t_after_preprocess, a_no_gadget, a_after_preprocess);
    spdlog::debug(
        "tie search phase1 best: config={} T={} A={} (start with previous_best)",
        global_best.preprocess_id,
        global_best.t_count,
        global_best.sat_width);

    // Phase 2: previous_best until early-stop N, then all_random until early-stop N/2.
    // Optional max_trials (env/--cycle) is a safety cap only; 0 = unlimited.
    // ponytail: A=0 floor; upgrade path: configurable floor.
    size_t last_at_improve_step = 0;  // 0 = best already at phase1
    size_t shots_used           = 0;
    if (global_best.sat_width == 0 && global_best.t_count < block_t) {
        spdlog::debug("tie search: A=0 after phase1, skip phase2");
    } else if (!phase2.previous_best && !phase2.all_random) {
        spdlog::debug("tie search: phase2 disabled (PHASE2=none)");
    } else {
        size_t tries = 0;
        std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);

        auto find_pool = [&](std::string const& id) -> PreprocessTieCandidate const* {
            for (auto const& c : pool) {
                if (c.config.id() == id) {
                    return &c;
                }
            }
            return pool.empty() ? nullptr : &pool.front();
        };

        auto under_cap = [&]() {
            return max_trials == 0 || tries < max_trials;
        };

        auto refresh_pb_pivot = [&](size_t& x, size_t& p, size_t& shots_per_pivot, size_t& pivot_used) {
            auto const tie_steps = collect_tie_steps(global_best.report);
            p               = std::max<size_t>(1, tie_steps.size());
            shots_per_pivot = std::max<size_t>(1, patience_n / p);
            x               = p;
            pivot_used      = 0;
        };

        auto run_phase = [&](TiePathKind kind) -> bool /*continue_search*/ {
            size_t phase_patience =
                kind == TiePathKind::all_random ? all_random_patience : patience_n;
            size_t non_improving = 0;
            size_t x = 1;
            size_t p = 1;
            size_t shots_per_pivot = 1;
            size_t pivot_used = 0;
            if (kind == TiePathKind::previous_best) {
                refresh_pb_pivot(x, p, shots_per_pivot, pivot_used);
            }
            spdlog::debug(
                "tie search phase2: enter kind={} patience_N={} p={} shots_per_pivot={} "
                "max_trials={}",
                tie_path_kind_label(kind),
                phase_patience,
                kind == TiePathKind::previous_best ? p : 0,
                kind == TiePathKind::previous_best ? shots_per_pivot : 0,
                max_trials == 0 ? std::string("unlimited") : std::to_string(max_trials));

            while (non_improving < phase_patience && under_cap()) {
                size_t force_prefix_len = 0;
                PreprocessTieCandidate const* cand = nullptr;
                if (kind == TiePathKind::previous_best) {
                    force_prefix_len = x;
                    cand             = find_pool(global_best.preprocess_id);
                } else {
                    cand = &pool[pick(rng)];
                }
                if (cand == nullptr) {
                    return false;
                }

                ++tries;
                shots_used = tries;
                TieSearchOutcome trial = run_tie_search_trial_at(
                    cand->gadgetized,
                    cand->config.id(),
                    block_t,
                    global_best.t_count,
                    global_best.sat_width,
                    min_t,
                    rng(),
                    kind,
                    &global_best.report,
                    force_prefix_len);
                spdlog::debug(
                    "tie search phase2: shot={} kind={} non_improving={}/{} "
                    "x={}/{} config={} best T={} A={}",
                    tries,
                    tie_path_kind_label(kind),
                    non_improving,
                    phase_patience,
                    kind == TiePathKind::previous_best ? x : 0,
                    p,
                    cand->config.id(),
                    global_best.t_count,
                    global_best.sat_width);

                if (!trial.ok) {
                    ++non_improving;
                } else if (is_improving_at(trial, global_best, block_t)) {
                    global_best          = std::move(trial);
                    last_at_improve_step = tries;
                    non_improving        = 0;
                    patience_n = refresh_patience();
                    all_random_patience = std::max<size_t>(1, patience_n / 2);
                    phase_patience = kind == TiePathKind::all_random
                        ? all_random_patience : patience_n;
                    log_patience();
                    spdlog::debug(
                        "tie search phase2: improved step={} kind={} config={} T={} A={}",
                        last_at_improve_step,
                        tie_path_kind_label(global_best.path_kind),
                        global_best.preprocess_id,
                        global_best.t_count,
                        global_best.sat_width);
                    if (global_best.sat_width == 0 && global_best.t_count < block_t) {
                        spdlog::debug("tie search: A=0, early stop phase2");
                        return false;
                    }
                    if (kind == TiePathKind::previous_best) {
                        refresh_pb_pivot(x, p, shots_per_pivot, pivot_used);
                    }
                    continue;
                } else {
                    ++non_improving;
                }

                if (kind == TiePathKind::previous_best) {
                    ++pivot_used;
                    if (pivot_used >= shots_per_pivot) {
                        pivot_used = 0;
                        if (x > 1) {
                            --x;
                        } else {
                            x = p;  // wrap pivots until patience N
                        }
                        spdlog::debug(
                            "tie search previous_best: advance x -> {}/{} (shots_per_pivot={})",
                            x,
                            p,
                            shots_per_pivot);
                    }
                }
            }
            spdlog::debug(
                "tie search phase2: leave kind={} shots_used={} reason={}",
                tie_path_kind_label(kind),
                shots_used,
                non_improving >= phase_patience ? "early_stop_N" : "max_trials");
            return under_cap() && !(global_best.sat_width == 0 && global_best.t_count < block_t);
        };

        if (phase2.previous_best) {
            if (run_phase(TiePathKind::previous_best) && phase2.all_random) {
                run_phase(TiePathKind::all_random);
            }
        } else if (phase2.all_random) {
            run_phase(TiePathKind::all_random);
        }
    }

    size_t const t_after_search = global_best.t_count;
    size_t const a_after_search = global_best.sat_width;
    log_topt_stage("tie-search", "FastTODD path searching",
                   t_after_preprocess, t_after_search, a_after_preprocess, a_after_search);
    log_topt_stage("tie-search", "unified T-opt",
                   t_no_gadget, t_after_search, a_no_gadget, a_after_search);
    sat_reorder(global_best.tableau, /*quiet=*/true);
    global_best.t_count   = global_best.tableau.n_pauli_rotations();
    global_best.sat_width = global_best.tableau.n_ancilla();
    log_topt_stage("tie-search", "SMT width search",
                   t_after_search, global_best.t_count, a_after_search, global_best.sat_width,
                   /*with_t=*/false);
    char const* method_label = "BC";
    if (phase2.previous_best && !phase2.all_random) {
        method_label = "B";
    } else if (!phase2.previous_best && phase2.all_random) {
        method_label = "C";
    }
    spdlog::debug(
        "tie search done: method={} best_config={} T={} A={} "
        "T_no_gadget={} Block.T={} last_at_improve_step={} shots_used={} "
        "patience_N={} all_random_N={} max_trials_M={} best_kind={}",
        method_label,
        global_best.preprocess_id,
        global_best.t_count,
        global_best.sat_width,
        t_no_gadget,
        block_t,
        last_at_improve_step,
        shots_used,
        patience_n,
        all_random_patience,
        max_trials,
        tie_path_kind_label(global_best.path_kind));
    return global_best;
}

std::string format_multi_choices(FastToddTieRunReport const& report) {
    std::string out;
    auto add = [&](char const* level, std::vector<FastToddTieStepDecision> const& decs) {
        for (auto const& d : decs) {
            if (d.tie_count <= 1) {
                continue;
            }
            if (!out.empty()) {
                out += ',';
            }
            out += fmt::format("{}:{}:{}:{}", level, d.step_index, d.chosen_index, d.tie_count);
        }
    };
    add("tohpe", report.tohpe_decisions);
    add("outer", report.outer_decisions);
    return out;
}

struct ParsedChoice {
    FastToddTieLevel level = FastToddTieLevel::tohpe;
    size_t step_index      = 0;
    size_t chosen_index    = 0;
    size_t tie_count       = 0;
};

std::vector<ParsedChoice> parse_multi_choices(std::string const& s) {
    std::vector<ParsedChoice> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t const comma = s.find(',', i);
        std::string const tok = s.substr(i, comma == std::string::npos ? std::string::npos : comma - i);
        i = comma == std::string::npos ? s.size() : comma + 1;
        if (tok.empty()) {
            continue;
        }
        // level:step:chosen:tie_count
        auto p1 = tok.find(':');
        auto p2 = tok.find(':', p1 == std::string::npos ? tok.size() : p1 + 1);
        auto p3 = tok.find(':', p2 == std::string::npos ? tok.size() : p2 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos) {
            spdlog::error("PATH_PROBE: bad FORCE token '{}'", tok);
            continue;
        }
        ParsedChoice c;
        auto const level = tok.substr(0, p1);
        c.level         = (level == "outer") ? FastToddTieLevel::outer : FastToddTieLevel::tohpe;
        c.step_index    = static_cast<size_t>(std::stoull(tok.substr(p1 + 1, p2 - p1 - 1)));
        c.chosen_index  = static_cast<size_t>(std::stoull(tok.substr(p2 + 1, p3 - p2 - 1)));
        c.tie_count     = static_cast<size_t>(std::stoull(tok.substr(p3 + 1)));
        out.push_back(c);
    }
    return out;
}

/** Product of the last `back` multi-way tie_counts (parent=1, grandparent=2, ...). */
unsigned __int128 remaining_product_ancestor(std::vector<ParsedChoice> const& choices, size_t back) {
    if (choices.empty() || back == 0) {
        return 1;
    }
    size_t const n = std::min(back, choices.size());
    unsigned __int128 prod = 1;
    for (size_t k = choices.size() - n; k < choices.size(); ++k) {
        prod *= std::max<size_t>(1, choices[k].tie_count);
    }
    return prod;
}

std::string int128_to_string(unsigned __int128 x) {
    if (x == 0) {
        return "0";
    }
    std::string s;
    while (x > 0) {
        s.push_back(char('0' + static_cast<int>(x % 10)));
        x /= 10;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

TieSearchOutcome run_full_smt_random_path(Tableau const& gadgetized,
                                          std::string const& preprocess_id,
                                          std::uint64_t seed,
                                          TiePathKind kind,
                                          FastToddTieRunReport const* force_report,
                                          size_t force_prefix_len,
                                          std::vector<ParsedChoice> const* force_choices) {
    TieSearchOutcome outcome{
        .ok            = false,
        .pre_t         = gadgetized.n_pauli_rotations(),
        .pre_a         = gadgetized.n_ancilla(),
        .tableau       = gadgetized,
        .preprocess_id = preprocess_id,
        .path_kind     = kind};
    FastToddTieControl control;
    control.enabled     = true;
    control.mode        = FastToddTieSearchMode::all_random;
    control.random_seed = seed;
    if (force_choices != nullptr && force_prefix_len > 0) {
        size_t const n = std::min(force_prefix_len, force_choices->size());
        for (size_t i = 0; i < n; ++i) {
            auto const& c = (*force_choices)[i];
            if (c.level == FastToddTieLevel::tohpe) {
                control.forced_tohpe_choice_by_step[c.step_index] = c.chosen_index;
            } else {
                control.forced_outer_choice_by_step[c.step_index] = c.chosen_index;
            }
        }
    } else if (kind == TiePathKind::previous_best && force_report != nullptr) {
        auto const tie_steps = collect_tie_steps(*force_report);
        force_tie_prefix(control, tie_steps, force_prefix_len);
    }
    set_fasttodd_tie_control(control);
    optimize_phase_polynomial_with_classical(outcome.tableau, FastToddPhasePolynomialOptimizationStrategy{});
    auto report = consume_fasttodd_tie_run_report();
    set_fasttodd_tie_control(std::nullopt);
    if (!report.has_value()) {
        return outcome;
    }
    outcome.report  = *report;
    outcome.t_count = outcome.report.final_term_count > 0 ? outcome.report.final_term_count
                                                         : outcome.tableau.n_pauli_rotations();
    // Full min-A SMT (no A_max prune) so bad paths are observable.
    if (!apply_sat_width(outcome, /*start_width=*/std::nullopt, /*stop_if_start_unsat=*/false)) {
        return outcome;
    }
    return outcome;
}

std::string format_parsed_choices(std::vector<ParsedChoice> const& cs) {
    std::string out;
    for (auto const& c : cs) {
        if (!out.empty()) {
            out += ',';
        }
        out += fmt::format("{}:{}:{}:{}",
                           c.level == FastToddTieLevel::outer ? "outer" : "tohpe",
                           c.step_index,
                           c.chosen_index,
                           c.tie_count);
    }
    return out;
}

std::vector<size_t> parse_size_list(char const* raw) {
    std::vector<size_t> out;
    if (raw == nullptr || raw[0] == '\0') {
        return out;
    }
    std::stringstream ss{raw};
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok.empty()) {
            continue;
        }
        try {
            out.push_back(static_cast<size_t>(std::stoull(tok)));
        } catch (...) {
            spdlog::warn("LEVEL_SAMPLE: skip bad index '{}'", tok);
        }
    }
    return out;
}

std::vector<size_t> sample_without_replacement(size_t take, size_t total, std::mt19937_64& rng) {
    if (total == 0) {
        return {};
    }
    take = std::min(take, total);
    std::vector<size_t> all(total);
    std::iota(all.begin(), all.end(), 0);
    for (size_t i = 0; i < take; ++i) {
        std::uniform_int_distribution<size_t> dist(i, total - 1);
        std::swap(all[i], all[dist(rng)]);
    }
    all.resize(take);
    std::sort(all.begin(), all.end());
    return all;
}

double level_sample_frac(size_t k, size_t p) {
    if (p <= 1 || k >= p) {
        return 1.0;
    }
    return 0.10 + 0.90 * static_cast<double>(k - 1) / static_cast<double>(p - 1);
}

std::uint64_t mix_prefix_seed(std::uint64_t seed, std::vector<ParsedChoice> const& forces) {
    std::uint64_t h = seed ^ 0x9e3779b97f4a7c15ULL;
    for (auto const& c : forces) {
        h ^= static_cast<std::uint64_t>(c.level) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= static_cast<std::uint64_t>(c.step_index) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= static_cast<std::uint64_t>(c.chosen_index) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    }
    return h;
}

TieSearchOutcome run_forced_path(Tableau const& gadgetized,
                                 std::string const& preprocess_id,
                                 std::vector<ParsedChoice> const& forces,
                                 bool do_smt) {
    TieSearchOutcome outcome{
        .ok            = false,
        .pre_t         = gadgetized.n_pauli_rotations(),
        .pre_a         = gadgetized.n_ancilla(),
        .tableau       = gadgetized,
        .preprocess_id = preprocess_id,
        .path_kind     = TiePathKind::previous_best};
    FastToddTieControl control;
    control.enabled = true;
    // Unforced steps take index 0 so a prefix uniquely determines the suffix.
    control.mode    = FastToddTieSearchMode::original;
    for (auto const& c : forces) {
        if (c.level == FastToddTieLevel::tohpe) {
            control.forced_tohpe_choice_by_step[c.step_index] = c.chosen_index;
        } else {
            control.forced_outer_choice_by_step[c.step_index] = c.chosen_index;
        }
    }
    set_fasttodd_tie_control(control);
    optimize_phase_polynomial_with_classical(
        outcome.tableau, FastToddPhasePolynomialOptimizationStrategy{}, &outcome.t_count);
    auto report = consume_fasttodd_tie_run_report();
    set_fasttodd_tie_control(std::nullopt);
    if (!report.has_value()) {
        return outcome;
    }
    outcome.report  = *report;
    outcome.t_count = outcome.report.final_term_count > 0 ? outcome.report.final_term_count
                                                         : outcome.tableau.n_pauli_rotations();
    if (!do_smt) {
        outcome.ok = true;
        return outcome;
    }
    if (!apply_sat_width(outcome, /*start_width=*/std::nullopt, /*stop_if_start_unsat=*/false)) {
        return outcome;
    }
    return outcome;
}

void run_level_sample(Tableau const& gadgetized,
                      std::string const& pin_id,
                      size_t block_t,
                      TieSearchOutcome const& original,
                      Tableau& tableau_out) {
    auto const multi_from = [](FastToddTieRunReport const& report) {
        std::vector<ParsedChoice> out;
        auto push = [&](FastToddTieLevel level, std::vector<FastToddTieStepDecision> const& decs) {
            for (auto const& d : decs) {
                if (d.tie_count > 1) {
                    out.push_back(ParsedChoice{
                        .level = level,
                        .step_index = d.step_index,
                        .chosen_index = d.chosen_index,
                        .tie_count = d.tie_count,
                    });
                }
            }
        };
        push(FastToddTieLevel::tohpe, report.tohpe_decisions);
        push(FastToddTieLevel::outer, report.outer_decisions);
        return out;
    };
    auto const det_steps = multi_from(original.report);
    if (det_steps.empty()) {
        spdlog::error("LEVEL_SAMPLE: det path has no multi-way ties");
        tableau_out = original.tableau;
        return;
    }
    size_t const p = det_steps.size();
    auto const first = det_steps.front();
    std::uint64_t const seed = read_size_t_env("QSYN_TIE_PATH_PROBE_SEED", 1);
    size_t const max_leaves = read_size_t_env("QSYN_TIE_PATH_PROBE_MAX_LEAVES", 0);
    char const* out_c = std::getenv("QSYN_TIE_PATH_PROBE_OUT");
    std::string const out_path = (out_c != nullptr && out_c[0] != '\0') ? std::string{out_c} : std::string{};
    std::ofstream out;
    if (!out_path.empty()) {
        out.open(out_path);
        if (!out) {
            spdlog::error("LEVEL_SAMPLE: cannot open {}", out_path);
            tableau_out = original.tableau;
            return;
        }
    }
    char const* worker_c = std::getenv("QSYN_TIE_PATH_PROBE_WORKER");
    std::string const worker = (worker_c != nullptr) ? std::string{worker_c} : std::string{"0"};

    std::vector<size_t> assigned = parse_size_list(std::getenv("QSYN_TIE_PATH_PROBE_STEP1"));
    if (assigned.empty()) {
        size_t const n1 = std::max<size_t>(
            1, static_cast<size_t>(std::ceil(level_sample_frac(1, p) * static_cast<double>(first.tie_count))));
        std::mt19937_64 rng{seed};
        assigned = sample_without_replacement(n1, first.tie_count, rng);
    }

    spdlog::info(
        "LEVEL_SAMPLE start worker={} config={} p={} step1_tc={} assigned={} max_leaves={} out={}",
        worker,
        pin_id,
        p,
        first.tie_count,
        assigned.size(),
        max_leaves,
        out_path.empty() ? "-" : out_path);

    size_t n_leaves = 0;
    size_t n_ok     = 0;
    bool stop       = false;
    auto const t0   = std::chrono::steady_clock::now();

    auto write_leaf = [&](std::vector<ParsedChoice> const& forces, TieSearchOutcome const& leaf) {
        ++n_leaves;
        if (leaf.ok) {
            ++n_ok;
        }
        double const at = leaf.ok ? at_score(block_t, leaf.t_count, leaf.sat_width)
                                  : std::numeric_limits<double>::infinity();
        std::string const line = fmt::format(
            "{{\"w\":\"{}\",\"prefix\":\"{}\",\"T\":{},\"A\":{},\"AT\":{:.6f},\"k\":{},\"ok\":{}}}\n",
            worker,
            format_parsed_choices(forces),
            leaf.t_count,
            leaf.ok ? leaf.sat_width : 0,
            at,
            forces.size(),
            leaf.ok ? 1 : 0);
        if (out.is_open()) {
            out << line;
            out.flush();
        }
        if (n_leaves % 200 == 0) {
            auto const sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            spdlog::info("LEVEL_SAMPLE progress worker={} leaves={} ok={} sec={:.1f}",
                         worker, n_leaves, n_ok, sec);
        }
        if (max_leaves > 0 && n_leaves >= max_leaves) {
            stop = true;
        }
    };

    std::function<void(std::vector<ParsedChoice>, size_t)> rec =
        [&](std::vector<ParsedChoice> forces, size_t k) {
            if (stop) {
                return;
            }
            // Discover next multi-way step under this prefix (FastTODD, no SMT).
            auto probe = run_forced_path(gadgetized, pin_id, forces, /*do_smt=*/false);
            auto const steps = multi_from(probe.report);
            if (!probe.ok || steps.size() <= forces.size()) {
                auto leaf = run_forced_path(gadgetized, pin_id, forces, /*do_smt=*/true);
                write_leaf(forces, leaf);
                return;
            }
            auto const next = steps[forces.size()];
            double const frac = level_sample_frac(k, p);
            size_t const n_take = std::max<size_t>(
                1, static_cast<size_t>(std::ceil(frac * static_cast<double>(next.tie_count))));
            std::mt19937_64 rng{mix_prefix_seed(seed, forces)};
            auto const children = sample_without_replacement(n_take, next.tie_count, rng);
            bool const at_leaf_depth = k >= p;
            for (size_t idx : children) {
                if (stop) {
                    return;
                }
                ParsedChoice child{
                    .level         = next.level,
                    .step_index    = next.step_index,
                    .chosen_index  = idx,
                    .tie_count     = next.tie_count};
                auto child_forces = forces;
                child_forces.push_back(child);
                if (at_leaf_depth) {
                    auto leaf = run_forced_path(gadgetized, pin_id, child_forces, /*do_smt=*/true);
                    write_leaf(child_forces, leaf);
                } else {
                    rec(std::move(child_forces), k + 1);
                }
            }
        };

    ParsedChoice root{
        .level        = first.level,
        .step_index   = first.step_index,
        .chosen_index = 0,
        .tie_count    = first.tie_count};
    for (size_t idx : assigned) {
        if (stop) {
            break;
        }
        if (idx >= first.tie_count) {
            spdlog::warn("LEVEL_SAMPLE worker={} skip step1={} (tc={})", worker, idx, first.tie_count);
            continue;
        }
        root.chosen_index = idx;
        rec({root}, /*k=*/2);
    }

    auto const sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    spdlog::info(
        "LEVEL_SAMPLE done worker={} config={} leaves={} ok={} sec={:.1f}",
        worker, pin_id, n_leaves, n_ok, sec);
    tableau_out = original.tableau;
}

}  // namespace

bool run_tie_path_probe_from_qcir(qcir::QCir const& source, Tableau& tableau_out) {
    char const* pin = std::getenv("QSYN_FASTTODD_TIE_SEARCH_PREPROCESS");
    if (pin == nullptr || pin[0] == '\0') {
        spdlog::error("PATH_PROBE: set QSYN_FASTTODD_TIE_SEARCH_PREPROCESS");
        return false;
    }
    std::string const pin_id{pin};
    char const* mode_c = std::getenv("QSYN_TIE_PATH_PROBE_MODE");
    std::string const mode = (mode_c == nullptr || mode_c[0] == '\0') ? "oneshot" : mode_c;
    std::uint64_t seed = read_size_t_env("QSYN_TIE_PATH_PROBE_SEED", 0);
    if (seed == 0) {
        seed = std::random_device{}();
    }

    TableauPreprocessConfig cfg{};
    bool found = false;
    for (auto const& c : all_tableau_preprocess_configs()) {
        if (c.id() == pin_id) {
            cfg   = c;
            found = true;
            break;
        }
    }
    if (!found) {
        spdlog::error("PATH_PROBE: unknown preprocess '{}'", pin_id);
        return false;
    }
    auto gadgetized = prepare_gadgetized_tableau(source, cfg);
    if (!gadgetized.has_value()) {
        return false;
    }
    size_t const block_t = compute_t_without_gadget(source);
    auto original        = run_det_fasttodd_only(*gadgetized, pin_id);
    if (!apply_sat_width(original, std::nullopt, false)) {
        spdlog::error("PATH_PROBE: original SMT failed");
        return false;
    }
    size_t const original_a = original.sat_width;
    size_t const thesis_a   = read_size_t_env("QSYN_TIE_PATH_PROBE_THESIS_A", 9);

    if (mode == "level_sample") {
        run_level_sample(*gadgetized, pin_id, block_t, original, tableau_out);
        return true;
    }

    TieSearchOutcome trial;
    size_t force_prefix_len = 0;
    std::string force_str;
    std::vector<ParsedChoice> force_parsed;
    if (mode == "subtree") {
        char const* fc = std::getenv("QSYN_TIE_PATH_PROBE_FORCE");
        if (fc == nullptr || fc[0] == '\0') {
            spdlog::error("PATH_PROBE subtree: need QSYN_TIE_PATH_PROBE_FORCE");
            return false;
        }
        force_str         = fc;
        force_parsed      = parse_multi_choices(force_str);
        force_prefix_len = read_size_t_env("QSYN_TIE_PATH_PROBE_PREFIX_LEN", 0);
        // Ancestor depth back: 1=parent, 2=grandparent, ... Default parent if unset.
        size_t const anc_back = read_size_t_env("QSYN_TIE_PATH_PROBE_ANCESTOR_K", 1);
        if (force_prefix_len == 0 && !force_parsed.empty()) {
            force_prefix_len = force_parsed.size() > anc_back ? force_parsed.size() - anc_back : 0;
        }
        trial = run_full_smt_random_path(
            *gadgetized, pin_id, seed, TiePathKind::previous_best, nullptr, force_prefix_len, &force_parsed);
    } else if (mode == "hunt") {
        // previous_best walk with full SMT until A<=thesis or HUNT_N shots (for rare goods).
        size_t const hunt_n = std::max<size_t>(1, read_size_t_env("QSYN_TIE_PATH_PROBE_HUNT_N", 200));
        std::mt19937_64 rng{seed};
        trial             = original;
        trial.path_kind   = TiePathKind::previous_best;
        size_t tries      = 0;
        size_t non_improve = 0;
        while (tries < hunt_n && trial.sat_width > thesis_a && non_improve < hunt_n) {
            auto const steps = collect_tie_steps(trial.report);
            size_t const p   = std::max<size_t>(1, steps.size());
            size_t const x   = std::uniform_int_distribution<size_t>(1, p)(rng);
            std::uint64_t const shot_seed = rng();
            auto cand = run_full_smt_random_path(
                *gadgetized, pin_id, shot_seed, TiePathKind::previous_best, &trial.report, x, nullptr);
            ++tries;
            if (!cand.ok) {
                ++non_improve;
                continue;
            }
            // Improve by A/T, then T, then A (match tie-search ranking).
            bool better = false;
            double const cand_at = at_score(block_t, cand.t_count, cand.sat_width);
            double const best_at = at_score(block_t, trial.t_count, trial.sat_width);
            if (cand_at < best_at) {
                better = true;
            } else if (cand_at == best_at && cand.t_count < trial.t_count) {
                better = true;
            } else if (cand_at == best_at && cand.t_count == trial.t_count &&
                       cand.sat_width < trial.sat_width) {
                better = true;
            }
            if (better) {
                trial        = std::move(cand);
                non_improve  = 0;
                force_prefix_len = x;
                spdlog::info(
                    "PATH_PROBE hunt: seed={} step={} T={} A={} (target thesis_A={})",
                    seed,
                    tries,
                    trial.t_count,
                    trial.sat_width,
                    thesis_a);
            } else {
                ++non_improve;
            }
        }
        spdlog::info("PATH_PROBE hunt done: seed={} tries={} best_A={}", seed, tries, trial.sat_width);
    } else {
        char const* kind_c = std::getenv("QSYN_TIE_PATH_PROBE_KIND");
        std::string const kind_s =
            (kind_c == nullptr || kind_c[0] == '\0') ? "all_random" : kind_c;
        TiePathKind kind = TiePathKind::all_random;
        if (kind_s == "previous_best" || kind_s == "B" || kind_s == "b") {
            kind = TiePathKind::previous_best;
            auto const steps = collect_tie_steps(original.report);
            size_t const p   = std::max<size_t>(1, steps.size());
            // Random pivot depth on the original path (still a random suffix).
            force_prefix_len = read_size_t_env("QSYN_TIE_PATH_PROBE_PREFIX_LEN", 0);
            if (force_prefix_len == 0) {
                std::mt19937_64 rng{seed};
                force_prefix_len = std::uniform_int_distribution<size_t>(1, p)(rng);
            }
            trial = run_full_smt_random_path(
                *gadgetized, pin_id, seed, kind, &original.report, force_prefix_len, nullptr);
        } else {
            trial = run_full_smt_random_path(
                *gadgetized, pin_id, seed, kind, nullptr, 0, nullptr);
        }
    }

    if (!trial.ok) {
        spdlog::info(
            "PATH_PROBE mode={} seed={} config={} ok=0 T={} A= fail original_A={} thesis_A={} "
            "block_t={}",
            mode,
            seed,
            pin_id,
            trial.t_count,
            original_a,
            thesis_a,
            block_t);
        tableau_out = std::move(original.tableau);
        return true;
    }

    auto const choices = format_multi_choices(trial.report);
    auto const parsed  = parse_multi_choices(choices);
    size_t const p     = parsed.size();
    // Log parent (k=1) remaining product; orchestrator computes deeper ancestors from choices=.
    unsigned __int128 const rem = remaining_product_ancestor(parsed, 1);
    char const* label           = "mid";
    if (trial.sat_width <= thesis_a) {
        label = "good";
    } else if (trial.sat_width > original_a) {
        label = "bad";
    }
    size_t const anc_prefix = p >= 1 ? p - 1 : 0;
    spdlog::info(
        "PATH_PROBE mode={} seed={} config={} ok=1 T={} A={} label={} original_A={} thesis_A={} "
        "block_t={} A/T={:.6f} multi_ties={} gp_prefix={} gp_remaining_product={} "
        "force_prefix_len={} choices={}",
        mode,
        seed,
        pin_id,
        trial.t_count,
        trial.sat_width,
        label,
        original_a,
        thesis_a,
        block_t,
        at_score(block_t, trial.t_count, trial.sat_width),
        p,
        anc_prefix,
        int128_to_string(rem),
        force_prefix_len,
        choices);
    tableau_out = std::move(trial.tableau);
    return true;
}

bool minimize_ancillary_t_opt_from_qcir(qcir::QCir const& source,
                                        Tableau& tableau_out) {
    if (read_truthy_env("QSYN_TIE_PATH_PROBE")) {
        return run_tie_path_probe_from_qcir(source, tableau_out);
    }
    auto const best = run_preprocess_aware_tie_search(source);
    if (!best.has_value()) {
        return false;
    }
    tableau_out = std::move(best->tableau);
    spdlog::debug("tie search: optimized tableau ready");
    return true;
}

bool minimize_t_opt_tie_search_from_qcir(qcir::QCir const& source,
                                         Tableau& tableau_out,
                                         size_t repeats,
                                         TOnlyTieSearchAggregate* aggregate_out) {
    if (repeats == 0) {
        spdlog::error("t-only tie search: repeats must be >= 1");
        return false;
    }

    size_t const patience_n = [&] {
        if (char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH_PATIENCE")) {
            try {
                return static_cast<size_t>(std::stoull(value));
            } catch (...) {
            }
        }
        return size_t{100};
    }();
    size_t const max_trials = [&] {
        if (char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS")) {
            try {
                return static_cast<size_t>(std::stoull(value));
            } catch (...) {
            }
        }
        return size_t{1000};
    }();
    spdlog::info(
        "t-only tie search: repeats={} patience_N={} max_trials_M={} (no ancilla/SMT)",
        repeats,
        patience_n,
        max_trials);

    std::mt19937_64 rng{std::random_device{}()};
    TOnlyTieSearchAggregate agg;
    agg.repeats = repeats;
    agg.per_repeat.reserve(repeats);

    std::optional<Tableau> best_tableau;
    size_t best_t = std::numeric_limits<size_t>::max();

    for (size_t r = 1; r <= repeats; ++r) {
        // Fresh RNG stream per repeat for independence.
        std::mt19937_64 run_rng{rng()};
        // Inline one run here so phase1_min_t is recorded correctly.
        // Re-call internal via a thin path: duplicate call into restored logic.

        // Phase-1 + phase-2 (T only) — local copy of run_t_only_tie_search_once fields.
        // The helper lives in the anonymous namespace above; call through a re-implemented
        // public-facing loop that mirrors it and fixes phase1_min_t.
        auto gadget_configs = all_tableau_preprocess_configs();
        // (config, pre-FastTODD gadgetized tableau, deterministic FastTODD T, post-FastTODD tableau)
        struct Phase1Entry {
            TableauPreprocessConfig config;
            Tableau gadgetized;
            size_t t_count = 0;
            Tableau optimized;
        };
        std::vector<Phase1Entry> phase1;
        phase1.reserve(8);
        size_t phase1_min_t = std::numeric_limits<size_t>::max();

        for (auto const& cfg : gadget_configs) {
            auto gadgetized = prepare_gadgetized_tableau(source, cfg);
            if (!gadgetized.has_value()) {
                continue;
            }
            Tableau optimized = *gadgetized;
            size_t t_count    = 0;
            optimize_phase_polynomial_with_classical(
                optimized, FastToddPhasePolynomialOptimizationStrategy{}, &t_count);
            spdlog::info(
                "t-only tie search phase1: repeat={}/{} config={} T={}",
                r,
                repeats,
                cfg.id(),
                t_count);
            phase1_min_t = std::min(phase1_min_t, t_count);
            phase1.push_back(Phase1Entry{
                .config     = cfg,
                .gadgetized = *gadgetized,
                .t_count    = t_count,
                .optimized  = std::move(optimized),
            });
        }
        if (phase1.empty() || phase1_min_t == std::numeric_limits<size_t>::max()) {
            spdlog::error("t-only tie search: repeat {} failed (no baseline)", r);
            return false;
        }

        Tableau best_tab{0};
        std::string best_cfg;
        size_t cur_best_t = phase1_min_t;
        bool have_incumbent = false;
        for (auto& entry : phase1) {
            if (entry.t_count != phase1_min_t) {
                continue;
            }
            if (!have_incumbent || entry.t_count < cur_best_t) {
                best_tab       = entry.optimized;
                best_cfg       = entry.config.id();
                cur_best_t     = entry.t_count;
                have_incumbent = true;
            }
        }
        assert(have_incumbent);

        size_t last_t_reduce_step = 0;
        size_t phase2_trials      = 0;
        for (auto const& entry : phase1) {
            if (entry.t_count != phase1_min_t) {
                continue;
            }
            size_t non_improving = 0;
            size_t tries         = 0;
            while (non_improving < patience_n && tries < max_trials) {
                ++tries;
                ++phase2_trials;
                Tableau tab = entry.gadgetized;
                FastToddTieControl control;
                control.enabled     = true;
                control.mode        = FastToddTieSearchMode::all_random;
                control.random_seed = run_rng();
                set_fasttodd_tie_control(control);
                optimize_phase_polynomial_with_classical(tab, FastToddPhasePolynomialOptimizationStrategy{});
                auto report = consume_fasttodd_tie_run_report();
                set_fasttodd_tie_control(std::nullopt);
                size_t t_count = 0;
                if (report.has_value() && report->final_term_count > 0) {
                    t_count = report->final_term_count;
                } else {
                    t_count = tab.n_pauli_rotations();
                }
                if (t_count < cur_best_t) {
                    cur_best_t         = t_count;
                    best_tab           = std::move(tab);
                    best_cfg           = entry.config.id();
                    last_t_reduce_step = phase2_trials;
                    non_improving      = 0;
                    spdlog::info(
                        "t-only tie search phase2: repeat={}/{} T-reduce step={} config={} T={}",
                        r,
                        repeats,
                        last_t_reduce_step,
                        best_cfg,
                        cur_best_t);
                } else {
                    ++non_improving;
                }
            }
        }

        TOnlyTieSearchStats st{
            .phase1_min_t       = phase1_min_t,
            .final_t            = cur_best_t,
            .last_t_reduce_step = last_t_reduce_step,
            .phase2_trials      = phase2_trials,
            .best_config        = best_cfg,
        };
        agg.per_repeat.push_back(st);
        spdlog::info(
            "t-only tie search done: repeat={}/{} phase1_T={} final_T={} last_t_reduce_step={} phase2_trials={} config={}",
            r,
            repeats,
            st.phase1_min_t,
            st.final_t,
            st.last_t_reduce_step,
            st.phase2_trials,
            st.best_config);

        if (cur_best_t < best_t) {
            best_t       = cur_best_t;
            best_tableau = std::move(best_tab);
        } else if (!best_tableau.has_value()) {
            best_tableau = std::move(best_tab);
            best_t       = cur_best_t;
        }
    }

    // Aggregate
    agg.min_t = agg.per_repeat.front().final_t;
    agg.max_t = agg.per_repeat.front().final_t;
    double sum_t = 0;
    double sum_step = 0;
    double sum_step_at_min = 0;
    for (auto const& st : agg.per_repeat) {
        agg.min_t = std::min(agg.min_t, st.final_t);
        agg.max_t = std::max(agg.max_t, st.final_t);
        sum_t += static_cast<double>(st.final_t);
        sum_step += static_cast<double>(st.last_t_reduce_step);
    }
    for (auto const& st : agg.per_repeat) {
        if (st.final_t == agg.min_t) {
            ++agg.n_hit_min_t;
            sum_step_at_min += static_cast<double>(st.last_t_reduce_step);
        }
    }
    agg.avg_final_t = sum_t / static_cast<double>(repeats);
    agg.avg_last_t_reduce_step = sum_step / static_cast<double>(repeats);
    agg.avg_last_t_reduce_step_at_min_t =
        agg.n_hit_min_t > 0 ? sum_step_at_min / static_cast<double>(agg.n_hit_min_t) : 0.0;

    spdlog::info(
        "t-only tie search summary: repeats={} min_T={} max_T={} avg_T={:.3f} "
        "avg_last_t_reduce_step={:.3f} avg_last_t_reduce_step_at_min_T={:.3f} hit_min_T={}/{}",
        repeats,
        agg.min_t,
        agg.max_t,
        agg.avg_final_t,
        agg.avg_last_t_reduce_step,
        agg.avg_last_t_reduce_step_at_min_t,
        agg.n_hit_min_t,
        repeats);

    if (aggregate_out != nullptr) {
        *aggregate_out = std::move(agg);
    }
    if (!best_tableau.has_value()) {
        return false;
    }
    tableau_out = std::move(*best_tableau);
    return true;
}

void log_topt_stage(std::string_view cmd, std::string_view stage,
                    size_t t_before, size_t t_after, size_t a_before, size_t a_after,
                    bool with_t) {
    if (with_t) {
        spdlog::warn("{}: after {}  T count: {} -> {}  A count: {} -> {}",
                     cmd, stage, t_before, t_after, a_before, a_after);
    } else {
        spdlog::warn("{}: after {}  A count: {} -> {}", cmd, stage, a_before, a_after);
    }
}

/**
 * @brief Run classical ancillary-T optimization with gadgetization and FastTODD phase optimization.
 *
 * @param tableau
 */
void minimize_ancillary_t_opt(Tableau& tableau,
                              std::optional<std::string> export_filename) {
    (void)export_filename;
    if (tableau.is_empty()) {
        return;
    }
    if (tie_search_enabled_from_env()) {
        spdlog::warn(
            "QSYN_FASTTODD_TIE_SEARCH is set but tie-search requires QCir; "
            "use minimize_ancillary_t_opt_from_qcir via tie-search after qc read");
    }
    size_t const t0 = tableau.t_count();
    size_t const a0 = tableau.n_ancilla();
    [[maybe_unused]] auto const pmc_to_unified_pr = minimize_internal_hadamards_n_gadgetize(tableau);
    if (has_gadget_ancillae(tableau)) {
        optimize_phase_polynomial_with_classical(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    } else {
        spdlog::debug("minimize_ancillary_t_opt: no gadget ancilla; using standard FastTODD");
        optimize_phase_polynomial(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    }
    log_topt_stage("unify", "unified T-opt", t0, tableau.t_count(), a0, tableau.n_ancilla());
}

void blockwise_gadgetize_optimize(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }

    auto const before_t = tableau.n_pauli_rotations();
    auto const before_a = tableau.ancilla_initial_states().size();
    spdlog::debug("BlockwiseAncillaryTopt: begin (non-Clifford={}, ancilla={})", before_t, before_a);

    spdlog::debug("BlockwiseAncillaryTopt: merge_rotations");
    merge_rotations(tableau);
    spdlog::debug("BlockwiseAncillaryTopt: after merge_rotations (non-Clifford={})", tableau.n_pauli_rotations());

    spdlog::debug("BlockwiseAncillaryTopt: properize");
    properize(tableau);
    spdlog::debug("BlockwiseAncillaryTopt: after properize (non-Clifford={})", tableau.n_pauli_rotations());

    spdlog::debug("BlockwiseAncillaryTopt: minimize_internal_hadamards");
    minimize_internal_hadamards(tableau);
    spdlog::debug(
        "BlockwiseAncillaryTopt: after minimize_internal_hadamards (non-Clifford={}, ancilla={})",
        tableau.n_pauli_rotations(), tableau.ancilla_initial_states().size());

    spdlog::debug("BlockwiseAncillaryTopt: blockwise_gadgetize");
    blockwise_gadgetize(tableau);

    spdlog::debug(
        "BlockwiseAncillaryTopt: end (non-Clifford={}, ancilla={})",
        tableau.n_pauli_rotations(), tableau.ancilla_initial_states().size());
}
// matroid partitioning

/**
 * @brief split the phase polynomial into matroids. The matroids are represented by a list of PauliRotations, which must be all-diagonal.
 *
 * @param polynomial
 * @return std::vector<std::vector<PauliRotation>>
 */
std::optional<std::vector<std::vector<PauliRotation>>> matroid_partition(std::vector<PauliRotation> const& polynomial, MatroidPartitionStrategy const& strategy, size_t num_ancillae) {
    if (!is_phase_polynomial(polynomial)) {
        return std::nullopt;
    }

    return strategy.partition(polynomial, num_ancillae);
}

/**
 * @brief split the phase polynomial into matroids. The matroids are represented by a list of PauliRotations, which must be all-diagonal.
 *
 * @param polynomial
 * @param strategy
 * @param num_ancillae
 * @return Tableau
 */
std::optional<Tableau> matroid_partition(Tableau const& tableau, MatroidPartitionStrategy const& strategy, size_t num_ancillae) {
    auto new_tableau = Tableau{tableau.n_qubits()};

    for (auto const& subtableau : tableau) {
        if (auto const pr = std::get_if<std::vector<PauliRotation>>(&subtableau)) {
            auto partitions = matroid_partition(*pr, strategy, num_ancillae);
            if (!partitions) {
                return std::nullopt;
            }
            for (auto const& partition : *partitions) {
                new_tableau.push_back(partition);
            }
        } else {
            new_tableau.push_back(subtableau);
        }
    }

    return new_tableau;
}

/**
 * @brief check if the terms of the polynomial are linearly independent; if so, the transformation |x_1, ..., x_m>|0...0> |--> |y_1, ..., y_n> is reversible
 *
 * @param polynomial
 * @param num_ancillae
 * @return true
 * @return false
 */
auto MatroidPartitionStrategy::is_independent(std::vector<PauliRotation> const& polynomial, size_t num_ancillae) const -> bool {
    DVLAB_ASSERT(is_phase_polynomial(polynomial), "The input pauli rotations a phase polynomial.");

    auto const dim_v = polynomial.front().n_qubits();
    auto const n     = dim_v + num_ancillae;
    // equivalent to the independence oracle lemma:
    //     dim(V) - rank(S) <= n - |S|
    // in the literature, where n is the number of qubits = polynomial dimension + num_ancillae
    // ref: [Polynomial-time T-depth Optimization of Clifford+T circuits via Matroid Partitioning](https://arxiv.org/pdf/1303.2042.pdf)
    // Here, we reorganize the inequality to make circumvent unsigned integer overflow
    return dim_v + polynomial.size() <= n + matrix_rank(polynomial);
};

MatroidPartitionStrategy::Partitions NaiveMatroidPartitionStrategy::partition(MatroidPartitionStrategy::Polynomial const& polynomial, size_t num_ancillae) const {
    auto matroids = std::vector(1, std::vector<PauliRotation>{});  // starts with an empty matroid

    if (polynomial.empty()) {
        return matroids;
    }

    for (auto const& term : polynomial) {
        matroids.back().push_back(term);
        if (!this->is_independent(matroids.back(), num_ancillae)) {
            matroids.back().pop_back();
            matroids.push_back({term});
        }
    }

    DVLAB_ASSERT(std::ranges::none_of(matroids, [](std::vector<PauliRotation> const& matroid) { return matroid.empty(); }), "The matroids must not be empty.");

    return matroids;
}

}  // namespace tableau

}  // namespace qsyn