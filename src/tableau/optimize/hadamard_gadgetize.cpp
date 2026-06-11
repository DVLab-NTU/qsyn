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
#include <cstdlib>
#include <fmt/format.h>
#include <limits>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace qsyn::experimental {

namespace {

bool env_flag_enabled(char const* name, bool default_on) {
    if (char const* v = std::getenv(name)) {
        return !(v[0] == '0' || v[0] == 'n' || v[0] == 'N');
    }
    return default_on;
}

bool tableau_merge_rotations_enabled() {
    static bool const enabled = env_flag_enabled("QSYN_TABLEAU_MERGE_ROTATIONS", true);
    return enabled;
}

bool tableau_properize_enabled() {
    static bool const enabled = env_flag_enabled("QSYN_TABLEAU_PROPERIZE", true);
    return enabled;
}

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
 * @brief Verify CT·PR ≡ C_H·PR·CT' by collapsing CT|PR|adjoint(C_H|PR|CT').
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
 * @brief Gadgetize one H gate into a paired CCC/PMC CCT structure.
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
 * @brief Gadgetize internal-H windows using a two-pass count-then-rebuild flow.
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
        if (!std::holds_alternative<StabilizerTableau>(tableau[i])) continue;
        if (i == 0 || i + 1 == n) continue;
        if (!is_pr(i - 1) || !is_pr(i + 1)) continue;

        auto const ops = extract_clifford_operators(std::get<StabilizerTableau>(tableau[i]));
        internal_h_count += static_cast<size_t>(
            std::count_if(ops.begin(), ops.end(), [](CliffordOperator const& op) {
                return op.first == CliffordOperatorType::h;
            }));
    }

    size_t const total_qubits = original_n_qubits + internal_h_count;
    size_t ancilla_index      = original_n_qubits;

    // ─── Pass 2: rebuild tableau with all subtableaux pre-sized ──────────────
    std::vector<SubTableau> new_subtableaux;
    std::vector<std::pair<size_t, AncillaInitialState>> new_ancilla_states;

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

                        new_subtableaux.push_back(std::move(ccc));
                        new_subtableaux.push_back(std::move(pmc));

                        new_ancilla_states.push_back({ancilla_index, AncillaInitialState::PLUS});

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

    spdlog::info(
        "gadgetize_tableau: {} total qubits ({} data + {} ancilla) before minimization",
        total_qubits,
        original_n_qubits,
        num_ancillae);
}

} // namespace

/**
 * @brief Minimize internal Hadamards, gadgetize H gates, commute classical operations, and optimize.
 *
 * @param tableau
 */



std::unordered_map<size_t, PmcUnifiedPrRelation> minimize_internal_hadamards_n_gadgetize(Tableau& tableau) {
    size_t count              = 0;
    size_t non_clifford_count = tableau.n_pauli_rotations();
    if (tableau_merge_rotations_enabled()) {
        merge_rotations(tableau);
    }
    if (tableau_properize_enabled()) {
        properize(tableau);
    }
    minimize_internal_hadamards(tableau);
    gadgetize_tableau(tableau);
    auto pmc_to_unified_pr = commute_and_merge_rotations(tableau);
    spdlog::debug("Done internal hadamard minimization and gadgetization");
    return pmc_to_unified_pr;
}

/**
 * @brief Zipper-commute PMCs and PRs into canonical {CCC&ST}{PR}{PMC} form.
 *
 * @param tableau
 */
std::unordered_map<size_t, PmcUnifiedPrRelation> commute_and_merge_rotations(Tableau& tableau) {
    std::unordered_map<size_t, PmcUnifiedPrRelation> pmc_to_unified_pr;
    if (tableau.is_empty()) {
        return pmc_to_unified_pr;
    }

    // Extract the last StabilizerTableau (Clifford) if it exists, to add it back at the end
    std::optional<StabilizerTableau> last_clifford;
    auto* last_st = std::get_if<StabilizerTableau>(&tableau.back());
    if (last_st) {
        last_clifford = *last_st;
        tableau.erase(tableau.end() - 1, tableau.end());
    }

    auto const find_rightmost_pr_before = [&](size_t end_idx) -> std::optional<size_t> {
        for (size_t idx = end_idx; idx > 0; --idx) {
            size_t const actual_idx = idx - 1;
            if (std::holds_alternative<std::vector<PauliRotation>>(tableau[actual_idx])) {
                return actual_idx;
            }
        }
        return std::nullopt;
    };
    auto const move_pr_to_before_last_pr = [&](size_t pr_idx, size_t last_pr_idx) {
        auto* pr = std::get_if<std::vector<PauliRotation>>(&tableau[pr_idx]);
        if (pr == nullptr) {
            spdlog::error(
                "zipper commute: expected PR at index {}, got other subtableau",
                pr_idx);
            return;
        }

        for (size_t j = pr_idx + 1; j < last_pr_idx; ++j) {
            if (std::holds_alternative<StabilizerTableau>(tableau[j])) {
                continue;
            }
            if (auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[j]); cct != nullptr && cct->is_gadget()) {
                continue;
            }

            std::string_view blocker = "Unknown";
            if (std::holds_alternative<std::vector<PauliRotation>>(tableau[j])) {
                blocker = "PauliRotationTableau";
            } else if (auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[j]); cct != nullptr) {
                blocker = cct->is_classical_control() ? "ClassicalControlTableau(PMC)" : "ClassicalControlTableau(UnknownType)";
            }

            spdlog::error(
                "zipper commute export error: invalid block while moving PR. pr_idx={}, last_pr_idx={}, blocker_idx={}, blocker={}",
                pr_idx,
                last_pr_idx,
                j,
                blocker);
            spdlog::error("zipper commute export error: blocker subtableau:\n{:g}", tableau[j]);
            spdlog::error("zipper commute export error: current tableau:\n{:g}", tableau);
            throw std::logic_error("zipper commute export error: PR move path contains non-(gadget/ST) block");
        }

        swap_along(tableau, pr_idx, last_pr_idx - 1);
    };

    size_t pmc_move_count = 0;
    size_t pr_merge_count = 0;

    auto last_pr_idx_opt = find_rightmost_pr_before(tableau.size());
    if (!last_pr_idx_opt.has_value()) {
        throw std::logic_error("zipper commute: expected at least one PR block");
    }
    size_t last_pr_idx = *last_pr_idx_opt;

    // Zipper invariant: suffix is always [UnifiedPR][PMC*].
    // For circuits in alternating form PR1,CCT1,PR2,...,CCT(n-1),PRn,
    // run strict PMC -> PR alternating rounds.
    while (true) {
        auto previous_pr = find_rightmost_pr_before(last_pr_idx);
        if (!previous_pr.has_value()) {
            break;
        }

        // Move all PMCs between previous PR and unified PR first.
        while (true) {
            std::optional<size_t> pmc_idx;
            for (size_t idx = last_pr_idx; idx > *previous_pr + 1; --idx) {
                size_t const actual_idx = idx - 1;
                auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[actual_idx]);
                if (cct != nullptr && cct->is_classical_control()) {
                    pmc_idx = actual_idx;
                    break;
                }
            }
            if (!pmc_idx.has_value()) {
                break;
            }

            auto* pmc = std::get_if<ClassicalControlTableau>(&tableau[*pmc_idx]);
            if (pmc == nullptr || !pmc->is_classical_control()) {
                spdlog::error(
                    "zipper commute: expected PMC at index {}, got other subtableau",
                    *pmc_idx);
                break;
            }

            if (auto const* right_pr = std::get_if<std::vector<PauliRotation>>(&tableau[last_pr_idx]); right_pr != nullptr) {
                pmc_to_unified_pr[pmc->ancilla_qubit()].unified_pr_history.push_back(*right_pr);
            } else {
                spdlog::error(
                    "zipper commute: expected unified PR at index {}, got non-PR while exporting map",
                    last_pr_idx);
            }

            swap_along(tableau, *pmc_idx, last_pr_idx - 1);

            if (auto const* pmc_before = std::get_if<ClassicalControlTableau>(&tableau[last_pr_idx - 1]);
                pmc_before != nullptr && pmc_before->is_classical_control()) {
                auto const ancilla = pmc_before->ancilla_qubit();
                auto const ops = extract_clifford_operators(pmc_before->operations());
                auto& x_qubits = pmc_to_unified_pr[ancilla].x_qubits;
                for (auto const& op : ops) {
                    auto const& [type, qubits] = op;
                    if (type != CliffordOperatorType::x) {
                        continue;
                    }
                    size_t const q = qubits[0];
                    if (std::find(x_qubits.begin(), x_qubits.end(), q) == x_qubits.end()) {
                        x_qubits.push_back(q);
                    }
                }
            }

            swap_along(tableau, last_pr_idx - 1, last_pr_idx);
            --last_pr_idx;
            ++pmc_move_count;
        }

        move_pr_to_before_last_pr(*previous_pr, last_pr_idx);

        auto* left_pr = std::get_if<std::vector<PauliRotation>>(&tableau[last_pr_idx - 1]);
        auto* right_pr = std::get_if<std::vector<PauliRotation>>(&tableau[last_pr_idx]);
        if (left_pr == nullptr || right_pr == nullptr) {
            throw std::logic_error("zipper commute: expected adjacent PR blocks during merge");
        }
        left_pr->insert(
            left_pr->end(),
            std::make_move_iterator(right_pr->begin()),
            std::make_move_iterator(right_pr->end()));
        tableau.erase(tableau.begin() + static_cast<std::ptrdiff_t>(last_pr_idx));
        --last_pr_idx;  // Track the unified/rightmost PR index after each merge.
        ++pr_merge_count;
    }

    remove_identities(tableau);

    // Add back the last StabilizerTableau (Clifford) if it was extracted
    if (last_clifford.has_value()) {
        tableau.push_back(std::move(last_clifford.value()));
    }
    return pmc_to_unified_pr;
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
            spdlog::debug("Local tableau after commute and merge: {:g}", local);
            optimize_phase_polynomial_with_classical(local, FastToddPhasePolynomialOptimizationStrategy{});
            spdlog::debug("Local tableau after phase polynomial optimization: {:g}", local);

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

namespace {

struct AncillaOperationBlocker {
    size_t subtableau_index;
    std::string kind;
};

/** Return the first sub-tableau (not in skip_indices) that acts on ancilla_qubit. */
std::optional<AncillaOperationBlocker> find_operation_on_ancilla(
    Tableau const& tableau,
    size_t ancilla_qubit,
    std::unordered_set<size_t> const& skip_indices) {
    for (size_t i = 0; i < tableau.size(); ++i) {
        if (skip_indices.contains(i)) {
            continue;
        }

        if (auto const* st = std::get_if<StabilizerTableau>(&tableau[i])) {
            for (auto const& op : extract_clifford_operators(*st)) {
                if (ClassicalControlTableau::clifford_touches_ancilla(op, ancilla_qubit)) {
                    return AncillaOperationBlocker{i, "StabilizerTableau"};
                }
            }
            continue;
        }

        if (auto const* pr = std::get_if<std::vector<PauliRotation>>(&tableau[i])) {
            for (auto const& rotation : *pr) {
                if (ancilla_qubit < rotation.n_qubits() && !rotation.is_i(ancilla_qubit)) {
                    return AncillaOperationBlocker{i, "PauliRotation"};
                }
            }
            continue;
        }

        if (auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[i])) {
            for (auto const& op : extract_clifford_operators(cct->operations())) {
                if (ClassicalControlTableau::clifford_touches_ancilla(op, ancilla_qubit)) {
                    std::string const kind = cct->is_gadget()
                                                 ? "ClassicalControlTableau(Gadget)"
                                                 : "ClassicalControlTableau(PMC)";
                    return AncillaOperationBlocker{i, kind};
                }
            }
        }
    }
    return std::nullopt;
}

void move_pmcs_adjacent_to_gadgets(Tableau& tableau, std::vector<size_t> const& ancilla_qubits) {
    std::vector<size_t> ordered_ancillae = ancilla_qubits;
    std::ranges::sort(ordered_ancillae, [&tableau](size_t lhs, size_t rhs) {
        auto const lhs_pair = find_gadget_pair(tableau, lhs);
        auto const rhs_pair = find_gadget_pair(tableau, rhs);
        if (!lhs_pair.has_value()) {
            return false;
        }
        if (!rhs_pair.has_value()) {
            return true;
        }
        return lhs_pair->pmc_index > rhs_pair->pmc_index;
    });

    size_t moved_count = 0;
    for (size_t const ancilla : ordered_ancillae) {
        auto const pair_opt = find_gadget_pair(tableau, ancilla);
        if (!pair_opt.has_value()) {
            spdlog::warn("move_pmcs_adjacent_to_gadgets: missing CCC/PMC pair for ancilla {}",ancilla);
            continue;
        }
        size_t const gadget_idx = pair_opt->gadget_index;
        size_t const pmc_idx    = pair_opt->pmc_index;
        size_t const target_idx = gadget_idx + 1;
        if (pmc_idx <= target_idx) {
            continue;
        }
        swap_along(tableau, pmc_idx, target_idx);
        ++moved_count;
    }


}

std::optional<std::string> validate_degadgetize_pair(
    Tableau const& tableau,
    size_t ccc_index,
    size_t pmc_index,
    std::unordered_set<size_t> const& skip_subtableau_indices) {
    if (pmc_index != ccc_index + 1) {
        return fmt::format(
            "PMC at index {} is not adjacent to CCC at index {}",
            pmc_index,
            ccc_index);
    }

    if (ccc_index >= tableau.size()) {
        return fmt::format("CCC index {} out of range (tableau size {})", ccc_index, tableau.size());
    }
    if (pmc_index >= tableau.size()) {
        return fmt::format("PMC index {} out of range (tableau size {})", pmc_index, tableau.size());
    }

    auto const* ccc_ptr = std::get_if<ClassicalControlTableau>(&tableau[ccc_index]);
    if (ccc_ptr == nullptr || !ccc_ptr->is_gadget()) {
        return fmt::format("sub-tableau at index {} is not a CCC", ccc_index);
    }

    auto const* pmc_ptr = std::get_if<ClassicalControlTableau>(&tableau[pmc_index]);
    if (pmc_ptr == nullptr || !pmc_ptr->is_classical_control()) {
        return fmt::format("sub-tableau at index {} is not a PMC", pmc_index);
    }

    if (ccc_ptr->ancilla_qubit() != pmc_ptr->ancilla_qubit()) {
        return fmt::format(
            "CCC/PMC ancilla mismatch at indices {} and {} ({} vs {})",
            ccc_index,
            pmc_index,
            ccc_ptr->ancilla_qubit(),
            pmc_ptr->ancilla_qubit());
    }

    size_t const ccc_ref = ccc_ptr->reference_qubit();
    if (ccc_ref != pmc_ptr->reference_qubit()) {
        return fmt::format(
            "CCC/PMC reference mismatch at indices {} and {} ({} vs {})",
            ccc_index,
            pmc_index,
            ccc_ref,
            pmc_ptr->reference_qubit());
    }

    auto const pmc_ops = extract_clifford_operators(pmc_ptr->operations());
    bool const pmc_is_single_ref_x =
        pmc_ops.size() == 1 &&
        pmc_ops[0].first == CliffordOperatorType::x &&
        pmc_ops[0].second[0] == ccc_ref;
    if (!pmc_is_single_ref_x) {
        return fmt::format(
            "PMC at index {} is not exactly one X on reference qubit {} (got {})",
            pmc_index,
            ccc_ref,
            clifford_ops_to_string(pmc_ops));
    }

    size_t const ancilla_qubit = ccc_ptr->ancilla_qubit();
    if (auto const blocker = find_operation_on_ancilla(
            tableau, ancilla_qubit, skip_subtableau_indices)) {
        return fmt::format(
            "ancilla {} still used by {} at index {}",
            ancilla_qubit,
            blocker->kind,
            blocker->subtableau_index);
    }

    return std::nullopt;
}

void apply_degadgetize_pair(Tableau& tableau, size_t ccc_index) {
    auto* ccc_ptr = std::get_if<ClassicalControlTableau>(&tableau[ccc_index]);
    if (ccc_ptr == nullptr || !ccc_ptr->is_gadget()) {
        throw std::logic_error(
            fmt::format("apply_degadgetize_pair: expected CCC at index {}", ccc_index));
    }

    size_t const pmc_index        = ccc_index + 1;
    size_t const ancilla_qubit    = ccc_ptr->ancilla_qubit();
    size_t const reference_qubit  = ccc_ptr->reference_qubit();
    size_t const insert_pos       = ccc_index;

    tableau.erase(tableau.begin() + static_cast<std::ptrdiff_t>(pmc_index));
    tableau.erase(tableau.begin() + static_cast<std::ptrdiff_t>(ccc_index));

    StabilizerTableau h_gate_tableau(tableau.n_qubits());
    h_gate_tableau.h(reference_qubit);
    tableau.insert(tableau.begin() + static_cast<std::ptrdiff_t>(insert_pos), h_gate_tableau);

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
            if (std::get<ClassicalControlTableau>(tableau[i]).is_gadget()) {
                std::get<ClassicalControlTableau>(tableau[i]).remove_ancilla_qubit(ancilla_qubit);
            }
            if (std::get<ClassicalControlTableau>(tableau[i]).is_classical_control()) {
                std::get<ClassicalControlTableau>(tableau[i]).remove_ancilla_qubit(ancilla_qubit);
            }
        }
    }
    tableau.set_n_qubits(tableau.n_qubits() - 1);
    tableau.set_n_ancilla(tableau.n_ancilla() - 1);

    spdlog::debug(
        "Degadgetized CCC at index {}: removed ancilla qubit {}, replaced with H gate on qubit {}",
        ccc_index,
        ancilla_qubit,
        reference_qubit);
}

}  // namespace

size_t hadamard_degadgetize(Tableau& tableau,
                            std::vector<size_t> const& ancilla_candidates,
                            std::vector<size_t>* removed_ancillae) {
    if (removed_ancillae != nullptr) {
        removed_ancillae->clear();
    }
    if (ancilla_candidates.empty()) {
        return 0;
    }
    std::unordered_set<size_t> candidate_pair_indices;
    for (size_t const ancilla : ancilla_candidates) {
        if (auto const pair_indices = find_gadget_pair(tableau, ancilla)) {
            candidate_pair_indices.insert(pair_indices->gadget_index);
            candidate_pair_indices.insert(pair_indices->pmc_index);
        }
    }

    std::vector<size_t> eligible_ancillae;
    eligible_ancillae.reserve(ancilla_candidates.size());
    for (size_t const ancilla : ancilla_candidates) {
        auto const pair_indices = find_gadget_pair(tableau, ancilla);
        if (!pair_indices.has_value()) {
            spdlog::warn(
                "hadamard_degadgetize: no CCC/PMC pairing for ancilla {}",
                ancilla);
            continue;
        }

        size_t const ccc_index = pair_indices->gadget_index;
        size_t const pmc_index = pair_indices->pmc_index;
        if (auto const error = validate_degadgetize_pair(
                tableau, ccc_index, pmc_index, candidate_pair_indices)) {
            spdlog::warn(
                "hadamard_degadgetize: skipping ancilla {}: {}",
                ancilla,
                *error);
            continue;
        }
        eligible_ancillae.push_back(ancilla);
    }

    spdlog::info(
        "hadamard_degadgetize: {}/{} candidates eligible after PMC move",
        eligible_ancillae.size(),
        ancilla_candidates.size());

    std::ranges::sort(eligible_ancillae, std::greater{});

    size_t degadgetized_count = 0;
    for (size_t const ancilla : eligible_ancillae) {
        auto const pair_indices = find_gadget_pair(tableau, ancilla);
        if (!pair_indices.has_value()) {
            spdlog::warn(
                "hadamard_degadgetize: no CCC/PMC pairing for ancilla {}",
                ancilla);
            continue;
        }

        size_t const ccc_index = pair_indices->gadget_index;
        auto* ccc_ptr = std::get_if<ClassicalControlTableau>(&tableau[ccc_index]);
        if (ccc_ptr == nullptr || !ccc_ptr->is_gadget()) {
            spdlog::warn(
                "hadamard_degadgetize: CCC missing at index {} for ancilla {}",
                ccc_index,
                ancilla);
            continue;
        }

        apply_degadgetize_pair(tableau, ccc_index);
        if (removed_ancillae != nullptr) {
            removed_ancillae->push_back(ancilla);
        }
        ++degadgetized_count;
    }

    return degadgetized_count;
}


}  // namespace qsyn::experimental


