#include "../tableau_optimization.hpp"

#include <algorithm>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

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
        SubTableau const unified_pr_subtableau = unified_pr;
        SubTableau const reduced_pr_subtableau = pr_pmc_ij;
        spdlog::info(
            "compare_pp PR matrices (history_idx={}):\n  unified_pr(:b):\n{:b}\n  reduced_pr_pmc_ij(:b):\n{:b}",
            history_idx,
            unified_pr_subtableau,
            reduced_pr_subtableau);
        auto const unified_signature = get_signature(unified_pr);

        SignatureComparisonResult result;
        result.history_index = history_idx;
        result.full_signature_equivalent = unified_signature.equivalent(reduced_signature);
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

    auto const find_gadget_index = [&](size_t ancilla) -> std::optional<size_t> {
        for (size_t i = 1; i < pr_idx; ++i) {
            auto const* cct = std::get_if<ClassicalControlTableau>(&working[i]);
            if (cct != nullptr && cct->is_gadget() && cct->ancilla_qubit() == ancilla) {
                return i;
            }
        }
        return std::nullopt;
    };

    while (pr_idx + 1 < working.size()) {
        size_t pmc_idx = pr_idx + 1;
        auto* pmc = std::get_if<ClassicalControlTableau>(&working[pmc_idx]);

        if (pmc == nullptr || !pmc->is_classical_control()) {
            break;
        }
        size_t const ancilla = pmc->ancilla_qubit();

        auto gadget_idx_opt  = find_gadget_index(ancilla);
        if (!gadget_idx_opt.has_value()) {
            spdlog::error(
                "move_pmcs_with_reduced_PR: missing gadget counterpart for ancilla {}",
                ancilla);
            return;
        }
        size_t const gadget_idx = *gadget_idx_opt;

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

        // Build PR_pmc(i,j): terms having Z support on any x_qubit.
        std::vector<PauliRotation> pr_pmc_ij;
        std::vector<PauliRotation> pr_rest;
        pr_pmc_ij.reserve(unified_pr->size());
        pr_rest.reserve(unified_pr->size());
        bool is_blocking = false;
        std::vector<std::string> blocking_terms;
        for (auto& rotation : *unified_pr) {
            bool has_z_on_any_x = false;
            for (size_t const q : x_qubits) {
                if (q < rotation.n_qubits() && rotation.is_z(q)) {
                    has_z_on_any_x = true;
                    if (rotation.is_z(ancilla)) {
                        is_blocking = true;
                        blocking_terms.push_back(rotation.to_bit_string());
                    }
                    break;
                }
            }
            // if (rotation.is_z(ancilla)) {
            //     pr_rest.push_back(std::move(rotation));
            // } else if (has_z_on_any_x) {
            //     pr_pmc_ij.push_back(std::move(rotation));
            // } else {
            //     pr_rest.push_back(std::move(rotation));
            // }
            if (has_z_on_any_x) {
                pr_pmc_ij.push_back(std::move(rotation));
            } else {
                pr_rest.push_back(std::move(rotation));
            }
        }
        std::optional<std::vector<SignatureComparisonResult>> comparisons_opt;
        if (relation_it != pmc_to_unified_pr.end() &&
            !relation_it->second.unified_pr_history.empty()) {
            // Compute comparisons before moving PR columns out of pr_pmc_ij.
            comparisons_opt.emplace(compare_pp(
                relation_it->second.unified_pr_history,
                pr_pmc_ij,
                x_qubits));
        }

        // Split unified PR in working: replace PR at pr_idx with [pr_rest][pr_pmc_ij].
        working.erase(
            working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
            working.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1));
        working.insert(
            working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
            SubTableau{std::move(pr_rest)});
        working.insert(
            working.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1),
            SubTableau{std::move(pr_pmc_ij)});

        auto rest_pr_prime = swap_along_test(working, pr_idx, 1);

        // Remove rest_pr after computing rest_pr'. Then pr_pmc_ij is at pr_idx, PMC at pr_idx+1.
        working.erase(
            working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
            working.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1));

        // Commute+move PMC to right behind its gadget.
        size_t const target_idx = gadget_idx + 1;
        swap_along(working, pr_idx + 1, target_idx);

        // Since PMC moved left of pr_pmc_ij, pr_pmc_ij shifts right by one.
        size_t pmc_ij_idx = pr_idx + 1;
        if (pmc_ij_idx >= working.size()) {
            spdlog::error(
                "move_pmcs_with_reduced_PR: invalid pr_pmc_ij index {} after moving PMC",
                pmc_ij_idx);
            return;
        }

        // Replace pr_pmc_ij with the original unified PR.
        working.erase(
            working.begin() + static_cast<std::ptrdiff_t>(pmc_ij_idx),
            working.begin() + static_cast<std::ptrdiff_t>(pmc_ij_idx + 1));
        working.insert(
            working.begin() + static_cast<std::ptrdiff_t>(pmc_ij_idx),
            SubTableau{unified_pr_original});
        pr_idx = pmc_ij_idx;

        auto* moved_pmc = std::get_if<ClassicalControlTableau>(&working[target_idx]);
        if (moved_pmc == nullptr || !moved_pmc->is_classical_control()) {
            spdlog::error(
                "move_pmcs_with_reduced_PR: expected moved PMC at index {} for ancilla {}",
                target_idx,
                ancilla);
            continue;
        }
        auto const* gadget = std::get_if<ClassicalControlTableau>(&working[gadget_idx]);
        size_t const reference_qubit = gadget->reference_qubit();
        auto const moved_ops = extract_clifford_operators(moved_pmc->operations());
        bool const is_single_x_on_reference =
            moved_ops.size() == 1 &&
            moved_ops.front().first == CliffordOperatorType::x &&
            moved_ops.front().second[0] == reference_qubit;

        if (!is_single_x_on_reference) {
            spdlog::info(
                " move_pmcs_with_reduced_PR: exported PMC final state (ancilla={}, ref_q={}, x_qubits=[{}], is_blocking={}):\n{}",
                ancilla,
                reference_qubit,
                fmt::join(x_qubits, ","),
                is_blocking,
                clifford_ops_to_string(moved_ops));
        } else {
            spdlog::info(
                " move_pmcs_with_reduced_PR: exported PMC final state success (ancilla={}, ref_q={}, x_qubits={}, is_blocking={}):\n",
                ancilla,
                reference_qubit,
                fmt::join(x_qubits, ","),
                is_blocking);
        }
        bool rest_pr_prime_has_phase_on_ancilla = false;
        if (auto const* rest_pr_prime_pr = std::get_if<std::vector<PauliRotation>>(&rest_pr_prime)) {
            rest_pr_prime_has_phase_on_ancilla = std::ranges::any_of(
                *rest_pr_prime_pr,
                [ancilla](PauliRotation const& rotation) {
                    return ancilla < rotation.n_qubits() && rotation.is_z(ancilla);
                });
        }
        if (rest_pr_prime_has_phase_on_ancilla) {
            spdlog::info(
                "move_pmcs_with_reduced_PR: have phase on ancilla_qubit {}",
                ancilla);
        } else {
            spdlog::info(
                "move_pmcs_with_reduced_PR: no phase on ancilla_qubit {}",
                ancilla);
        }

        if (comparisons_opt.has_value()) {
            for (auto const& comparison : *comparisons_opt) {
                for (auto const& per_qubit : comparison.per_qubit_comparisons) {
                    size_t unified_diff_terms = 0;
                    size_t reduced_diff_terms = 0;

                    if (per_qubit.unified_terms.linear_mod8 != per_qubit.reduced_terms.linear_mod8) {
                        if (per_qubit.unified_terms.linear_mod8.has_value()) ++unified_diff_terms;
                        if (per_qubit.reduced_terms.linear_mod8.has_value()) ++reduced_diff_terms;
                    }

                    std::unordered_map<SignatureTensor::PairTerm, uint8_t, SignatureTensor::PairTermHash> unified_quad_map;
                    std::unordered_map<SignatureTensor::PairTerm, uint8_t, SignatureTensor::PairTermHash> reduced_quad_map;
                    for (auto const& [term, coeff] : per_qubit.unified_terms.quadratic_terms) {
                        unified_quad_map[term] = coeff;
                    }
                    for (auto const& [term, coeff] : per_qubit.reduced_terms.quadratic_terms) {
                        reduced_quad_map[term] = coeff;
                    }
                    for (auto const& [term, coeff] : unified_quad_map) {
                        auto const it = reduced_quad_map.find(term);
                        if (it == reduced_quad_map.end() || it->second != coeff) {
                            ++unified_diff_terms;
                        }
                    }
                    for (auto const& [term, coeff] : reduced_quad_map) {
                        auto const it = unified_quad_map.find(term);
                        if (it == unified_quad_map.end() || it->second != coeff) {
                            ++reduced_diff_terms;
                        }
                    }

                    std::unordered_set<SignatureTensor::TripleTerm, SignatureTensor::TripleTermHash> unified_cubic_set(
                        per_qubit.unified_terms.cubic_terms.begin(),
                        per_qubit.unified_terms.cubic_terms.end());
                    std::unordered_set<SignatureTensor::TripleTerm, SignatureTensor::TripleTermHash> reduced_cubic_set(
                        per_qubit.reduced_terms.cubic_terms.begin(),
                        per_qubit.reduced_terms.cubic_terms.end());
                    for (auto const& term : unified_cubic_set) {
                        if (!reduced_cubic_set.contains(term)) {
                            ++unified_diff_terms;
                        }
                    }
                    for (auto const& term : reduced_cubic_set) {
                        if (!unified_cubic_set.contains(term)) {
                            ++reduced_diff_terms;
                        }
                    }

                    spdlog::info(
                        "compare_pp: ancilla={} history_idx={} qubit={} equivalent={} diff_terms(unified={}, reduced={})",
                        ancilla,
                        comparison.history_index,
                        per_qubit.qubit,
                        per_qubit.equivalent,
                        unified_diff_terms,
                        reduced_diff_terms);
                    
                }
            }
        }
        if (is_blocking) {
            spdlog::info(
                " move_pmcs_with_reduced_PR: blocking pr_pmc_ij (ancilla={}):\n{}",
                ancilla,
                fmt::join(blocking_terms, "\n"));
        }
    }
}

void minimize_ancillary_t_opt_with_degadgetization(Tableau& tableau, std::optional<std::string> export_filename) {
    (void)export_filename;
    if (tableau.is_empty()) {
        return;
    }
    auto const pmc_to_unified_pr = minimize_internal_hadamards_n_gadgetize(tableau);
    spdlog::debug("After minimize_internal_hadamards_n_gadgetize: {:g}", tableau);
    optimize_phase_polynomial_with_classical(tableau, FastToddPhasePolynomialOptimizationStrategy{});
    move_pmcs_with_reduced_PR(tableau, pmc_to_unified_pr);
}

}  // namespace experimental

}  // namespace qsyn
