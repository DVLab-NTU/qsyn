/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <gsl/narrow>
#include <limits>
#include <ranges>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <variant>
#include <vector>

#include "tableau/classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

namespace qsyn {

namespace experimental {

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
 * @brief Commute PMCs to the end of the tableau.
 *        Moves all post-measurement CCTs (PMCs) to the end while commuting them through
 *        intermediate tableaux (STs, PRs, and CCCs).
 *        Iterates through all subtableaux and commutes PMCs through StabilizerTableau, PauliRotation, and CCCs.
 *
 * @param tableau
 */
void commute_classical(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }
    // Index pointing to where the next PMC should be moved (starts at end)
    size_t pmc_target_idx = tableau.size();
    size_t pmc_count = 0;
    
    // Iterate through subtableaux in reverse order, only processing PMCs
    for (size_t idx = tableau.size(); idx > 0; --idx) {
        size_t actual_idx = idx - 1;  // Convert to 0-based index        
        if (auto* pmc = std::get_if<ClassicalControlTableau>(&tableau[actual_idx])){
            // Only process post-measurement CCTs (PMCs)
            if (pmc->is_pmc()) {
                // This is a post-measurement CCT - commute it through following tableaux
                for (size_t j = actual_idx + 1; j < pmc_target_idx; ++j) {
                    std::visit(
                        dvlab::overloaded{
                            [pmc](StabilizerTableau& st) {
                                commute_through_stabilizer(*pmc, st);  
                            },
                            [pmc](std::vector<PauliRotation>& pr) {
                                commute_through_pauli_rotation(*pmc, pr);
                            },
                            [pmc](ClassicalControlTableau& other_cct) {
                                // Check if we encountered another PMC or a CCC
                                if (other_cct.is_pmc()) {
                                    spdlog::error("PMC encountered another PMC during commutation - this should not happen");
                                } else if (other_cct.is_ccc()) {
                                    // Extract the CCC's StabilizerTableau (as reference) and commute PMC through it
                                    // commute_through_stabilizer modifies pmc.operations(), not the CCC
                                    StabilizerTableau& ccc_st = other_cct.operations();
                                    commute_through_stabilizer(*pmc, ccc_st);
                                } else {
                                    spdlog::error("Encountered CCT with unknown type during commutation");
                                }
                            }},
                        tableau[j]);                
                }
                
                // Move PMC to target position (shift elements left, place PMC at end)
                // Target position is pmc_target_idx - 1 (the end of non-PMC section)
                if (actual_idx < pmc_target_idx - 1) {
                    // Save the PMC
                    SubTableau pmc_sub = std::move(tableau[actual_idx]);
                    // Shift all elements from (actual_idx + 1) to (pmc_target_idx - 1) one position left
                    for (size_t k = actual_idx; k < pmc_target_idx - 1; ++k) {
                        tableau[k] = std::move(tableau[k + 1]);
                    }
                    // Place PMC at target position (end of non-PMC section)
                    tableau[pmc_target_idx - 1] = std::move(pmc_sub);
                }
                // Decrement target index for next PMC
                pmc_target_idx--;
                pmc_count++;
            }
            // Skip CCCs - they are not moved, only their operations are modified when PMC commutes through them
        }
    }
    
    if (pmc_count > 0) {
        spdlog::info("Commutation complete. Moved {} post-measurement CCT(s) to end.", pmc_count);
    }
    
    // Re-establish CCC-PMC pairing after moves (pointers may have been invalidated)
    reestablish_hadamard_gadget_pairing(tableau);
}

/**
 * @brief Commute PRs to the end and merge them into one.
 *        Final structure: {CCC & ST}{PR}{PMC}
 *        - CCCs and STs remain unchanged (not collapsed)
 *        - All PRs are commuted to the end and merged into one
 *        - PMCs are at the end (kept in original tableau)
 *
 * @param tableau
 */
void commute_and_merge_rotations(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }

    // Step 1: Commute PMCs to the end
    commute_classical(tableau);

    // Step 2: Find where PMCs start (they are at the end after commute_classical)
    size_t pmc_start_idx = tableau.size();
    for (size_t idx = tableau.size(); idx > 0; --idx) {
        size_t actual_idx = idx - 1;
        if (auto* cct = std::get_if<ClassicalControlTableau>(&tableau[actual_idx])) {
            if (cct->is_pmc()) {
                pmc_start_idx = actual_idx;
            } else {
                // Found a CCC, stop
                break;
            }
        } else {
            // Found non-CCT, stop
            break;
        }
    }
    
    // Step 3: Commute PRs to the end (before PMCs)
    // Index pointing to where the next PR should be moved (starts at pmc_start_idx)
    size_t pr_target_idx = pmc_start_idx;
    size_t pr_count = 0;
    
    // Iterate through subtableaux in reverse order (up to pmc_start_idx), only processing PRs
    for (size_t idx = pmc_start_idx; idx > 0; --idx) {
        size_t actual_idx = idx - 1;  // Convert to 0-based index
        if (auto* pr = std::get_if<std::vector<PauliRotation>>(&tableau[actual_idx])) {
            // This is a PR - commute it through following tableaux (STs and CCCs)
            for (size_t j = actual_idx + 1; j < pr_target_idx; ++j) {
                std::visit(
                    dvlab::overloaded{
                        [pr](StabilizerTableau& st) {
                            // Commute PR through ST by extracting clifford operators and applying to PR
                            auto clifford_ops = extract_clifford_operators(st);
                            for (auto& rotation : *pr) {
                                rotation.apply(clifford_ops);
                            }
                        },
                        [pr](std::vector<PauliRotation>& /* other_pr */) {
                            // Should not encounter another PR at this point
                            spdlog::error("PR encountered another PR during commutation - this should not happen");
                        },
                        [pr](ClassicalControlTableau& cct) {
                            // Commute PR through CCC's internal StabilizerTableau
                            // CCCs remain unchanged - we only commute through their operations
                            if (cct.is_ccc()) {
                                StabilizerTableau& ccc_st = cct.operations();
                                auto clifford_ops = extract_clifford_operators(ccc_st);
                                for (auto& rotation : *pr) {
                                    rotation.apply(clifford_ops);
                                }
                            }
                            // Skip PMCs - they are already at the end
                        }},
                    tableau[j]);
            }
            
            // Move PR to target position (shift elements left, place PR at end of non-PR section)
            if (actual_idx < pr_target_idx - 1) {
                // Save the PR
                SubTableau pr_sub = std::move(tableau[actual_idx]);
                // Shift all elements from (actual_idx + 1) to (pr_target_idx - 1) one position left
                for (size_t k = actual_idx; k < pr_target_idx - 1; ++k) {
                    tableau[k] = std::move(tableau[k + 1]);
                }
                // Place PR at target position (end of non-PR section, before PMCs)
                tableau[pr_target_idx - 1] = std::move(pr_sub);
            }
            // Decrement target index for next PR
            pr_target_idx--;
            pr_count++;
        }
        // Skip STs and CCCs - they are not moved, only PRs commute through them
    }
    
    if (pr_count > 0) {
        spdlog::info("PR commutation complete. Moved {} PR(s) to end.", pr_count);
    }
    
    // Step 4: Merge all consecutive PRs at the end (before PMCs) into one
    // Find where PRs start (they should be consecutive before PMCs)
    size_t pr_start_idx = pmc_start_idx;
    while (pr_start_idx > 0 && std::holds_alternative<std::vector<PauliRotation>>(tableau[pr_start_idx - 1])) {
        pr_start_idx--;
    }
    
    // Merge all PRs from pr_start_idx to pmc_start_idx
    if (pr_start_idx < pmc_start_idx) {
        // Get the first PR vector (will become the merged one)
        auto* merged_pr = std::get_if<std::vector<PauliRotation>>(&tableau[pr_start_idx]);
        if (merged_pr) {
            // Merge all subsequent PRs into the first one
            for (size_t idx = pr_start_idx + 1; idx < pmc_start_idx; ++idx) {
                auto* pr = std::get_if<std::vector<PauliRotation>>(&tableau[idx]);
                if (pr) {
                    merged_pr->insert(merged_pr->end(), pr->begin(), pr->end());
                }
            }
            
            // Erase all PRs except the first (merged) one
            tableau.erase(tableau.begin() + pr_start_idx + 1, tableau.begin() + pmc_start_idx);
        }
    }
    
    // Step 5: Remove identity STs (optional cleanup)
    tableau.erase(
        std::remove_if(
            tableau.begin(),
            tableau.end(),
            [](SubTableau const& subtableau) -> bool {
                if (auto* st = std::get_if<StabilizerTableau>(&subtableau)) {
                    return st->is_identity();
                }
                return false;
            }),
        tableau.end());
    
    // Re-establish CCC-PMC pairing after moves (pointers may have been invalidated)
    reestablish_hadamard_gadget_pairing(tableau);
}

/**
 * @brief Collapse the tableau with classical operations.
 *        Final structure: {ST}{PR}{PMC}
 *        - Calls commute_classical() first to move PMCs to end
 *        - Applies normal collapse() to non-PMC part, treating CCCs as stabilizers
 *        - Result: single ST, single PR, and PMCs
 *
 * @param tableau
 */
void collapse_with_classical(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }

    size_t const n_qubits = tableau.n_qubits();

    // Step 1: Commute PMCs to the end
    commute_classical(tableau);

    // Step 2: Extract PMCs from the end
    std::vector<SubTableau> pmc_ccts;
    while (!tableau.is_empty() && std::holds_alternative<ClassicalControlTableau>(tableau.back())) {
        auto* cct = std::get_if<ClassicalControlTableau>(&tableau.back());
        if (cct && cct->is_pmc()) {
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
            if (cct->is_ccc()) {
                // Replace CCC with its internal StabilizerTableau
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
 * @brief Classical T optimization: gadgetize H gates, commute classical operations, and optimize with FastTodd.
 * This function first minimizes internal Hadamards and gadgetizes them, then commutes classical operations,
 * collapses, and finally applies FastTodd phase polynomial optimization.
 *
 * @param tableau
 */
void minimize_ancillary_t_opt(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }
    size_t non_clifford_count = tableau.n_pauli_rotations();
    minimize_internal_hadamards_n_gadgetize(tableau);
    export_hadamard_gadget_pairs(tableau);
    commute_and_merge_rotations(tableau);
    export_hadamard_gadget_pairs(tableau);
    spdlog::debug("Phase polynomial optimization");
    optimize_phase_polynomial(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    export_hadamard_gadget_pairs(tableau);
    collapse_with_classical(tableau);
    spdlog::info("Reduced the number of non-Clifford gates from {} to {}, at the cost of {} ancilla qubits", non_clifford_count, tableau.n_pauli_rotations(), tableau.ancilla_initial_states().size());
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

/**
 * @brief Re-establish CCC-PMC pairing after moves that may have invalidated pointers.
 *        Matches CCCs and PMCs by ancilla qubit and reference qubit.
 *
 * @param tableau The tableau to fix pairing for
 */
void reestablish_hadamard_gadget_pairing(Tableau& tableau) {
    // Collect all CCCs and PMCs with their indices
    std::vector<std::pair<size_t, ClassicalControlTableau*>> ccc_list;
    std::vector<std::pair<size_t, ClassicalControlTableau*>> pmc_list;
    
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct) {
            if (cct->is_ccc()) {
                ccc_list.emplace_back(idx, cct);
            } else if (cct->is_pmc()) {
                pmc_list.emplace_back(idx, cct);
            }
        }
    }
    
    // Match CCCs with PMCs based on ancilla qubit
    size_t paired_count = 0;
    for (auto& [ccc_idx, ccc_ptr] : ccc_list) {
        // Find matching PMC by ancilla qubit
        for (auto& [pmc_idx, pmc_ptr] : pmc_list) {
            if (ccc_ptr->ancilla_qubit() == pmc_ptr->ancilla_qubit()) {
                // Verify reference qubit matches if present
                if (ccc_ptr->reference_qubit().has_value() && 
                    pmc_ptr->reference_qubit().has_value() &&
                    ccc_ptr->reference_qubit().value() == pmc_ptr->reference_qubit().value()) {
                    // Re-establish bidirectional pairing
                    ccc_ptr->set_paired_cct(pmc_ptr);
                    pmc_ptr->set_paired_cct(ccc_ptr);
                    paired_count++;
                    break;
                } else if (!ccc_ptr->reference_qubit().has_value() && 
                          !pmc_ptr->reference_qubit().has_value()) {
                    // Both don't have reference qubits, match by ancilla only
                    ccc_ptr->set_paired_cct(pmc_ptr);
                    pmc_ptr->set_paired_cct(ccc_ptr);
                    paired_count++;
                    break;
                }
            }
        }
    }
    
    if (paired_count > 0) {
        spdlog::debug("Re-established {} H-gadget pairings after commutation", paired_count);
    }
}

/**
 * @brief Export all H-gadget pairs (CCC-PMC pairs) from a tableau.
 *        Examines the correctness of the pairing structure by finding all CCCs
 *        and their corresponding PMCs, tracking ancilla qubits.
 *
 * @param tableau The tableau to examine
 * @return Vector of HadamardGadgetPair structures containing pairing information
 */
std::vector<HadamardGadgetPair> export_hadamard_gadget_pairs(Tableau const& tableau) {
    std::vector<HadamardGadgetPair> pairs;
    
    // First pass: collect all CCCs and their indices
    std::vector<std::pair<size_t, ClassicalControlTableau const*>> ccc_list;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct && cct->is_ccc()) {
            ccc_list.emplace_back(idx, cct);
        }
    }
    
    // Second pass: for each CCC, find its paired PMC
    for (auto const& [ccc_idx, ccc_ptr] : ccc_list) {
        HadamardGadgetPair pair;
        pair.ccc_index = ccc_idx;
        pair.ancilla_qubit = ccc_ptr->ancilla_qubit();
        pair.reference_qubit = ccc_ptr->reference_qubit();
        pair.is_paired = false;
        pair.pmc_index = std::numeric_limits<size_t>::max();  // Invalid index
        
        // Check if CCC has a paired PMC pointer
        auto const* paired_pmc = ccc_ptr->get_paired_cct();
        if (paired_pmc && paired_pmc->is_pmc()) {
            // Find the index of the paired PMC in the tableau
            for (size_t idx = 0; idx < tableau.size(); ++idx) {
                auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
                if (cct == paired_pmc) {
                    pair.pmc_index = idx;
                    pair.is_paired = true;
                    
                    // Verify pairing is bidirectional
                    if (cct->get_paired_cct() == ccc_ptr) {
                        // Verify ancilla qubits match
                        if (cct->ancilla_qubit() == pair.ancilla_qubit) {
                            pairs.push_back(pair);
                            // Log the correctly paired Hadamard gadget
                            if (pair.reference_qubit.has_value()) {
                                spdlog::info("Hadamard Gadget Pair: CCC[{}] <-> PMC[{}] | ancilla={}, reference={}",
                                           ccc_idx, idx, pair.ancilla_qubit, pair.reference_qubit.value());
                            } else {
                                spdlog::info("Hadamard Gadget Pair: CCC[{}] <-> PMC[{}] | ancilla={}, reference=N/A",
                                           ccc_idx, idx, pair.ancilla_qubit);
                            }
                        } else {
                            spdlog::warn("CCC at index {} and PMC at index {} have mismatched ancilla qubits: {} vs {}",
                                       ccc_idx, idx, pair.ancilla_qubit, cct->ancilla_qubit());
                        }
                    } else {
                        spdlog::warn("Pairing is not bidirectional: CCC at {} -> PMC at {}, but PMC -> CCC is {}",
                                   ccc_idx, idx, cct->get_paired_cct() ? "different" : "null");
                    }
                    break;
                }
            }
            
            if (!pair.is_paired) {
                spdlog::warn("CCC at index {} has a paired PMC pointer, but PMC not found in tableau", ccc_idx);
            }
        } else {
            spdlog::warn("CCC at index {} (ancilla {}, reference {}) has no paired PMC",
                        ccc_idx, pair.ancilla_qubit, 
                        pair.reference_qubit.has_value() ? pair.reference_qubit.value() : std::numeric_limits<size_t>::max());
        }
    }
    
    // Third pass: find orphaned PMCs (PMCs without paired CCCs)
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct && cct->is_pmc()) {
            bool found_in_pairs = false;
            for (auto const& pair : pairs) {
                if (pair.pmc_index == idx) {
                    found_in_pairs = true;
                    break;
                }
            }
            if (!found_in_pairs) {
                spdlog::warn("PMC at index {} (ancilla {}) has no paired CCC", idx, cct->ancilla_qubit());
            }
        }
    }
    
    spdlog::info("Exported {} H-gadget pairs from tableau ({} CCCs found)", pairs.size(), ccc_list.size());
    
    return pairs;
}

}  // namespace experimental

}  // namespace qsyn
