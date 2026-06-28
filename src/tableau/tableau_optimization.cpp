/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gsl/narrow>
#include <limits>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <stdexcept>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <unordered_map>
#include <variant>
#include <vector>

#include "tableau/classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

namespace qsyn {

namespace experimental {

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
        optimize_phase_polynomial(tableau, ToddPhasePolynomialOptimizationStrategy{});
        spdlog::info("{}: Reduced the number of non-Clifford gates from {} to {}.", ++count, non_clifford_count, tableau.n_pauli_rotations());
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
void optimize_phase_polynomial_with_classical(Tableau& tableau, PhasePolynomialOptimizationStrategy const& strategy) {
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

namespace {

bool tie_search_enabled_from_env() {
    if (char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH")) {
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y';
    }
    return false;
}

bool tie_search_random_all_steps_enabled_from_env() {
    if (char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH_RANDOM_ALL_STEPS")) {
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y';
    }
    return false;
}

std::optional<FastToddTieSearchMode> parse_tie_search_mode(std::string_view mode) {
    if (mode == "original") {
        return FastToddTieSearchMode::original;
    }
    if (mode == "all-random") {
        return FastToddTieSearchMode::all_random;
    }
    if (mode == "target-random") {
        return FastToddTieSearchMode::random_target_only;
    }
    if (mode == "force-prefix-target-random") {
        return FastToddTieSearchMode::force_prefix_random_target;
    }
    return std::nullopt;
}

FastToddTieSearchMode tie_search_mode_from_env() {
    if (char const* value = std::getenv("QSYN_FASTTODD_TIE_SEARCH_MODE")) {
        auto const parsed = parse_tie_search_mode(value);
        if (parsed.has_value()) {
            return *parsed;
        }
        spdlog::warn(
            "QSYN_FASTTODD_TIE_SEARCH_MODE='{}' invalid, fallback=target-random", value);
    }
    // Backward-compatible flag support.
    if (tie_search_random_all_steps_enabled_from_env()) {
        return FastToddTieSearchMode::all_random;
    }
    return FastToddTieSearchMode::random_target_only;
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

struct TieSearchOutcome {
    bool                  ok = false;
    bool                  early_unsat = false;
    size_t                t_count = 0;
    size_t                sat_width = 0;
    Tableau               tableau{0};
    FastToddTieRunReport  report;
};

std::optional<size_t> solve_sat_min_width(Tableau const& tableau,
                                          std::optional<size_t> start_width,
                                          bool stop_if_start_unsat,
                                          bool* start_unsat) {
    if (start_unsat != nullptr) {
        *start_unsat = false;
    }
    SatSignatureExport const sig = compute_sat_signature_blocks(tableau);
    AncillaSmtInstance inst = build_ancilla_smt_instance(sig);
    AncillaScheduleResult const schedule = solve_ancilla_schedule(
        inst, AncillaScheduleSolveOptions{
            .start_width = start_width,
            .stop_if_start_unsat = stop_if_start_unsat,
        });
    if (!schedule.ok) {
        if (start_width.has_value() && stop_if_start_unsat && start_unsat != nullptr) {
            *start_unsat = true;
        }
        return std::nullopt;
    }
    return schedule.width_w;
}

bool dominates(std::pair<size_t, size_t> const& lhs, std::pair<size_t, size_t> const& rhs) {
    return lhs.first <= rhs.first && lhs.second <= rhs.second &&
           (lhs.first < rhs.first || lhs.second < rhs.second);
}

void pareto_insert(std::vector<std::pair<size_t, size_t>>& pareto,
                   std::pair<size_t, size_t> const point) {
    if (std::ranges::any_of(pareto, [&](auto const& cur) { return dominates(cur, point); })) {
        return;
    }
    pareto.erase(
        std::remove_if(
            pareto.begin(), pareto.end(),
            [&](auto const& cur) { return dominates(point, cur); }),
        pareto.end());
    pareto.push_back(point);
}

bool is_improving(TieSearchOutcome const& trial, TieSearchOutcome const& best) {
    if (trial.t_count < best.t_count) {
        return true;
    }
    if (trial.t_count == best.t_count && trial.sat_width < best.sat_width) {
        return true;
    }
    return false;
}

std::optional<size_t> chosen_index_for_step(FastToddTieRunReport const& report,
                                            FastToddTieLevel const level,
                                            size_t const step_index) {
    auto const& decisions = level == FastToddTieLevel::tohpe
        ? report.tohpe_decisions
        : report.outer_decisions;
    for (auto const& decision : decisions) {
        if (decision.step_index == step_index) {
            return decision.chosen_index;
        }
    }
    return std::nullopt;
}

size_t step_count_for_level(FastToddTieRunReport const& report, FastToddTieLevel const level) {
    return level == FastToddTieLevel::tohpe ? report.tohpe_step_count : report.outer_step_count;
}

TieSearchOutcome run_tie_search_trial(Tableau const& base_tableau,
                                      FastToddTieControl const& control,
                                      std::optional<FastToddTieStepTarget> target,
                                      size_t best_t,
                                      size_t best_width,
                                      std::uint64_t seed) {
    TieSearchOutcome outcome{.ok = false, .tableau = base_tableau};
    FastToddTieControl run_control = control;
    run_control.enabled = true;
    run_control.target_random_step = target;
    run_control.random_seed = seed;

    set_fasttodd_tie_control(run_control);
    optimize_phase_polynomial_with_classical(outcome.tableau, FastToddPhasePolynomialOptimizationStrategy{});
    auto report = consume_fasttodd_tie_run_report();
    set_fasttodd_tie_control(std::nullopt);

    if (!report.has_value()) {
        spdlog::error("tie search: missing FastTODD tie report");
        return outcome;
    }
    outcome.report = *report;
    outcome.t_count = outcome.report.final_term_count > 0
        ? outcome.report.final_term_count
        : outcome.tableau.n_pauli_rotations();

    if (outcome.t_count > best_t) {
        return outcome;
    }

    bool start_unsat = false;
    std::optional<size_t> start_width = std::nullopt;
    bool stop_if_start_unsat = false;
    if (outcome.t_count == best_t) {
        start_width = best_width > 0 ? best_width - 1 : 0;
        stop_if_start_unsat = true;
    }

    auto const sat_width = solve_sat_min_width(
        outcome.tableau, start_width, stop_if_start_unsat, &start_unsat);
    if (!sat_width.has_value()) {
        outcome.early_unsat = start_unsat;
        return outcome;
    }

    outcome.sat_width = *sat_width;
    outcome.ok = true;
    return outcome;
}

void optimize_phase_polynomial_with_classical_tie_search(
    Tableau& tableau,
    std::optional<FastToddTieSearchMode> tie_search_mode_override) {
    size_t const patience_n = read_size_t_env("QSYN_FASTTODD_TIE_SEARCH_PATIENCE", 10);
    size_t const max_trials = read_size_t_env("QSYN_FASTTODD_TIE_SEARCH_MAX_TRIALS", 50);
    std::mt19937_64 rng{std::random_device{}()};

    FastToddTieControl tie_control;
    tie_control.mode = tie_search_mode_override.value_or(tie_search_mode_from_env());
    TieSearchOutcome best = run_tie_search_trial(
        tableau, tie_control, std::nullopt,
        std::numeric_limits<size_t>::max(),
        std::numeric_limits<size_t>::max(),
        rng());
    if (!best.ok) {
        spdlog::warn("tie search: baseline run failed, fallback to standard FastTODD");
        optimize_phase_polynomial_with_classical(tableau, FastToddPhasePolynomialOptimizationStrategy{});
        return;
    }

    std::vector<std::pair<size_t, size_t>> pareto;
    pareto_insert(pareto, {best.t_count, best.sat_width});

    std::array<FastToddTieLevel, 2> const levels = {
        FastToddTieLevel::tohpe, FastToddTieLevel::outer};
    for (auto const level : levels) {
        size_t step_index = 0;
        while (step_index < step_count_for_level(best.report, level)) {
            size_t non_improving = 0;
            size_t tries = 0;
            while (non_improving < patience_n && tries < max_trials) {
                auto const target = FastToddTieStepTarget{.level = level, .step_index = step_index};
                TieSearchOutcome trial = run_tie_search_trial(
                    tableau, tie_control, target, best.t_count, best.sat_width, rng());
                ++tries;

                if (!trial.ok) {
                    ++non_improving;
                    spdlog::info(
                        "tie search: level={} step={} try={}/{} stop={} (non-improving)",
                        level == FastToddTieLevel::tohpe ? "tohpe" : "outer",
                        step_index, tries, max_trials,
                        trial.early_unsat ? "start-width-unsat" : "eval-fail");
                    continue;
                }

                pareto_insert(pareto, {trial.t_count, trial.sat_width});
                if (is_improving(trial, best)) {
                    if (tie_control.mode == FastToddTieSearchMode::force_prefix_random_target) {
                        if (auto const chosen = chosen_index_for_step(trial.report, level, step_index);
                            chosen.has_value()) {
                            if (level == FastToddTieLevel::tohpe) {
                                tie_control.forced_tohpe_choice_by_step[step_index] = *chosen;
                            } else {
                                tie_control.forced_outer_choice_by_step[step_index] = *chosen;
                            }
                        }
                    }
                    best = std::move(trial);
                    non_improving = 0;
                    spdlog::info(
                        "tie search: improved at level={} step={} -> T={} width={} pareto={}",
                        level == FastToddTieLevel::tohpe ? "tohpe" : "outer",
                        step_index, best.t_count, best.sat_width, pareto.size());
                } else {
                    ++non_improving;
                }
            }
            ++step_index;
        }
    }

    spdlog::info(
        "tie search: final best T={} width={} pareto_points={}",
        best.t_count, best.sat_width, pareto.size());
    // ponytail: commit the best T-opt route once, then run reorder on that exact tableau.
    tableau = std::move(best.tableau);
    size_t const pr_after_topt = tableau.n_pauli_rotations();
    size_t const anc_before_reorder = tableau.n_ancilla();
    sat_reorder(tableau);
    spdlog::info(
        "tie search: reorder on best route finished (PR={}, ancilla {} -> {})",
        pr_after_topt, anc_before_reorder, tableau.n_ancilla());
}

}  // namespace

/**
 * @brief Run classical ancillary-T optimization with gadgetization and FastTODD phase optimization.
 *
 * @param tableau
 */
void minimize_ancillary_t_opt(Tableau& tableau,
                              std::optional<std::string> export_filename,
                              bool enable_tie_search,
                              std::optional<FastToddTieSearchMode> tie_search_mode) {
    (void)export_filename;
    if (tableau.is_empty()) {
        return;
    }
    [[maybe_unused]] auto const pmc_to_unified_pr = minimize_internal_hadamards_n_gadgetize(tableau);
    if (has_gadget_ancillae(tableau)) {
        if (enable_tie_search || tie_search_enabled_from_env()) {
            optimize_phase_polynomial_with_classical_tie_search(tableau, tie_search_mode);
        } else {
            optimize_phase_polynomial_with_classical(tableau, FastToddPhasePolynomialOptimizationStrategy{});
        }
    } else {
        spdlog::info("minimize_ancillary_t_opt: no gadget ancilla; using standard FastTODD");
        optimize_phase_polynomial(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    }
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

}  // namespace experimental

}  // namespace qsyn