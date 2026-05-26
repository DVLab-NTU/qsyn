#include "../tableau_optimization.hpp"

#include <algorithm>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tableau/classical_tableau.hpp"

namespace qsyn {

namespace experimental {

bool SignatureTensor::equivalent(SignatureTensor const& other) const {
    return n_qubits == other.n_qubits &&
           linear_mod8 == other.linear_mod8 &&
           quadratic_mod4 == other.quadratic_mod4 &&
           cubic_mod2 == other.cubic_mod2;
}

SignatureTensor::TouchingTerms SignatureTensor::get_touching(size_t qubit) const {
    TouchingTerms touching{};
    if (qubit < linear_mod8.size() && linear_mod8[qubit] != 0) {
        touching.linear_mod8 = linear_mod8[qubit];
    }
    touching.quadratic_terms = get_quadratic_terms_touching(qubit);
    touching.cubic_terms = get_cubic_terms_touching(qubit);
    return touching;
}

std::vector<std::pair<SignatureTensor::PairTerm, uint8_t>> SignatureTensor::get_quadratic_terms_touching(size_t qubit) const {
    std::vector<std::pair<PairTerm, uint8_t>> result;
    auto const it = quadratic_by_qubit.find(qubit);
    if (it == quadratic_by_qubit.end()) {
        return result;
    }
    result.reserve(it->second.size());
    for (auto const& term : it->second) {
        if (auto coeff_it = quadratic_mod4.find(term); coeff_it != quadratic_mod4.end()) {
            result.emplace_back(term, coeff_it->second);
        }
    }
    return result;
}

std::vector<SignatureTensor::TripleTerm> SignatureTensor::get_cubic_terms_touching(size_t qubit) const {
    std::vector<TripleTerm> result;
    auto const it = cubic_by_qubit.find(qubit);
    if (it == cubic_by_qubit.end()) {
        return result;
    }
    result.reserve(it->second.size());
    for (auto const& term : it->second) {
        result.push_back(term);
    }
    return result;
}

SignatureTensor get_signature(std::vector<PauliRotation> const& rotations) {
    SignatureTensor signature{};
    if (rotations.empty()) {
        return signature;
    }

    signature.n_qubits = rotations.front().n_qubits();
    signature.linear_mod8.assign(signature.n_qubits, 0);

    auto const phase_to_mod8 = [](dvlab::Phase const& phase) -> uint8_t {
        auto const denominator = phase.denominator();
        if (denominator == 0 || (4 % denominator) != 0) {
            throw std::logic_error("get_signature: phase denominator must divide 4");
        }
        auto const scaled = phase.numerator() * static_cast<int64_t>(4 / denominator);
        auto mod8         = scaled % 8;
        if (mod8 < 0) mod8 += 8;
        return static_cast<uint8_t>(mod8);
    };

    for (auto const& rotation : rotations) {
        if (rotation.n_qubits() != signature.n_qubits) {
            throw std::logic_error("get_signature: all PauliRotation columns must have the same n_qubits");
        }
        if (!rotation.is_diagonal()) {
            throw std::logic_error("get_signature: all PauliRotation columns must be diagonal");
        }

        uint8_t const k = phase_to_mod8(rotation.phase());
        if (k == 0) {
            continue;
        }
        std::vector<size_t> z_support;
        z_support.reserve(signature.n_qubits);
        for (size_t q = 0; q < signature.n_qubits; ++q) {
            if (rotation.pauli_product().is_z_set(q)) {
                z_support.push_back(q);
            }
        }
        if (z_support.empty()) {
            continue;
        }

        for (size_t const q : z_support) {
            signature.linear_mod8[q] = static_cast<uint8_t>((signature.linear_mod8[q] + k) % 8);
        }

        uint8_t const k_mod4 = static_cast<uint8_t>(k % 4);
        if (k_mod4 != 0) {
            for (size_t a = 0; a < z_support.size(); ++a) {
                for (size_t b = a + 1; b < z_support.size(); ++b) {
                    auto const term = SignatureTensor::PairTerm::canonical(z_support[a], z_support[b]);
                    auto const it   = signature.quadratic_mod4.find(term);
                    uint8_t const old_coeff = (it == signature.quadratic_mod4.end()) ? 0 : it->second;
                    uint8_t const new_coeff = static_cast<uint8_t>((old_coeff + k_mod4) % 4);
                    if (new_coeff == 0) {
                        if (it != signature.quadratic_mod4.end()) {
                            signature.quadratic_mod4.erase(it);
                            signature.quadratic_by_qubit[term.i].erase(term);
                            signature.quadratic_by_qubit[term.j].erase(term);
                        }
                    } else {
                        signature.quadratic_mod4[term] = new_coeff;
                        signature.quadratic_by_qubit[term.i].insert(term);
                        signature.quadratic_by_qubit[term.j].insert(term);
                    }
                }
            }
        }

        if ((k % 2) == 1) {
            for (size_t a = 0; a < z_support.size(); ++a) {
                for (size_t b = a + 1; b < z_support.size(); ++b) {
                    for (size_t c = b + 1; c < z_support.size(); ++c) {
                        auto const term = SignatureTensor::TripleTerm::canonical(z_support[a], z_support[b], z_support[c]);
                        if (signature.cubic_mod2.contains(term)) {
                            signature.cubic_mod2.erase(term);
                            signature.cubic_by_qubit[term.i].erase(term);
                            signature.cubic_by_qubit[term.j].erase(term);
                            signature.cubic_by_qubit[term.k].erase(term);
                        } else {
                            signature.cubic_mod2.insert(term);
                            signature.cubic_by_qubit[term.i].insert(term);
                            signature.cubic_by_qubit[term.j].insert(term);
                            signature.cubic_by_qubit[term.k].insert(term);
                        }
                    }
                }
            }
        }
    }

    return signature;
}

namespace {

void normalize_touching_terms(SignatureTensor::TouchingTerms& touching) {
    std::ranges::sort(
        touching.quadratic_terms,
        [](auto const& lhs, auto const& rhs) {
            auto const& [lhs_term, lhs_coeff] = lhs;
            auto const& [rhs_term, rhs_coeff] = rhs;
            if (lhs_term.i != rhs_term.i) return lhs_term.i < rhs_term.i;
            if (lhs_term.j != rhs_term.j) return lhs_term.j < rhs_term.j;
            return lhs_coeff < rhs_coeff;
        });
    std::ranges::sort(
        touching.cubic_terms,
        [](auto const& lhs, auto const& rhs) {
            if (lhs.i != rhs.i) return lhs.i < rhs.i;
            if (lhs.j != rhs.j) return lhs.j < rhs.j;
            return lhs.k < rhs.k;
        });
}

bool touching_terms_equivalent(
    SignatureTensor::TouchingTerms lhs,
    SignatureTensor::TouchingTerms rhs) {
    normalize_touching_terms(lhs);
    normalize_touching_terms(rhs);
    return lhs.linear_mod8 == rhs.linear_mod8 &&
           lhs.quadratic_terms == rhs.quadratic_terms &&
           lhs.cubic_terms == rhs.cubic_terms;
}

}  // namespace

std::vector<SignatureComparisonResult> compare_pp(
    std::vector<std::vector<PauliRotation>> const& unified_pr_history,
    std::vector<PauliRotation> const& pr_pmc_ij,
    std::vector<size_t> const& x_qubits) {
    std::vector<SignatureComparisonResult> comparison_results;
    comparison_results.reserve(unified_pr_history.size());

    auto const reduced_signature = get_signature(pr_pmc_ij);
    for (size_t history_idx = 0; history_idx < unified_pr_history.size(); ++history_idx) {
        auto const& unified_pr = unified_pr_history[history_idx];
        auto const unified_signature = get_signature(unified_pr);

        SignatureComparisonResult result;
        result.history_index = history_idx;
        result.per_qubit_comparisons.reserve(x_qubits.size());

        for (size_t const qubit : x_qubits) {
            TouchingTermComparison per_qubit;
            per_qubit.qubit = qubit;
            per_qubit.unified_terms = unified_signature.get_touching(qubit);
            per_qubit.reduced_terms = reduced_signature.get_touching(qubit);
            per_qubit.equivalent = touching_terms_equivalent(per_qubit.unified_terms, per_qubit.reduced_terms);
            result.per_qubit_comparisons.push_back(std::move(per_qubit));
        }

        comparison_results.push_back(std::move(result));
    }

    return comparison_results;
}

std::vector<size_t> extract_pmc_x_qubits(ClassicalControlTableau const& pmc) {
    std::vector<size_t> x_qubits;
    for (auto const& op : extract_clifford_operators(pmc.operations())) {
        auto const& [type, qubits] = op;
        if (type != CliffordOperatorType::x) {
            continue;
        }
        size_t const q = qubits[0];
        if (std::find(x_qubits.begin(), x_qubits.end(), q) == x_qubits.end()) {
            x_qubits.push_back(q);
        }
    }
    return x_qubits;
}

std::vector<PauliRotation> find_pr_blocking_rotations(
    std::vector<PauliRotation> const& pr,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits) {
    std::vector<PauliRotation> pr_blocking;
    pr_blocking.reserve(pr.size());
    for (auto const& rotation : pr) {
        bool has_z_on_any_x = false;
        for (size_t const q : x_qubits) {
            if (q < rotation.n_qubits() && rotation.is_z(q)) {
                has_z_on_any_x = true;
                break;
            }
        }
        if (rotation.is_z(ancilla_qubit) && has_z_on_any_x) {
            pr_blocking.push_back(rotation);
        }
    }
    return pr_blocking;
}

PmcPrBlockingAnalysis analyze_pmc_pr_blocking(
    ClassicalControlTableau const& pmc,
    std::vector<PauliRotation> const& unified_pr) {
    PmcPrBlockingAnalysis analysis;
    analysis.reference_qubit = pmc.reference_qubit();
    analysis.ancilla_qubit   = pmc.ancilla_qubit();
    analysis.x_qubits        = extract_pmc_x_qubits(pmc);
    analysis.pr_blocking =
        find_pr_blocking_rotations(unified_pr, analysis.ancilla_qubit, analysis.x_qubits);
    analysis.is_degadgetizable = analysis.pr_blocking.empty();
    return analysis;
}

namespace {

std::optional<size_t> find_unified_pr_index_for_sat(Tableau const& tableau) {
    size_t idx = 1;
    while (idx < tableau.size()) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct != nullptr && cct->is_gadget()) {
            ++idx;
            continue;
        }
        if (std::holds_alternative<StabilizerTableau>(tableau[idx])) {
            ++idx;
            continue;
        }
        break;
    }
    if (idx < tableau.size() && std::holds_alternative<std::vector<PauliRotation>>(tableau[idx])) {
        return idx;
    }
    return std::nullopt;
}

void classify_pr_for_gadget(
    PauliRotation const& rotation,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits,
    bool& in_block_left,
    bool& in_block_right) {
    in_block_left  = false;
    in_block_right = false;
    bool has_z_on_any_x = false;
    for (size_t const q : x_qubits) {
        if (q < rotation.n_qubits() && rotation.is_z(q)) {
            has_z_on_any_x = true;
            break;
        }
    }
    bool const z_on_ancilla = rotation.is_z(ancilla_qubit);
    if (z_on_ancilla && has_z_on_any_x) {
        in_block_left  = true;
        in_block_right = true;
    } else if (z_on_ancilla) {
        in_block_right = true;
    } else if (has_z_on_any_x) {
        in_block_left = true;
    }
}

}  // namespace

SatSignatureExport compute_sat_signature_blocks(Tableau const& tableau) {
    SatSignatureExport out;
    out.qubit_count   = tableau.n_qubits();
    out.ancilla_count = tableau.n_ancilla();

    Tableau tableau_copy = tableau;
    auto gadgets         = export_hadamard_gadget_pairs(tableau_copy);
    size_t const num_gadgets = gadgets.size();

    std::vector<PauliRotation> unified_pr;
    auto const pr_idx_opt = find_unified_pr_index_for_sat(tableau);
    if (pr_idx_opt.has_value()) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[*pr_idx_opt]);
        if (pr_vec != nullptr) {
            unified_pr = *pr_vec;
        }
    }
    out.pauli_count = unified_pr.size();

    out.blocks_by_gid.resize(num_gadgets);
    for (size_t g_idx = 0; g_idx < num_gadgets; ++g_idx) {
        out.blocks_by_gid[g_idx].gid           = g_idx;
        out.blocks_by_gid[g_idx].ancilla_qubit = gadgets[g_idx].ancilla_qubit;
    }

    std::vector<size_t> gadget_rank_order(num_gadgets);
    std::iota(gadget_rank_order.begin(), gadget_rank_order.end(), 0);
    std::ranges::sort(gadget_rank_order, [&](size_t a, size_t b) {
        if (gadgets[a].ancilla_qubit != gadgets[b].ancilla_qubit) {
            return gadgets[a].ancilla_qubit < gadgets[b].ancilla_qubit;
        }
        return gadgets[a].ccc_index < gadgets[b].ccc_index;
    });
    out.gadget_order = gadget_rank_order;

    for (size_t g_idx = 0; g_idx < num_gadgets; ++g_idx) {
        auto const& gadget = gadgets[g_idx];
        size_t const ancilla = gadget.ancilla_qubit;

        std::vector<size_t> x_qubits;
        auto const pair_opt = find_gadget_pair(tableau, ancilla);
        if (pair_opt.has_value()) {
            auto const* pmc =
                std::get_if<ClassicalControlTableau>(&tableau[pair_opt->pmc_index]);
            if (pmc != nullptr) {
                x_qubits = extract_pmc_x_qubits(*pmc);
            }
        }

        auto& lists = out.blocks_by_gid[g_idx];
        for (size_t pr_idx = 0; pr_idx < unified_pr.size(); ++pr_idx) {
            bool in_left  = false;
            bool in_right = false;
            classify_pr_for_gadget(unified_pr[pr_idx], ancilla, x_qubits, in_left, in_right);
            size_t const pid = num_gadgets + pr_idx;
            if (in_left) {
                lists.block_left.push_back(pid);
            }
            if (in_right) {
                lists.block_right.push_back(pid);
            }
        }
        std::ranges::sort(lists.block_left);
        std::ranges::sort(lists.block_right);
    }

    spdlog::info(
        "compute_sat_signature_blocks: {} gadgets, {} PRs, fixed order by ancilla",
        num_gadgets,
        out.pauli_count);
    return out;
}

BlockingSignatureInfo analyze_blocking_signature(
    std::vector<PauliRotation> const& pr_blocking,
    std::vector<size_t> const& x_qubits,
    size_t ancilla_qubit) {
    BlockingSignatureInfo info;
    info.signature = get_signature(pr_blocking);

    auto const touching_ancilla = info.signature.get_touching(ancilla_qubit);
    info.ancilla_in_signature =
        touching_ancilla.linear_mod8.has_value() ||
        !touching_ancilla.quadratic_terms.empty() ||
        !touching_ancilla.cubic_terms.empty();

    if (!info.ancilla_in_signature) {
        return info;
    }

    std::unordered_set<size_t> const x_set(x_qubits.begin(), x_qubits.end());

    for (auto const& [term, coeff] : touching_ancilla.quadratic_terms) {
        (void)coeff;
        size_t const other = (term.i == ancilla_qubit) ? term.j : term.i;
        if (x_set.contains(other)) {
            info.has_ancilla_x_overlap = true;
            return info;
        }
    }

    for (auto const& term : touching_ancilla.cubic_terms) {
        for (size_t const q : {term.i, term.j, term.k}) {
            if (q != ancilla_qubit && x_set.contains(q)) {
                info.has_ancilla_x_overlap = true;
                return info;
            }
        }
    }

    return info;
}

void move_pmcs_with_reduced_PR(Tableau const& tableau, std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr) {
    Tableau working = tableau;

    // Canonical structure expected after commute-and-merge:
    //   {ST0, (CCCs|intermediate STs)..., PR, (optional CX-ST), PMCs..., ST_back}
    size_t idx = 1;
    while (idx < working.size()) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&working[idx]);
        if (cct != nullptr && cct->is_gadget()) {
            ++idx;
            continue;
        }
        if (std::holds_alternative<StabilizerTableau>(working[idx])) {
            ++idx;
            continue;
        }
        break;
    }
    if (idx >= working.size() || !std::holds_alternative<std::vector<PauliRotation>>(working[idx])) {
        spdlog::error("move_pmcs_with_reduced_PR: missing PR block");
        return;
    }
    size_t pr_idx = idx;

    while (pr_idx + 1 < working.size()) {
        size_t pmc_idx = pr_idx + 1;
        auto* pmc = std::get_if<ClassicalControlTableau>(&working[pmc_idx]);

        if (pmc == nullptr || !pmc->is_classical_control()) {
            break;
        }
        size_t const ancilla = pmc->ancilla_qubit();

        auto const pair_opt = find_gadget_pair(working, ancilla);
        if (!pair_opt.has_value() || pair_opt->gadget_index >= pr_idx) {
            spdlog::error(
                "move_pmcs_with_reduced_PR: missing gadget counterpart for ancilla {}",
                ancilla);
            return;
        }
        size_t const gadget_idx = pair_opt->gadget_index;

        auto const relation_it = pmc_to_unified_pr.find(ancilla);
        std::vector<size_t> const empty_x_qubits;
        auto const& x_qubits =
            (relation_it != pmc_to_unified_pr.end()) ? relation_it->second.x_qubits : empty_x_qubits;
        auto* unified_pr = std::get_if<std::vector<PauliRotation>>(&working[pr_idx]);
        if (unified_pr == nullptr) {
            spdlog::error(
                "move_pmcs_with_reduced_PR: expected unified PR block at index {}",
                pr_idx);
            return;
        }
        auto const unified_pr_original = *unified_pr;

        // Build BOTH splits of the unified PR:
        //   pr_commuting / pr_commuting_rest: original rule (terms with Z on ancilla go to rest).
        //   pr_ancilla   / pr_ancilla_rest:   ignore ancilla phase (Z-on-any-x_qubit goes to pr_ancilla).
        // pr_blocking collects the set difference pr_ancilla \ pr_commuting: rotations whose
        // Z-support hits BOTH the ancilla AND some x_qubit. These are the blocking terms.
        std::vector<PauliRotation> pr_commuting;
        std::vector<PauliRotation> pr_commuting_rest;
        std::vector<PauliRotation> pr_ancilla;
        std::vector<PauliRotation> pr_ancilla_rest;
        std::vector<PauliRotation> pr_blocking;
        pr_commuting.reserve(unified_pr_original.size());
        pr_commuting_rest.reserve(unified_pr_original.size());
        pr_ancilla.reserve(unified_pr_original.size());
        pr_ancilla_rest.reserve(unified_pr_original.size());
        pr_blocking.reserve(unified_pr_original.size());

        for (auto const& rotation : unified_pr_original) {
            bool has_z_on_any_x = false;
            for (size_t const q : x_qubits) {
                if (q < rotation.n_qubits() && rotation.is_z(q)) {
                    has_z_on_any_x = true;
                    break;
                }
            }
            bool const z_on_ancilla = rotation.is_z(ancilla);
            if (z_on_ancilla) {
                pr_commuting_rest.push_back(rotation);
            } else if (has_z_on_any_x) {
                pr_commuting.push_back(rotation);
            } else {
                pr_commuting_rest.push_back(rotation);
            }
            if (has_z_on_any_x) {
                pr_ancilla.push_back(rotation);
            } else {
                pr_ancilla_rest.push_back(rotation);
            }
        }
        pr_blocking = find_pr_blocking_rotations(unified_pr_original, ancilla, x_qubits);

        auto const run_compare = [&](std::vector<PauliRotation> const& pr_split)
            -> std::optional<std::vector<SignatureComparisonResult>> {
            if (relation_it == pmc_to_unified_pr.end() ||
                relation_it->second.unified_pr_history.empty()) {
                return std::nullopt;
            }
            return compare_pp(relation_it->second.unified_pr_history, pr_split, x_qubits);
        };
        auto const cmp_commuting = run_compare(pr_commuting);
        auto const cmp_ancilla   = run_compare(pr_ancilla);

        struct CommuteOutcome {
            Tableau working_after;
            bool success = false;
            size_t new_pr_idx = 0;
            size_t reference_qubit = 0;
            bool is_single_x_on_reference = false;
            explicit CommuteOutcome(Tableau t) : working_after(std::move(t)) {}
        };
        auto const run_commute = [&](std::vector<PauliRotation> pr_split,
                                     std::vector<PauliRotation> pr_rest) -> CommuteOutcome {
            CommuteOutcome out{working};
            Tableau& w = out.working_after;

            w.erase(
                w.begin() + static_cast<std::ptrdiff_t>(pr_idx),
                w.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1));
            w.insert(
                w.begin() + static_cast<std::ptrdiff_t>(pr_idx),
                SubTableau{std::move(pr_rest)});
            w.insert(
                w.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1),
                SubTableau{std::move(pr_split)});

            [[maybe_unused]] auto rest_pr_prime = swap_along_test(w, pr_idx, 1);

            w.erase(
                w.begin() + static_cast<std::ptrdiff_t>(pr_idx),
                w.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1));

            size_t const target_idx = gadget_idx + 1;
            swap_along(w, pr_idx + 1, target_idx);

            size_t const pmc_ij_idx = pr_idx + 1;
            if (pmc_ij_idx >= w.size()) {
                return out;
            }
            w.erase(
                w.begin() + static_cast<std::ptrdiff_t>(pmc_ij_idx),
                w.begin() + static_cast<std::ptrdiff_t>(pmc_ij_idx + 1));
            w.insert(
                w.begin() + static_cast<std::ptrdiff_t>(pmc_ij_idx),
                SubTableau{unified_pr_original});
            out.new_pr_idx = pmc_ij_idx;

            auto* moved_pmc = std::get_if<ClassicalControlTableau>(&w[target_idx]);
            if (moved_pmc == nullptr || !moved_pmc->is_classical_control()) {
                return out;
            }
            auto const* gadget = std::get_if<ClassicalControlTableau>(&w[gadget_idx]);
            if (gadget == nullptr) {
                return out;
            }
            out.reference_qubit = gadget->reference_qubit();
            auto const ops = extract_clifford_operators(moved_pmc->operations());
            out.is_single_x_on_reference =
                ops.size() == 1 &&
                ops.front().first == CliffordOperatorType::x &&
                ops.front().second[0] == out.reference_qubit;
            out.success = true;
            return out;
        };

        auto commuting_out = run_commute(pr_commuting, pr_commuting_rest);
        auto ancilla_out   = run_commute(pr_ancilla, pr_ancilla_rest);

        size_t const reference_qubit =
            commuting_out.success ? commuting_out.reference_qubit
            : ancilla_out.success ? ancilla_out.reference_qubit
                                  : 0;

        auto const count_diff_terms = [](TouchingTermComparison const& per_qubit) -> std::pair<size_t, size_t> {
            size_t unified_diff_terms = 0;
            size_t reduced_diff_terms = 0;

            if (per_qubit.unified_terms.linear_mod8 != per_qubit.reduced_terms.linear_mod8) {
                if (per_qubit.unified_terms.linear_mod8.has_value()) ++unified_diff_terms;
                if (per_qubit.reduced_terms.linear_mod8.has_value()) ++reduced_diff_terms;
            }

            std::unordered_map<SignatureTensor::PairTerm, uint8_t, SignatureTensor::PairTermHash> u_quad;
            std::unordered_map<SignatureTensor::PairTerm, uint8_t, SignatureTensor::PairTermHash> r_quad;
            for (auto const& [t, c] : per_qubit.unified_terms.quadratic_terms) u_quad[t] = c;
            for (auto const& [t, c] : per_qubit.reduced_terms.quadratic_terms) r_quad[t] = c;
            for (auto const& [t, c] : u_quad) {
                auto const it = r_quad.find(t);
                if (it == r_quad.end() || it->second != c) ++unified_diff_terms;
            }
            for (auto const& [t, c] : r_quad) {
                auto const it = u_quad.find(t);
                if (it == u_quad.end() || it->second != c) ++reduced_diff_terms;
            }

            std::unordered_set<SignatureTensor::TripleTerm, SignatureTensor::TripleTermHash> u_cubic(
                per_qubit.unified_terms.cubic_terms.begin(),
                per_qubit.unified_terms.cubic_terms.end());
            std::unordered_set<SignatureTensor::TripleTerm, SignatureTensor::TripleTermHash> r_cubic(
                per_qubit.reduced_terms.cubic_terms.begin(),
                per_qubit.reduced_terms.cubic_terms.end());
            for (auto const& t : u_cubic) if (!r_cubic.contains(t)) ++unified_diff_terms;
            for (auto const& t : r_cubic) if (!u_cubic.contains(t)) ++reduced_diff_terms;

            return {unified_diff_terms, reduced_diff_terms};
        };

        // Invariant: the ancilla variant always reduces to a single X on the reference qubit
        // and matches the historical PR per-qubit. Anything else is a logic error.
        if (!ancilla_out.is_single_x_on_reference) {
            throw std::logic_error(fmt::format(
                "move_pmcs_with_reduced_PR: ancilla variant did not reduce to single X on reference (ancilla={}, ref_q={})",
                ancilla, reference_qubit));
        }
        if (cmp_ancilla.has_value()) {
            for (auto const& comparison : *cmp_ancilla) {
                for (auto const& per_qubit : comparison.per_qubit_comparisons) {
                    if (!per_qubit.equivalent) {
                        throw std::logic_error(fmt::format(
                            "move_pmcs_with_reduced_PR: ancilla variant per-qubit equivalence failed (ancilla={}, qubit={})",
                            ancilla, per_qubit.qubit));
                    }
                }
            }
        }

        bool const success =
            commuting_out.is_single_x_on_reference && ancilla_out.is_single_x_on_reference;

        spdlog::info(
            "PMC({},{}): x_qubits:[{}] pr-blocking:{}",
            reference_qubit,
            ancilla,
            fmt::join(x_qubits, ","),
            pr_blocking.size());
        spdlog::info(
            "  reversing of classical control {}",
            success ? "success" : "failed");

        if (success) {
            if (!pr_blocking.empty()) {
                throw std::logic_error(fmt::format(
                    "move_pmcs_with_reduced_PR: success but pr_blocking non-empty (ancilla={}, size={})",
                    ancilla, pr_blocking.size()));
            }
            if (cmp_commuting.has_value()) {
                for (auto const& comparison : *cmp_commuting) {
                    for (auto const& per_qubit : comparison.per_qubit_comparisons) {
                        if (!per_qubit.equivalent) {
                            throw std::logic_error(fmt::format(
                                "move_pmcs_with_reduced_PR: success but commuting per-qubit equivalence failed (ancilla={}, qubit={})",
                                ancilla, per_qubit.qubit));
                        }
                    }
                }
            }
        } else {
            for (auto const& rotation : pr_blocking) {
                spdlog::info("    pr_blocking: {}", rotation.to_bit_string());
            }
            auto const blocking_info = analyze_blocking_signature(pr_blocking, x_qubits, ancilla);
            spdlog::info(
                "    pr_blocking signature: ancilla_in_signature={} has_ancilla_x_overlap={}",
                blocking_info.ancilla_in_signature,
                blocking_info.has_ancilla_x_overlap);
            if (cmp_commuting.has_value()) {
                for (auto const& comparison : *cmp_commuting) {
                    for (auto const& per_qubit : comparison.per_qubit_comparisons) {
                        auto const [unified_diff_terms, reduced_diff_terms] = count_diff_terms(per_qubit);
                        spdlog::info(
                            "    qubit={} equivalent={} diff_terms(unified={}, reduced={})",
                            per_qubit.qubit,
                            per_qubit.equivalent,
                            unified_diff_terms,
                            reduced_diff_terms);
                    }
                }
            }
        }

        if (!commuting_out.success) {
            spdlog::error(
                "move_pmcs_with_reduced_PR: commuting variant failed for ancilla {}",
                ancilla);
            return;
        }
        working = std::move(commuting_out.working_after);
        pr_idx = commuting_out.new_pr_idx;
    }
}

void minimize_ancillary_t_opt_with_degadgetization(Tableau& tableau, std::optional<std::string> export_filename) {
    (void)export_filename;
    if (tableau.is_empty()) {
        return;
    }
    auto const pmc_to_unified_pr = minimize_internal_hadamards_n_gadgetize(tableau);
    optimize_phase_polynomial_with_classical(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    reorder_n_degadgetize(tableau);
    spdlog::info("Tableau after optimization: {:g}", tableau);
}

}  // namespace experimental

}  // namespace qsyn
