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
#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>
#include <unordered_set>

namespace qsyn::experimental {

namespace {

std::pair<ClassicalControlTableau, ClassicalControlTableau>
gadgetize_hadamard(size_t reference_qubit, size_t ancilla_index, size_t total_qubits);

size_t count_h_gates(StabilizerTableau const& st) {
    auto const ops = extract_clifford_operators(st);
    return static_cast<size_t>(std::count_if(
        ops.begin(), ops.end(),
        [](CliffordOperator const& op) { return op.first == CliffordOperatorType::h; }));
}

void pad_to_qubits(StabilizerTableau& st, size_t n_qubits) {
    while (st.n_qubits() < n_qubits) {
        st.add_ancilla_qubit();
    }
}

void pad_to_qubits(std::vector<PauliRotation>& pr, size_t n_qubits) {
    for (auto& r : pr) {
        while (r.n_qubits() < n_qubits) {
            r.add_ancilla_qubit();
        }
    }
}

void pad_to_qubits(ClassicalControlTableau& cct, size_t n_qubits) {
    while (cct.operations().n_qubits() < n_qubits) {
        cct.add_ancilla_qubit();
    }
}

void pad_to_qubits(SubTableau& sub, size_t n_qubits) {
    std::visit(
        dvlab::overloaded{
            [&](StabilizerTableau& st) { pad_to_qubits(st, n_qubits); },
            [&](std::vector<PauliRotation>& pr) { pad_to_qubits(pr, n_qubits); },
            [&](ClassicalControlTableau& cct) { pad_to_qubits(cct, n_qubits); }},
        sub);
}

struct GadgetizedWindow {
    size_t total_qubits{};
    size_t new_ancillae{};
    std::vector<SubTableau> subtableaux;
    std::vector<std::pair<size_t, AncillaInitialState>> ancilla_states;
    std::vector<std::pair<size_t, size_t>> gadget_pairs;
};

GadgetizedWindow gadgetize_one_internal_window(
    std::vector<PauliRotation> pr_left,
    StabilizerTableau st_mid,
    std::vector<PauliRotation> pr_right,
    size_t total_qubits,
    size_t ancilla_start_index) {

    GadgetizedWindow w;
    auto const h_count = count_h_gates(st_mid);
    w.total_qubits = total_qubits;
    w.new_ancillae = h_count;

    pad_to_qubits(pr_left, total_qubits);
    pad_to_qubits(pr_right, total_qubits);
    pad_to_qubits(st_mid, total_qubits);

    w.subtableaux.emplace_back(std::move(pr_left));

    auto const clifford_ops = extract_clifford_operators(st_mid);
    std::vector<CliffordOperator> pending;
    size_t ancilla_index = ancilla_start_index;

    for (auto const& op : clifford_ops) {
        if (op.first != CliffordOperatorType::h) {
            pending.push_back(op);
            continue;
        }

        if (!pending.empty()) {
            StabilizerTableau st_before(w.total_qubits);
            st_before.apply(pending);
            w.subtableaux.emplace_back(std::move(st_before));
            pending.clear();
        }

        size_t const qubit = op.second[0];
        auto [ccc, pmc] = gadgetize_hadamard(qubit, ancilla_index, w.total_qubits);

        size_t const ccc_idx = w.subtableaux.size();
        w.subtableaux.emplace_back(std::move(ccc));
        size_t const pmc_idx = w.subtableaux.size();
        w.subtableaux.emplace_back(std::move(pmc));

        w.gadget_pairs.emplace_back(ccc_idx, pmc_idx);
        w.ancilla_states.emplace_back(ancilla_index, AncillaInitialState::PLUS);
        ++ancilla_index;
    }

    if (!pending.empty()) {
        StabilizerTableau remaining_st(w.total_qubits);
        remaining_st.apply(pending);
        w.subtableaux.emplace_back(std::move(remaining_st));
    }

    w.subtableaux.emplace_back(std::move(pr_right));
    return w;
}

std::pair<std::vector<PauliRotation>, std::vector<PauliRotation>> split_by_ancilla_support(
    std::vector<PauliRotation> const& pr,
    size_t data_n_qubits) {

    std::vector<PauliRotation> uses_ancilla;
    std::vector<PauliRotation> ancilla_free;
    uses_ancilla.reserve(pr.size());
    ancilla_free.reserve(pr.size());
    
    for (auto const& rot : pr) {
        bool touches_ancilla = false;
        for (size_t q = data_n_qubits; q < rot.n_qubits(); ++q) {
            if (!rot.is_i(q)) {
                touches_ancilla = true;
                break;
            }
        }
        (touches_ancilla ? uses_ancilla : ancilla_free).push_back(rot);
    }

    spdlog::debug("Split by ancilla support: uses_ancilla={}, ancilla_free={}", uses_ancilla.size(), ancilla_free.size());
    return {std::move(uses_ancilla), std::move(ancilla_free)};
}


/**
 * @brief Verify the per-step identity: CT · PR ≡ C_H · PR · CT'
 *
 * Builds the combined circuit CT | PR | adjoint(C_H | PR | CT') and checks
 * that full optimization reduces it to the identity (empty tableau).
 *
 * @param ct_orig       The original Clifford block before the split
 * @param pr            The (unchanged) Pauli Rotation block
 * @param c_h           The H-only factor: C_H
 * @param c_rest_prime  The updated non-H factor: CT' = C_rest after commutation
 * @return true iff the identity holds
 */
[[nodiscard]] bool verify_push_z_step(
    StabilizerTableau const& ct_orig,
    std::vector<PauliRotation> const& pr,
    StabilizerTableau const& c_h,
    StabilizerTableau const& c_rest_prime) {

    size_t const n = ct_orig.n_qubits();

    // LHS: CT | PR
    Tableau lhs(n);
    lhs.push_back(ct_orig);
    lhs.push_back(pr);

    // RHS: C_H | PR | CT'
    Tableau rhs(n);
    rhs.push_back(c_h);
    rhs.push_back(pr);
    rhs.push_back(c_rest_prime);

    // Build LHS | adjoint(RHS) — should collapse to identity if the step is correct
    Tableau combined(n);
    for (auto const& sub : lhs)           combined.push_back(sub);
    for (auto const& sub : adjoint(rhs))  combined.push_back(sub);

    full_optimize(combined);
    remove_identities(combined);
    return combined.is_empty();
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
 * @param ancilla_index   The ancilla qubit for the gadget (b)
 * @param total_qubits    Pre-computed n_data + total_ancilla_count
 * @return std::pair<ClassicalControlTableau, ClassicalControlTableau> 
 *         First: CCC (pre-measurement), Second: PMC (post-measurement)
 */
std::pair<ClassicalControlTableau, ClassicalControlTableau>
gadgetize_hadamard(size_t reference_qubit, size_t ancilla_index, size_t total_qubits) {

    ClassicalControlTableau ccc(CCTType::Gadget, ancilla_index, reference_qubit);
    pad_to_qubits(ccc, total_qubits);

    ClassicalControlTableau pmc(CCTType::ClassicalControl, ancilla_index, reference_qubit);
    pad_to_qubits(pmc, total_qubits);
    pmc.add_gate({CliffordOperatorType::x, std::array<size_t, 2>{reference_qubit, 0}});

    return {std::move(ccc), std::move(pmc)};
}

  // namespace





/**
 * @brief Process tableau and gadgetize H gates in StabilizerTableau when no PauliRotations follow.
 *
 * Two-pass implementation:
 *   Pass 1 – count internal H gates.  After minimize_internal_hadamards the
 *             tableau has the form ST [PR ST]*, so a StabilizerTableau is
 *             "internal" iff its immediate predecessor AND successor are both
 *             PauliRotation blocks.  This neighbour check avoids maintaining
 *             any running PR counter.
 *   Pass 2 – build all subtableaux at the pre-computed total size; no
 *             incremental resizing needed.
 *
 * @param tableau
 */




void gadgetize_tableau(Tableau& tableau) {
    size_t const original_n_qubits = tableau.n_qubits();
    size_t const n                  = tableau.size();

    auto const is_pr = [&](size_t i) {
        return std::holds_alternative<std::vector<PauliRotation>>(tableau[i]);
    };

    // ─── Pass 1: count internal H gates ──────────────────────────────────────
    // An ST at index i is internal iff tableau[i-1] and tableau[i+1] are PRs.
    size_t internal_h_count = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::holds_alternative<std::vector<PauliRotation>>(tableau[i])) {
            const auto& pauli_rotations = std::get<std::vector<PauliRotation>>(tableau[i]);
            // Print the count of columns (i.e., Pauli rotations applied in this block)
            spdlog::debug("PauliRotation columns count: {}", pauli_rotations.size());
        }
        if (!std::holds_alternative<StabilizerTableau>(tableau[i])) continue;
        if (i == 0 || i + 1 == n) continue;
        if (!is_pr(i - 1) || !is_pr(i + 1)) continue;

        auto const ops = extract_clifford_operators(std::get<StabilizerTableau>(tableau[i]));
        internal_h_count += static_cast<size_t>(
            std::count_if(ops.begin(), ops.end(), [](CliffordOperator const& op) {
                return op.first == CliffordOperatorType::h;
            }));
        spdlog::debug("Internal H count: {}", internal_h_count);
    }

    size_t const total_qubits = original_n_qubits + internal_h_count;
    size_t ancilla_index      = original_n_qubits;

    // ─── Pass 2: rebuild tableau with all subtableaux pre-sized ──────────────
    std::vector<SubTableau> new_subtableaux;
    std::vector<std::pair<size_t, AncillaInitialState>> new_ancilla_states;
    std::vector<std::pair<size_t, size_t>> gadget_pairs;

    for (size_t i = 0; i < n; ++i) {
        std::visit(
            dvlab::overloaded{
                [&](StabilizerTableau const& st) {
                    bool const is_internal =
                        i > 0 && i + 1 < n && is_pr(i - 1) && is_pr(i + 1);

                    if (!is_internal) {
                        // First or last ST: operates only on the original n data
                        // qubits and never touches the ancilla — keep at size n.
                        new_subtableaux.push_back(st);
                        return;
                    }

                    // Internal ST: gadgetize each H gate in sequence.
                    auto const clifford_ops = extract_clifford_operators(st);
                    std::vector<CliffordOperator> pending;

                    for (auto const& op : clifford_ops) {
                        if (op.first != CliffordOperatorType::h) {
                            pending.push_back(op);
                            continue;
                        }
                        size_t const qubit = op.second[0];

                        // Flush non-H ops accumulated before this H gate.
                        if (!pending.empty()) {
                            StabilizerTableau st_before(total_qubits);
                            st_before.apply(pending);
                            new_subtableaux.push_back(std::move(st_before));
                            pending.clear();
                        }



                        // Emit the CCC + PMC pair for this Hadamard gadget.
                        auto [ccc, pmc] = gadgetize_hadamard(qubit, ancilla_index, total_qubits);

                        size_t const ccc_idx = new_subtableaux.size();
                        new_subtableaux.push_back(std::move(ccc));
                        size_t const pmc_idx = new_subtableaux.size();
                        new_subtableaux.push_back(std::move(pmc));

                        gadget_pairs.push_back({ccc_idx, pmc_idx});
                        new_ancilla_states.push_back({ancilla_index, AncillaInitialState::PLUS});

                        spdlog::debug("Gadgetized H gate on qubit {} with ancilla {}", qubit, ancilla_index);
                        ++ancilla_index;
                    }

                    // Flush any remaining non-H ops after the last H gate.
                    if (!pending.empty()) {
                        StabilizerTableau remaining_st(total_qubits);
                        remaining_st.apply(pending);
                        new_subtableaux.push_back(std::move(remaining_st));
                    }
                },
                [&](std::vector<PauliRotation> const& rotations) {
                    std::vector<PauliRotation> copied;
                    copied.reserve(rotations.size());
                    for (auto const& rotation : rotations) {
                        PauliRotation r = rotation;
                        while (r.n_qubits() < total_qubits) {
                            r.add_ancilla_qubit();
                        }
                        copied.push_back(std::move(r));
                    }
                    new_subtableaux.push_back(std::move(copied));
                },
                [&](ClassicalControlTableau const& cct) {
                    new_subtableaux.push_back(cct);
                }},
            tableau[i]);
    }

    // ─── Assemble the new tableau ─────────────────────────────────────────────
    size_t const num_ancillae = total_qubits - original_n_qubits;

    tableau = Tableau(total_qubits);
    tableau.set_n_ancilla(num_ancillae);
    if (!tableau.is_empty()) {
        tableau.erase(tableau.begin(), tableau.end());
    }
    for (auto& subtableau : new_subtableaux) {
        tableau.push_back(std::move(subtableau));
    }
    tableau.set_n_ancilla(num_ancillae);

    for (auto const& [anc_idx, state] : new_ancilla_states) {
        tableau.add_ancilla_state(anc_idx, state);
        tableau.set_ancilla_measurement_type(anc_idx, MeasurementType::X);
    }

    for (auto const& [ccc_idx, pmc_idx] : gadget_pairs) {
        tableau.cct_pairing().emplace_back(ccc_idx, pmc_idx);
        spdlog::debug("Paired CCC at index {} with PMC at index {}", ccc_idx, pmc_idx);
    }
}

} // namespace

/**
 * @brief Minimize internal Hadamards, gadgetize H gates, commute classical operations, and optimize.
 *
 * @param tableau
 */



void minimize_internal_hadamards_n_gadgetize(Tableau& tableau) {
    size_t count              = 0;
    size_t non_clifford_count = tableau.n_pauli_rotations();
    // spdlog::debug("TMerge");
    merge_rotations(tableau);
    properize(tableau);
    minimize_internal_hadamards(tableau);
    // z_basisify_rotations_h_s_only(tableau);
    spdlog::debug("Minimized tableau : {:g}", tableau);
    gadgetize_tableau(tableau);
    spdlog::debug("Done internal hadamard minimization and gadgetization");
}

void blockwise_gadgetize(Tableau& tableau) {
    if (tableau.is_empty()) {
        return;
    }



    bool changed = true;
    while (changed) {
        changed = false;

        for (size_t mid = 1; mid + 1 < tableau.size(); ++mid) {
            auto* st_ptr = std::get_if<StabilizerTableau>(&tableau[mid]);
            if (!st_ptr) continue;

            auto* pr_left_ptr = std::get_if<std::vector<PauliRotation>>(&tableau[mid - 1]);
            auto* pr_right_ptr = std::get_if<std::vector<PauliRotation>>(&tableau[mid + 1]);
            if (!pr_left_ptr || !pr_right_ptr) continue;

            size_t const h_count = count_h_gates(*st_ptr);
            if (h_count == 0) continue;

            // This window introduces exactly h_count new ancilla qubits.
            size_t const old_total_qubits = tableau.n_qubits();
            size_t const new_total_qubits = old_total_qubits + h_count;
            spdlog::debug("Blockwise gadgetize: old_total_qubits={}, new_total_qubits={}", old_total_qubits, new_total_qubits);


            GadgetizedWindow w = gadgetize_one_internal_window(
                *pr_left_ptr, *st_ptr, *pr_right_ptr,
                new_total_qubits, old_total_qubits);

            Tableau local{w.total_qubits};
            local.set_n_ancilla(h_count);
            local.erase(local.begin(), local.end());
            for (auto& sub : w.subtableaux) {
                local.push_back(std::move(sub));
            }
            
            commute_and_merge_rotations(local);
            spdlog::debug("Local tableau after commute and merge: {:b}", local);
            optimize_phase_polynomial_with_classical(local, FastToddPhasePolynomialOptimizationStrategy{});
            spdlog::debug("Local tableau after phase polynomial optimization: {:b}", local);

            // Move ancilla-free PR columns behind PMCs.
            for (size_t i = 0; i < local.size(); ++i) {
                auto* pr = std::get_if<std::vector<PauliRotation>>(&local[i]);
                if (!pr) continue;

                auto [uses_anc, anc_free] = split_by_ancilla_support(*pr, tableau.n_qubits());
                *pr = std::move(uses_anc);

                if (!anc_free.empty()) {
                    size_t insert_pos = local.size();
                    if (insert_pos > 0 && std::holds_alternative<StabilizerTableau>(local.back())) {
                        insert_pos = local.size() - 1;
                    }
                    local.insert(
                        local.begin() + insert_pos,
                        SubTableau{std::move(anc_free)});
                }
                break;
            }
            // spdlog::debug("Local tableau after splitting by ancilla support: {:b}", local);

            remove_identities(local);

            // Replace the window PR, ST, PR by the optimized local segment.
            auto insert_it = tableau.begin() + (mid - 1);
            tableau.erase(tableau.begin() + (mid - 1), tableau.begin() + (mid + 2));
            tableau.insert(insert_it, local.begin(), local.end());


            changed = true;
            break;
        }
    }
}


/**
 * @brief Degadgetize a single CCC-PMC pair (removes ancilla for the whole circuit)
 * @param tableau The tableau containing the CCC and PMC
 * @param ccc_index The index of the CCC in the tableau
 * @param pmc_index The index of the PMC in the tableau
 */
void hadamard_degadgetize(Tableau& tableau, size_t ccc_index, size_t pmc_index) {

    spdlog::debug("Degadgetizing CCC at index {} and PMC at index {}", ccc_index, pmc_index);
    // Verify CCC at ccc_index
    auto* ccc_ptr = std::get_if<ClassicalControlTableau>(&tableau[ccc_index]);
    if (!ccc_ptr || !ccc_ptr->is_gadget()) {
        spdlog::error("Element at index {} is not a CCC", ccc_index);
        return;
    }
    
    // Verify PMC at pmc_index
    auto* pmc_ptr = std::get_if<ClassicalControlTableau>(&tableau[pmc_index]);
    if (!pmc_ptr || !pmc_ptr->is_classical_control()) {
        spdlog::error("Element at index {} is not a PMC", pmc_index);
        return;
    }
    
    // Verify they are correctly paired (check qubit pairs match)
    if (ccc_ptr->ancilla_qubit() != pmc_ptr->ancilla_qubit()) {
        spdlog::error("CCC at index {} and PMC at index {} have mismatched ancilla qubits ({} vs {})",
                     ccc_index, pmc_index, ccc_ptr->ancilla_qubit(), pmc_ptr->ancilla_qubit());
        return;
    }
    
    // Verify reference qubits match (required for Hadamard gadgets)
    size_t ccc_ref = ccc_ptr->reference_qubit();
    size_t pmc_ref = pmc_ptr->reference_qubit();
    if (ccc_ref != pmc_ref) {
        spdlog::error("CCC at index {} and PMC at index {} have mismatched reference qubits ({} vs {})",
                     ccc_index, pmc_index, ccc_ref, pmc_ref);
        return;
    }
    
    size_t ancilla_qubit = ccc_ptr->ancilla_qubit();
    size_t reference_qubit = ccc_ref;

    // Determine insertion position (where CCC was, before any removals)
    size_t insert_pos = ccc_index;
    
    tableau.erase(tableau.begin() + pmc_index);
    tableau.erase(tableau.begin() + ccc_index);



    // Insert H gate at the position where CCC was
    StabilizerTableau h_gate_tableau(tableau.n_qubits());
    h_gate_tableau.h(reference_qubit);
    tableau.insert(tableau.begin() + insert_pos, h_gate_tableau);
    
    for (size_t i = 0; i < tableau.size(); ++i) {
        if (std::holds_alternative<StabilizerTableau>(tableau[i])) {
            std::get<StabilizerTableau>(tableau[i]).remove_ancilla_qubit(ancilla_qubit);
        }
        if (std::holds_alternative<std::vector<PauliRotation>>(tableau[i])) {
            for (auto& rotation : std::get<std::vector<PauliRotation>>(tableau[i])) {
                rotation.remove_ancilla_qubit(ancilla_qubit);
            }
        }
        if (std::holds_alternative<ClassicalControlTableau>(tableau[i])) {
            if(std::get<ClassicalControlTableau>(tableau[i]).is_gadget()) {
                std::get<ClassicalControlTableau>(tableau[i]).remove_ancilla_qubit(ancilla_qubit);
            }
            if(std::get<ClassicalControlTableau>(tableau[i]).is_classical_control()) {
                std::get<ClassicalControlTableau>(tableau[i]).remove_ancilla_qubit(ancilla_qubit);
            }
        }
    }
    tableau.set_n_qubits(tableau.n_qubits() - 1);
    tableau.set_n_ancilla(tableau.n_ancilla() - 1);
    
    spdlog::debug("Degadgetized CCC at index {}: removed ancilla qubit {}, replaced with H gate on qubit {}",
                 ccc_index, ancilla_qubit, reference_qubit);
}


}  // namespace qsyn::experimental


