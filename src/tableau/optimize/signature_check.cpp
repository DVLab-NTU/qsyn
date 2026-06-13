#include "../tableau_optimization.hpp"

#include <algorithm>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <numeric>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "convert/qcir_to_tableau.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_io.hpp"
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

bool column_has_z_on_ancilla(PauliRotation const& rotation, size_t ancilla_qubit) {
    return ancilla_qubit < rotation.n_qubits() && rotation.is_z(ancilla_qubit);
}

/** Same layout as run_commute: pr_rest @ pr_idx, pr_split @ pr_idx+1. */
void insert_pr_split_rest_at_idx(
    Tableau& working,
    size_t pr_idx,
    std::vector<PauliRotation> pr_rest,
    std::vector<PauliRotation> pr_split) {
    working.erase(
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1));
    working.insert(
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
        SubTableau{std::move(pr_rest)});
    working.insert(
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1),
        SubTableau{std::move(pr_split)});
}

/** Column group at pr_idx (circuit-front slot), remainder @ pr_idx+1. */
void insert_pr_group_front_at_idx(
    Tableau& working,
    size_t pr_idx,
    std::vector<PauliRotation> pr_group,
    std::vector<PauliRotation> pr_rest) {
    working.erase(
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx + 1));
    working.insert(
        working.begin() + static_cast<std::ptrdiff_t>(pr_idx),
        SubTableau{std::move(pr_group)});
}

std::vector<PauliRotation> swap_along_test_to_circuit_front(
    Tableau const& working,
    size_t from_idx) {
    if (from_idx >= working.size() || working.size() < 2) {
        throw std::logic_error("swap_along_test_to_circuit_front: invalid tableau layout");
    }
    auto moved = swap_along_test(working, from_idx, 1);
    if (auto* pr = std::get_if<std::vector<PauliRotation>>(&moved)) {
        return *pr;
    }
    throw std::logic_error("swap_along_test_to_circuit_front: expected PR block");
}

}  // namespace

char const* pr_column_block_kind_str(PrColumnBlockKind kind) {
    switch (kind) {
        case PrColumnBlockKind::x_only:
            return "x_only";
        case PrColumnBlockKind::ancilla_only:
            return "ancilla_only";
        case PrColumnBlockKind::x_and_ancilla:
            return "x_and_ancilla";
        case PrColumnBlockKind::other:
        default:
            return "other";
    }
}

PrColumnBlockKind pr_column_block_kind_for_pmc(
    PauliRotation const& rotation,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits) {
    bool has_z_on_any_x = false;
    for (size_t const q : x_qubits) {
        if (q < rotation.n_qubits() && rotation.is_z(q)) {
            has_z_on_any_x = true;
            break;
        }
    }
    bool const z_on_ancilla =
        ancilla_qubit < rotation.n_qubits() && rotation.is_z(ancilla_qubit);
    if (has_z_on_any_x && z_on_ancilla) {
        return PrColumnBlockKind::x_and_ancilla;
    }
    if (has_z_on_any_x) {
        return PrColumnBlockKind::x_only;
    }
    if (z_on_ancilla) {
        return PrColumnBlockKind::ancilla_only;
    }
    return PrColumnBlockKind::other;
}

std::optional<size_t> find_unified_pr_block_index(Tableau const& tableau) {
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

PrWholeBlockPhaseExport export_whole_pr_phases_after_swap_to_front(Tableau const& tableau) {
    auto const pr_idx_opt = find_unified_pr_block_index(tableau);
    if (!pr_idx_opt.has_value()) {
        throw std::logic_error("export_whole_pr_phases_after_swap_to_front: no unified PR block");
    }
    size_t const pr_idx = *pr_idx_opt;
    auto const* pr_before =
        std::get_if<std::vector<PauliRotation>>(&tableau[pr_idx]);
    if (pr_before == nullptr) {
        throw std::logic_error("export_whole_pr_phases_after_swap_to_front: expected PR block");
    }
    PrWholeBlockPhaseExport out;
    out.pr_idx = pr_idx;
    out.before = *pr_before;
    out.after  = swap_along_test_to_circuit_front(tableau, pr_idx);
    if (out.after.size() != out.before.size()) {
        spdlog::warn(
            "export_whole_pr_phases_after_swap_to_front: column count changed {} -> {}",
            out.before.size(),
            out.after.size());
    }
    return out;
}

namespace {

std::vector<PrAncillaColumnBlockRow> build_ancilla_column_block_rows(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    PrWholeBlockPhaseExport const& pr_export) {
    std::vector<PrAncillaColumnBlockRow> rows;
    auto const pr_idx_opt = find_unified_pr_block_index(tableau);
    if (!pr_idx_opt.has_value()) {
        return rows;
    }
    size_t const pr_idx = *pr_idx_opt;

    for (size_t pmc_idx = pr_idx + 1; pmc_idx < tableau.size(); ++pmc_idx) {
        auto const* pmc = std::get_if<ClassicalControlTableau>(&tableau[pmc_idx]);
        if (pmc == nullptr || !pmc->is_classical_control()) {
            break;
        }
        size_t const ancilla = pmc->ancilla_qubit();

        std::vector<size_t> const empty_x_qubits;
        auto const relation_it = pmc_to_unified_pr.find(ancilla);
        auto const& x_qubits =
            (relation_it != pmc_to_unified_pr.end()) ? relation_it->second.x_qubits : empty_x_qubits;

        size_t const n_cols = std::min(pr_export.before.size(), pr_export.after.size());
        rows.reserve(rows.size() + n_cols);
        for (size_t col = 0; col < n_cols; ++col) {
            PrAncillaColumnBlockRow row;
            row.ancilla_qubit = ancilla;
            row.column_index  = col;
            row.phase_before  = fmt::format("{}", pr_export.before[col].phase());
            row.phase_after   = fmt::format("{}", pr_export.after[col].phase());
            row.pauli_before  = pr_export.before[col].to_bit_string();
            row.pauli_after   = pr_export.after[col].to_bit_string();
            row.block_before =
                pr_column_block_kind_for_pmc(pr_export.before[col], ancilla, x_qubits);
            row.block_after =
                pr_column_block_kind_for_pmc(pr_export.after[col], ancilla, x_qubits);
            row.z_on_ancilla_before = column_has_z_on_ancilla(pr_export.before[col], ancilla);
            row.z_on_ancilla_after  = column_has_z_on_ancilla(pr_export.after[col], ancilla);
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

struct BlockKindCounts {
    size_t x_only        = 0;
    size_t ancilla_only  = 0;
    size_t x_and_ancilla = 0;
    size_t other         = 0;
};

BlockKindCounts count_blocks(std::vector<PrAncillaColumnBlockRow> const& rows, bool after) {
    BlockKindCounts c;
    for (auto const& row : rows) {
        auto const kind = after ? row.block_after : row.block_before;
        switch (kind) {
            case PrColumnBlockKind::x_only:
                ++c.x_only;
                break;
            case PrColumnBlockKind::ancilla_only:
                ++c.ancilla_only;
                break;
            case PrColumnBlockKind::x_and_ancilla:
                ++c.x_and_ancilla;
                break;
            case PrColumnBlockKind::other:
            default:
                ++c.other;
                break;
        }
    }
    return c;
}

}  // namespace

bool write_pr_whole_swap_phase_export_csv(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    PrWholeBlockPhaseExport const& pr_export,
    std::filesystem::path const& out_path) {
    auto const rows = build_ancilla_column_block_rows(tableau, pmc_to_unified_pr, pr_export);
    std::ofstream out{out_path};
    if (!out) {
        spdlog::error("write_pr_whole_swap_phase_export_csv: cannot open {}", out_path.string());
        return false;
    }
    out << "ancilla_qubit,column_index,phase_before,phase_after,pauli_before,pauli_after,"
           "block_before,block_after,z_ancilla_before,z_ancilla_after\n";
    for (auto const& row : rows) {
        out << row.ancilla_qubit << ',' << row.column_index << ','
            << '"' << row.phase_before << '"' << ','
            << '"' << row.phase_after << '"' << ','
            << row.pauli_before << ',' << row.pauli_after << ','
            << pr_column_block_kind_str(row.block_before) << ','
            << pr_column_block_kind_str(row.block_after) << ','
            << (row.z_on_ancilla_before ? 1 : 0) << ','
            << (row.z_on_ancilla_after ? 1 : 0) << '\n';
    }
    spdlog::info(
        "Wrote {} PR column rows (whole-block swap to idx 1) -> {}",
        rows.size(),
        out_path.string());
    return true;
}

void log_pr_block_summary_per_ancilla(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    PrWholeBlockPhaseExport const& pr_export) {
    auto const all_rows = build_ancilla_column_block_rows(tableau, pmc_to_unified_pr, pr_export);
    auto const pr_idx_opt = find_unified_pr_block_index(tableau);
    if (!pr_idx_opt.has_value()) {
        return;
    }
    size_t const pr_idx = *pr_idx_opt;

    for (size_t pmc_idx = pr_idx + 1; pmc_idx < tableau.size(); ++pmc_idx) {
        auto const* pmc = std::get_if<ClassicalControlTableau>(&tableau[pmc_idx]);
        if (pmc == nullptr || !pmc->is_classical_control()) {
            break;
        }
        size_t const ancilla = pmc->ancilla_qubit();

        std::vector<PrAncillaColumnBlockRow> ancilla_rows;
        for (auto const& row : all_rows) {
            if (row.ancilla_qubit == ancilla) {
                ancilla_rows.push_back(row);
            }
        }
        auto const before = count_blocks(ancilla_rows, false);
        auto const after  = count_blocks(ancilla_rows, true);
        spdlog::info(
            "whole-PR swap ancilla={} blocks before: x_only={} ancilla_only={} x_and_ancilla={} other={} | "
            "after: x_only={} ancilla_only={} x_and_ancilla={} other={}",
            ancilla,
            before.x_only,
            before.ancilla_only,
            before.x_and_ancilla,
            before.other,
            after.x_only,
            after.ancilla_only,
            after.x_and_ancilla,
            after.other);
    }
}

UnifiedPrPmcSplit split_unified_pr_for_pmc(
    std::vector<PauliRotation> const& unified_pr,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits) {
    UnifiedPrPmcSplit split;
    split.commuting.reserve(unified_pr.size());
    split.commuting_rest.reserve(unified_pr.size());
    split.ancilla_group.reserve(unified_pr.size());
    split.ancilla_rest.reserve(unified_pr.size());

    for (auto const& rotation : unified_pr) {
        bool has_z_on_any_x = false;
        for (size_t const q : x_qubits) {
            if (q < rotation.n_qubits() && rotation.is_z(q)) {
                has_z_on_any_x = true;
                break;
            }
        }
        bool const z_on_ancilla = rotation.is_z(ancilla_qubit);
        if (z_on_ancilla) {
            split.commuting_rest.push_back(rotation);
        } else if (has_z_on_any_x) {
            split.commuting.push_back(rotation);
        } else {
            split.commuting_rest.push_back(rotation);
        }
        if (has_z_on_any_x) {
            split.ancilla_group.push_back(rotation);
        } else {
            split.ancilla_rest.push_back(rotation);
        }
    }
    split.blocking = find_pr_blocking_rotations(unified_pr, ancilla_qubit, x_qubits);
    return split;
}

PrGroupPushFrontZCheck check_pr_group_z_on_ancilla_after_push_front(
    std::vector<PauliRotation> const& group,
    std::vector<PauliRotation> const& rest,
    Tableau const& tableau,
    size_t pr_idx,
    size_t ancilla_qubit,
    std::string_view group_name) {
    PrGroupPushFrontZCheck result;
    result.group_name = std::string(group_name);
    result.group_size = group.size();
    if (group.empty()) {
        result.all_z_on_ancilla = true;
        return result;
    }

    if (pr_idx >= tableau.size() || tableau.size() < 2) {
        throw std::logic_error("check_pr_group_z_on_ancilla_after_push_front: invalid tableau layout");
    }
    Tableau working = tableau;
    insert_pr_group_front_at_idx(working, pr_idx, group, rest);
    auto const group_after = swap_along_test_to_circuit_front(working, pr_idx);
    for (size_t i = 0; i < group.size() && i < group_after.size(); ++i) {
        if (column_has_z_on_ancilla(group_after[i], ancilla_qubit)) {
            ++result.z_on_ancilla_count;
        }
    }
    result.all_z_on_ancilla = (result.z_on_ancilla_count == result.group_size);
    return result;
}

PrColumnThreeGroupSplit classify_pr_three_column_groups(
    std::vector<PauliRotation> const& unified_pr,
    size_t ancilla_qubit,
    std::vector<size_t> const& x_qubits) {
    PrColumnThreeGroupSplit groups;
    groups.x_only.reserve(unified_pr.size());
    groups.ancilla_only.reserve(unified_pr.size());
    groups.x_and_ancilla.reserve(unified_pr.size());
    groups.other.reserve(unified_pr.size());

    for (auto const& rotation : unified_pr) {
        bool has_z_on_any_x = false;
        for (size_t const q : x_qubits) {
            if (q < rotation.n_qubits() && rotation.is_z(q)) {
                has_z_on_any_x = true;
                break;
            }
        }
        bool const z_on_ancilla = rotation.is_z(ancilla_qubit);
        if (has_z_on_any_x && z_on_ancilla) {
            groups.x_and_ancilla.push_back(rotation);
        } else if (has_z_on_any_x) {
            groups.x_only.push_back(rotation);
        } else if (z_on_ancilla) {
            groups.ancilla_only.push_back(rotation);
        } else {
            groups.other.push_back(rotation);
        }
    }
    return groups;
}

namespace {

std::vector<PauliRotation> concat_pr_groups(
    std::vector<PauliRotation> const& a,
    std::vector<PauliRotation> const& b,
    std::vector<PauliRotation> const& c) {
    std::vector<PauliRotation> out;
    out.reserve(a.size() + b.size() + c.size());
    out.insert(out.end(), a.begin(), a.end());
    out.insert(out.end(), b.begin(), b.end());
    out.insert(out.end(), c.begin(), c.end());
    return out;
}

}  // namespace

void test_pr_column_groups_push_front_z_on_ancilla(
    Tableau const& tableau,
    std::unordered_map<size_t, PmcUnifiedPrRelation> const& pmc_to_unified_pr,
    std::string_view circuit_label) {
    Tableau working = tableau;
    size_t idx      = 1;
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
        spdlog::warn(
            "test_pr_column_groups_push_front: [{}] no unified PR block",
            circuit_label);
        return;
    }
    size_t const pr_idx = idx;

    for (size_t pmc_idx = pr_idx + 1; pmc_idx < working.size(); ++pmc_idx) {
        auto* pmc = std::get_if<ClassicalControlTableau>(&working[pmc_idx]);
        if (pmc == nullptr || !pmc->is_classical_control()) {
            break;
        }
        size_t const ancilla = pmc->ancilla_qubit();

        auto const relation_it = pmc_to_unified_pr.find(ancilla);
        std::vector<size_t> const empty_x_qubits;
        auto const& x_qubits =
            (relation_it != pmc_to_unified_pr.end()) ? relation_it->second.x_qubits : empty_x_qubits;

        auto* unified_pr = std::get_if<std::vector<PauliRotation>>(&working[pr_idx]);
        if (unified_pr == nullptr) {
            break;
        }

        auto const groups = classify_pr_three_column_groups(*unified_pr, ancilla, x_qubits);

        auto const rest_for_x_only = concat_pr_groups(
            groups.ancilla_only, groups.x_and_ancilla, groups.other);
        auto const rest_for_ancilla_only = concat_pr_groups(
            groups.x_only, groups.x_and_ancilla, groups.other);
        auto const rest_for_x_and_ancilla = concat_pr_groups(
            groups.x_only, groups.ancilla_only, groups.other);

        PrSplitPushFrontTestReport report;
        report.ancilla_qubit = ancilla;
        report.x_qubits      = x_qubits;
        report.x_only_check = check_pr_group_z_on_ancilla_after_push_front(
            groups.x_only, rest_for_x_only, working, pr_idx, ancilla, "x_only");
        report.ancilla_only_check = check_pr_group_z_on_ancilla_after_push_front(
            groups.ancilla_only, rest_for_ancilla_only, working, pr_idx, ancilla, "ancilla_only");
        report.x_and_ancilla_check = check_pr_group_z_on_ancilla_after_push_front(
            groups.x_and_ancilla, rest_for_x_and_ancilla, working, pr_idx, ancilla, "x_and_ancilla");

        spdlog::info(
            "push-front Z@ancilla [{}] PMC ancilla={} x_qubits=[{}]: "
            "x_only {}/{} (all={}) | ancilla_only {}/{} (all={}) | x_and_ancilla {}/{} (all={})",
            circuit_label,
            ancilla,
            fmt::join(x_qubits, ","),
            report.x_only_check.z_on_ancilla_count,
            report.x_only_check.group_size,
            report.x_only_check.all_z_on_ancilla,
            report.ancilla_only_check.z_on_ancilla_count,
            report.ancilla_only_check.group_size,
            report.ancilla_only_check.all_z_on_ancilla,
            report.x_and_ancilla_check.z_on_ancilla_count,
            report.x_and_ancilla_check.group_size,
            report.x_and_ancilla_check.all_z_on_ancilla);
    }
}



SatSignatureExport compute_sat_signature_blocks(Tableau const& tableau) {
    SatSignatureExport out;
    out.qubit_count   = tableau.n_qubits();
    out.ancilla_count = tableau.n_ancilla();

    Tableau tableau_copy = tableau;
    auto gadgets         = export_hadamard_gadget_pairs(tableau_copy);
    size_t const num_gadgets = gadgets.size();

    std::vector<PauliRotation> unified_pr;
    std::vector<PauliRotation> unified_pr_commuted_front;
    auto const pr_idx_opt = find_unified_pr_block_index(tableau);
    if (pr_idx_opt.has_value()) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[*pr_idx_opt]);
        if (pr_vec != nullptr) {
            unified_pr = *pr_vec;
            unified_pr_commuted_front = swap_along_test_to_circuit_front(tableau, *pr_idx_opt);
            if (unified_pr_commuted_front.size() != unified_pr.size()) {
                spdlog::warn(
                    "compute_sat_signature_blocks: PR column count changed {} -> {} after front commute",
                    unified_pr.size(),
                    unified_pr_commuted_front.size());
            }
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
        size_t const ancilla = gadgets[g_idx].ancilla_qubit;
        auto&        lists   = out.blocks_by_gid[g_idx];
        bool         degadgetizable = true;

        for (size_t col = 0; col < unified_pr.size(); ++col) {
            bool const in_right =
                ancilla < unified_pr[col].n_qubits() && unified_pr[col].is_z(ancilla);
            bool const in_left = col < unified_pr_commuted_front.size() &&
                                 ancilla < unified_pr_commuted_front[col].n_qubits() &&
                                 unified_pr_commuted_front[col].is_z(ancilla);
            if (in_left && in_right) {
                degadgetizable = false;
            }
            size_t const pid = num_gadgets + col;
            if (in_left) {
                lists.block_left.push_back(pid);
            }
            if (in_right) {
                lists.block_right.push_back(pid);
            }
        }
        lists.degadgetizable = degadgetizable;
        std::ranges::sort(lists.block_left);
        std::ranges::sort(lists.block_right);
    }

    auto const overlap = compute_gadget_overlap_constraints(out);
    std::vector<size_t> gadget_ancilla_qubit(num_gadgets);
    for (size_t gid = 0; gid < num_gadgets; ++gid) {
        gadget_ancilla_qubit[gid] = out.blocks_by_gid[gid].ancilla_qubit;
    }
    for (size_t const gid :
         collect_lower_ancilla_overlap_excluded_gids(gadget_ancilla_qubit, overlap)) {
        out.blocks_by_gid[gid].degadgetizable = false;
    }

    return out;
}

std::unordered_set<size_t> collect_lower_ancilla_overlap_excluded_gids(
    std::vector<size_t> const&      gadget_ancilla_qubit,
    GadgetOverlapConstraints const& overlap) {
    std::unordered_set<size_t> excluded;
    for (size_t gid = 0; gid < overlap.gadget_count; ++gid) {
        if (gid >= gadget_ancilla_qubit.size()) {
            continue;
        }
        size_t const ancilla = gadget_ancilla_qubit[gid];
        for (size_t const other : overlap.overlap_neighbors[gid]) {
            if (other < gadget_ancilla_qubit.size() && gadget_ancilla_qubit[other] < ancilla) {
                excluded.insert(gid);
                break;
            }
        }
    }
    return excluded;
}

size_t GadgetOverlapConstraints::overlapping_pair_count() const {
    size_t count = 0;
    for (size_t gid = 0; gid < gadget_count; ++gid) {
        for (size_t const other : overlap_neighbors[gid]) {
            if (gid < other) {
                ++count;
            }
        }
    }
    return count;
}

size_t GadgetOverlapConstraints::max_overlap_among_columns() const {
    size_t max_n = 0;
    for (size_t const n : overlap_gadget_count_by_column) {
        max_n = std::max(max_n, n);
    }
    return max_n;
}

size_t GadgetOverlapConstraints::max_overlap_among_gadgets() const {
    size_t max_n = 0;
    for (auto const& neighbors : overlap_neighbors) {
        max_n = std::max(max_n, neighbors.size());
    }
    return max_n;
}

GadgetOverlapConstraints compute_gadget_overlap_constraints(SatSignatureExport const& sig) {
    size_t const G = sig.blocks_by_gid.size();
    GadgetOverlapConstraints out;
    out.gadget_count = G;
    out.overlap_neighbors.assign(G, {});
    out.bridge_columns_by_neighbor.assign(G, {});
    out.overlap_gadget_count_by_column.assign(sig.pauli_count, 0);

    std::vector<std::set<size_t>> neighbor_sets(G);
    size_t const pid_lo = G;
    for (size_t col = 0; col < sig.pauli_count; ++col) {
        size_t const pid = pid_lo + col;
        std::vector<size_t> right_gids;
        std::vector<size_t> left_gids;
        right_gids.reserve(G);
        left_gids.reserve(G);
        for (size_t gid = 0; gid < G; ++gid) {
            auto const& lists = sig.blocks_by_gid[gid];
            if (std::binary_search(lists.block_right.begin(), lists.block_right.end(), pid)) {
                right_gids.push_back(gid);
            }
            if (std::binary_search(lists.block_left.begin(), lists.block_left.end(), pid)) {
                left_gids.push_back(gid);
            }
        }
        std::set<size_t> gadgets_in_column_overlap;
        for (size_t const a : right_gids) {
            for (size_t const b : left_gids) {
                if (a >= b) {
                    continue;
                }
                gadgets_in_column_overlap.insert(a);
                gadgets_in_column_overlap.insert(b);
                neighbor_sets[a].insert(b);
                neighbor_sets[b].insert(a);
                out.bridge_columns_by_neighbor[a][b].push_back(col);
                out.bridge_columns_by_neighbor[b][a].push_back(col);
            }
        }
        out.overlap_gadget_count_by_column[col] = gadgets_in_column_overlap.size();
    }

    for (size_t gid = 0; gid < G; ++gid) {
        out.overlap_neighbors[gid].assign(neighbor_sets[gid].begin(), neighbor_sets[gid].end());
        for (auto& [other, cols] : out.bridge_columns_by_neighbor[gid]) {
            std::sort(cols.begin(), cols.end());
            cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
        }
    }
    return out;
}

void log_sat_reorder_preprocess(SatSignatureExport const& sig) {
    size_t const G = sig.blocks_by_gid.size();
    std::vector<std::string> degadgetizable_entries;
    degadgetizable_entries.reserve(G);
    for (auto const& lists : sig.blocks_by_gid) {
        if (!lists.degadgetizable) {
            continue;
        }
        degadgetizable_entries.push_back(fmt::format("g{}", lists.gid));
    }
    spdlog::info(
        "sat_reorder_preprocess: {}/{} gadgets degadgetizable",
        degadgetizable_entries.size(),
        G);
    if (!degadgetizable_entries.empty()) {
        spdlog::info(
            "sat_reorder_preprocess:   {}",
            fmt::join(degadgetizable_entries, ", "));
    }

    auto const overlap = compute_gadget_overlap_constraints(sig);
    spdlog::info(
        "sat_reorder_preprocess: {} overlapping gadget pairs from {} PR columns",
        overlap.overlapping_pair_count(),
        sig.pauli_count);

    size_t const max_col_overlap = overlap.max_overlap_among_columns();
    size_t       max_col         = 0;
    for (size_t col = 0; col < overlap.overlap_gadget_count_by_column.size(); ++col) {
        if (overlap.overlap_gadget_count_by_column[col] == max_col_overlap) {
            max_col = col;
            break;
        }
    }
    size_t const max_gadget_overlap = overlap.max_overlap_among_gadgets();
    size_t       max_gid            = 0;
    for (size_t gid = 0; gid < G; ++gid) {
        if (overlap.overlap_neighbors[gid].size() == max_gadget_overlap) {
            max_gid = gid;
            break;
        }
    }
    spdlog::info(
        "sat_reorder_preprocess: max overlap among columns={} (col {})",
        max_col_overlap,
        max_col);
    spdlog::info(
        "sat_reorder_preprocess: max overlap among gadgets={} (g{})",
        max_gadget_overlap,
        max_gid);

    for (size_t gid = 0; gid < G; ++gid) {
        auto const& neighbors = overlap.overlap_neighbors[gid];
        if (neighbors.empty()) {
            continue;
        }
        spdlog::info(
            "sat_reorder_preprocess:   g{}: {} overlaps",
            gid,
            neighbors.size());
        std::vector<std::string> entries;
        entries.reserve(neighbors.size());
        for (size_t const other : neighbors) {
            entries.push_back(fmt::format("g{}", other));
        }
        spdlog::info(
            "sat_reorder_preprocess:     {}",
            fmt::join(entries, ", "));
    }
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

        auto const split = split_unified_pr_for_pmc(unified_pr_original, ancilla, x_qubits);
        auto& pr_commuting      = split.commuting;
        auto& pr_commuting_rest = split.commuting_rest;
        auto& pr_ancilla        = split.ancilla_group;
        auto& pr_ancilla_rest    = split.ancilla_rest;
        auto const& pr_blocking  = split.blocking;

        auto const zchk_commuting = check_pr_group_z_on_ancilla_after_push_front(
            pr_commuting, pr_commuting_rest, working, pr_idx, ancilla, "commuting");
        auto const zchk_ancilla = check_pr_group_z_on_ancilla_after_push_front(
            pr_ancilla, pr_ancilla_rest, working, pr_idx, ancilla, "ancilla_group");
        spdlog::debug(
            "move_pmcs push-front Z@ancilla ancilla={}: commuting {}/{} ancilla_group {}/{}",
            ancilla,
            zchk_commuting.z_on_ancilla_count,
            zchk_commuting.group_size,
            zchk_ancilla.z_on_ancilla_count,
            zchk_ancilla.group_size);

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

            insert_pr_split_rest_at_idx(w, pr_idx, std::move(pr_rest), std::move(pr_split));

            [[maybe_unused]] auto rest_pr_prime = swap_along_test_to_circuit_front(w, pr_idx);

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

    std::string const label =
        tableau.get_filename().empty() ? "tableau" : tableau.get_filename();
    auto const pr_export = export_whole_pr_phases_after_swap_to_front(tableau);
    std::filesystem::path const phase_csv =
        fmt::format("/home/ferayer/minimize_ancilla/results/pr_whole_swap_{}.csv", label);
    write_pr_whole_swap_phase_export_csv(tableau, pmc_to_unified_pr, pr_export, phase_csv);
    log_pr_block_summary_per_ancilla(tableau, pmc_to_unified_pr, pr_export);

    spdlog::info("Tableau after optimization: {:g}", tableau);
}

}  // namespace experimental

}  // namespace qsyn
