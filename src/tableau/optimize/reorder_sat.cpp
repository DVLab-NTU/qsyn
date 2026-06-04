/**
 * @file reorder_sat.cpp
 * @brief SAT-driven gadget / Pauli column reorder (export → Z3 → apply); may degadgetize when schedule + blocking allow.
 */

#include "../tableau_optimization.hpp"

#include "tableau/classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef __unix__
#include <unistd.h>
#endif

namespace qsyn::experimental {

namespace {

enum class ScheduleParseSection { None, GadgetOrder, ColumnSlot, Degadgetizable, Span };

struct ParsedGadgetOrdering {
    size_t qubit_count  = 0;
    size_t ancilla_count = 0;
    size_t width_w      = 0;
    std::vector<size_t> gadget_order_gids;
    /// From ``Span :`` section: gadget id -> (min_i, max_i) gap indices (non-degadgetizable only).
    std::unordered_map<size_t, size_t> column_slot;
    std::unordered_set<size_t> degadgetizable_gids;
    std::unordered_map<size_t, std::pair<size_t, size_t>> span_by_gid;
    /// True when the ordering file contained an explicit ``degadgetizable`` section (even if empty).
    bool has_degadgetizable_section = false;

    /// `occupied_gadget_gap_time[gid][t] == 1` iff gadget `gid` is **busy** at gap time `t` per Span `[min_i,max_i]`.
    std::vector<std::vector<std::uint8_t>> occupied_gadget_gap_time;
};

struct AncillaInterval {
    size_t gid                   = 0;
    size_t logical_ancilla_qubit = 0;
    size_t min_gap_i             = 0;
    size_t max_gap_i             = 0;
};

struct AncillaOccupancyTableau {
    size_t width_w            = 0;
    size_t ancilla_base_qubit = 0;
    /// occupied_gid_by_time_lane[t][lane] = gid, or -1 if lane is free at time t.
    std::vector<std::vector<int64_t>> occupied_gid_by_time_lane;
    std::unordered_map<size_t, size_t> gid_to_lane;
    std::unordered_map<size_t, size_t> gid_to_physical_ancilla;
};

bool find_pmc_in_range(
    Tableau const& tableau,
    size_t         ancilla_qubit,
    size_t         search_from,
    size_t         search_to,
    size_t&        out_index,
    std::string&   err);

std::optional<size_t> gid_rank_in_gadget_order(ParsedGadgetOrdering const& ord, size_t const gid) {
    for (size_t rank = 0; rank < ord.gadget_order_gids.size(); ++rank) {
        if (ord.gadget_order_gids[rank] == gid) {
            return rank;
        }
    }
    return std::nullopt;
}

std::unordered_set<size_t> ancillae_touched_by_cct_ops(
    ClassicalControlTableau const& cct,
    size_t const                   data_qubit_end,
    size_t const                   ancilla_qubit_hi) {
    std::unordered_set<size_t> touched;
    for (auto const& op : extract_clifford_operators(cct.operations())) {
        auto const& [op_type, qubits] = op;
        if (op_type == CliffordOperatorType::cx) {
            for (size_t const q : {qubits[0], qubits[1]}) {
                if (q >= data_qubit_end && q < ancilla_qubit_hi) {
                    touched.insert(q);
                }
            }
        } else if (qubits[0] >= data_qubit_end && qubits[0] < ancilla_qubit_hi) {
            touched.insert(qubits[0]);
        }
    }
    return touched;
}

std::optional<size_t> pmc_target_index_for_span(
    ParsedGadgetOrdering const&           ord,
    size_t const                          gid,
    size_t const                          rank,
    std::vector<size_t> const&            ccc_pos_by_rank,
    std::vector<size_t> const&            emit_start_by_rank) {
    if (ord.degadgetizable_gids.count(gid) != 0) {
        return ccc_pos_by_rank[rank] + 1;
    }
    auto const it = ord.span_by_gid.find(gid);
    if (it == ord.span_by_gid.end()) {
        return std::nullopt;
    }
    size_t const span_min = it->second.first;
    size_t const span_max = it->second.second;
    size_t const G          = ord.gadget_order_gids.size();

    if (span_min == rank) {
        return ccc_pos_by_rank[rank] + 1;
    }
    if (rank > 0 && span_max <= rank - 1) {
        return ccc_pos_by_rank[rank] + 1;
    }
    if (span_max >= G) {
        return std::nullopt;
    }
    return emit_start_by_rank[span_max];
}

void finalize_parsed_gadget_ordering(ParsedGadgetOrdering& ord, std::string& err) {
    if (!ord.has_degadgetizable_section) {
        err = "missing degadgetizable section (expected from SMT export)";
        return;
    }
    size_t const G = ord.gadget_order_gids.size();
    for (size_t gid = 0; gid < G; ++gid) {
        if (ord.degadgetizable_gids.count(gid) != 0) {
            continue;
        }
        if (ord.span_by_gid.count(gid) == 0) {
            err = fmt::format("missing Span for non-degadgetizable gid {}", gid);
            return;
        }
    }
    if (ord.span_by_gid.empty() && ord.degadgetizable_gids.size() != G) {
        err = "empty Span section but not all gadgets are degadgetizable";
        return;
    }
    if (ord.occupied_gadget_gap_time.empty() && G > 0) {
        size_t const num_gap_times = G + 1;
        ord.occupied_gadget_gap_time.assign(G, std::vector<std::uint8_t>(num_gap_times, 0));
        for (auto const& [gid, span] : ord.span_by_gid) {
            if (gid >= G || span.first > span.second || span.second >= num_gap_times) {
                continue;
            }
            for (size_t t = span.first; t <= span.second; ++t) {
                ord.occupied_gadget_gap_time[gid][t] = 1;
            }
        }
    }
}

void update_pmc_emit_positions_after_move(
    std::vector<size_t>& emit_start_by_rank,
    std::vector<size_t>& ccc_pos_by_rank,
    std::vector<size_t>& pmc_pos,
    size_t               G,
    size_t               i,
    size_t               gid,
    size_t               t,
    size_t               p);

bool validate_middle_against_span(
    std::vector<SubTableau> const& middle,
    size_t const G,
    ParsedGadgetOrdering const& ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    size_t data_qubit_end,
    size_t ancilla_qubit_hi,
    std::unordered_set<size_t> const& degadgetizable_gids);

bool remap_middle_to_physical_ancillae(
    std::vector<SubTableau>& middle,
    std::unordered_map<size_t, size_t> const& logical_to_physical,
    size_t target_n_qubits,
    std::string& err);

bool parse_gadget_ordering_file(std::filesystem::path const& path, ParsedGadgetOrdering& out, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = fmt::format("cannot open {}", path.string());
        return false;
    }

    ScheduleParseSection section = ScheduleParseSection::None;
    std::string line;
    while (std::getline(in, line)) {
        if (auto const hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        // trim
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.erase(line.begin());
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        if (line.starts_with("qubit_count:")) {
            out.qubit_count = static_cast<size_t>(std::stoull(line.substr(std::string_view("qubit_count:").size())));
            continue;
        }
        if (line.starts_with("ancilla_count:")) {
            out.ancilla_count = static_cast<size_t>(std::stoull(line.substr(std::string_view("ancilla_count:").size())));
            continue;
        }
        if (line.starts_with("width:")) {
            out.width_w = static_cast<size_t>(std::stoull(line.substr(std::string_view("width:").size())));
            continue;
        }
        if (line == "gadget_order") {
            section = ScheduleParseSection::GadgetOrder;
            continue;
        }
        if (line == "column_slot") {
            section = ScheduleParseSection::ColumnSlot;
            continue;
        }
        if (line == "degadgetizable" || line.starts_with("degadgetizable")) {
            section               = ScheduleParseSection::Degadgetizable;
            out.has_degadgetizable_section = true;
            continue;
        }
        if (line == "Span :" || line.starts_with("Span")) {
            section = ScheduleParseSection::Span;
            continue;
        }

        if (section == ScheduleParseSection::GadgetOrder) {
            std::istringstream iss(line);
            size_t gid = 0;
            size_t rank = 0;
            if (!(iss >> gid >> rank)) {
                err = fmt::format("bad gadget_order line: {}", line);
                return false;
            }
            if (rank != out.gadget_order_gids.size()) {
                err = fmt::format("gadget_order rank mismatch: expected {}, got {}", out.gadget_order_gids.size(), rank);
                return false;
            }
            out.gadget_order_gids.push_back(gid);
        } else if (section == ScheduleParseSection::ColumnSlot) {
            std::istringstream iss(line);
            size_t pid = 0;
            size_t gap = 0;
            if (!(iss >> pid >> gap)) {
                err = fmt::format("bad column_slot line: {}", line);
                return false;
            }
            out.column_slot[pid] = gap;
        } else if (section == ScheduleParseSection::Degadgetizable) {
            std::istringstream iss(line);
            size_t gid = 0;
            if (!(iss >> gid)) {
                err = fmt::format("bad degadgetizable line: {}", line);
                return false;
            }
            if (out.gadget_order_gids.empty()) {
                err = "degadgetizable requires gadget_order first";
                return false;
            }
            if (gid >= out.gadget_order_gids.size()) {
                err = fmt::format("degadgetizable gid {} >= gadget count {}", gid, out.gadget_order_gids.size());
                return false;
            }
            out.degadgetizable_gids.insert(gid);
        } else if (section == ScheduleParseSection::Span) {
            if (out.gadget_order_gids.empty()) {
                err = "Span data requires gadget_order before Span (need G to size occupied_gadget_gap_time)";
                return false;
            }
            size_t const G = out.gadget_order_gids.size();
            size_t const num_gap_times = G + 1;
            if (out.occupied_gadget_gap_time.empty()) {
                out.occupied_gadget_gap_time.assign(G, std::vector<std::uint8_t>(num_gap_times, 0));
            }

            std::istringstream iss(line);
            size_t gid = 0;
            size_t min_i = 0;
            size_t max_i = 0;
            if (!(iss >> gid >> min_i >> max_i)) {
                err = fmt::format("bad Span line: {}", line);
                return false;
            }
            if (gid >= G) {
                err = fmt::format("Span gid {} >= gadget count {}", gid, G);
                return false;
            }
            if (min_i > max_i) {
                err = fmt::format("Span gid {} has min_i {} > max_i {}", gid, min_i, max_i);
                return false;
            }
            if (max_i >= num_gap_times) {
                err = fmt::format(
                    "Span gid {} max_i {} out of range for gap times [0,{}]", gid, max_i, num_gap_times - 1);
                return false;
            }
            if (out.span_by_gid.count(gid) != 0) {
                err = fmt::format("duplicate Span gid {}", gid);
                return false;
            }
            out.span_by_gid.emplace(gid, std::pair<size_t, size_t>{min_i, max_i});

            for (size_t t = min_i; t <= max_i; ++t) {
                out.occupied_gadget_gap_time[gid][t] = 1;
            }
        } else {
            err = fmt::format("unexpected line before gadget_order: {}", line);
            return false;
        }
    }

    if (out.qubit_count == 0 || out.ancilla_count == 0) {
        err = "missing qubit_count or ancilla_count";
        return false;
    }
    if (out.gadget_order_gids.empty()) {
        err = "empty gadget_order";
        return false;
    }
    if (out.column_slot.empty()) {
        err = "empty column_slot";
        return false;
    }
    finalize_parsed_gadget_ordering(out, err);
    if (!err.empty()) {
        return false;
    }
    return true;
}

std::string shell_single_quote(std::filesystem::path const& p) {
    std::string s = p.string();
    std::string out;
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

std::filesystem::path resolve_sat_formulation_script() {
    if (char const* env = std::getenv("QSYN_SAT_FORMULATION")) {
        std::filesystem::path p(env);
        if (std::filesystem::is_regular_file(p)) {
            return std::filesystem::weakly_canonical(p);
        }
        spdlog::warn("QSYN_SAT_FORMULATION={} is not a regular file", env);
    }
    std::filesystem::path const cwd = std::filesystem::current_path();
    for (char const* rel : {"ancilla-minimization-with-sat/sat_formulation.py",
                            "../ancilla-minimization-with-sat/sat_formulation.py",
                            "../../ancilla-minimization-with-sat/sat_formulation.py"}) {
        auto p = cwd / rel;
        if (std::filesystem::is_regular_file(p)) {
            return std::filesystem::weakly_canonical(p);
        }
    }
    return {};
}

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

bool find_gadget_ccc_in_range(
    Tableau const& tableau,
    size_t const   ancilla_qubit,
    size_t const   search_from,
    size_t const   search_to,
    size_t&        out_ccc_idx,
    std::string&   err) {
    for (size_t idx = search_from; idx < search_to; ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct == nullptr || !cct->is_gadget() || cct->ancilla_qubit() != ancilla_qubit) {
            continue;
        }
        out_ccc_idx = idx;
        return true;
    }
    err = fmt::format("CCC gadget for ancilla q{} not found in [{}, {})", ancilla_qubit, search_from, search_to);
    return false;
}

bool find_pmc_in_range(
    Tableau const& tableau,
    size_t const   ancilla_qubit,
    size_t const   search_from,
    size_t const   search_to,
    size_t&        out_pmc_idx,
    std::string&   err) {
    for (size_t idx = search_from; idx < search_to; ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct == nullptr || cct->is_gadget() || !cct->is_classical_control()) {
            continue;
        }
        if (cct->ancilla_qubit() != ancilla_qubit) {
            continue;
        }
        out_pmc_idx = idx;
        return true;
    }
    err = fmt::format("PMC for ancilla q{} not found in [{}, {})", ancilla_qubit, search_from, search_to);
    return false;
}

size_t compute_residual_pr_index_in_unified(
    size_t const                 original_idx,
    size_t const                 slot,
    size_t const                 num_gadgets,
    std::vector<std::vector<size_t>> const& paulis_at_gap) {
    size_t current_pos = original_idx;
    for (size_t prior_slot = 0; prior_slot < slot; ++prior_slot) {
        for (size_t const prior_vid : paulis_at_gap[prior_slot]) {
            if (prior_vid < num_gadgets) {
                continue;
            }
            size_t const prior_original_idx = prior_vid - num_gadgets;
            if (prior_original_idx < original_idx) {
                --current_pos;
            }
        }
    }
    return current_pos;
}

void update_emit_positions_after_pr_move(
    std::vector<size_t>& emit_start_by_rank,
    std::vector<size_t>& ccc_pos_by_rank,
    size_t const           G,
    size_t const           t,
    size_t const           p) {
    for (size_t k = 0; k < G; ++k) {
        if (emit_start_by_rank[k] >= t && emit_start_by_rank[k] < p) {
            ++emit_start_by_rank[k];
        }
        if (ccc_pos_by_rank[k] >= t && ccc_pos_by_rank[k] < p) {
            ++ccc_pos_by_rank[k];
        }
    }
}

bool find_slot_gadget_anchor(
    Tableau const&  tableau,
    size_t const    slot,
    size_t const    ancilla_qubit,
    size_t const    cursor,
    size_t const    search_to,
    size_t&         out_emit_start,
    size_t&         out_ccc_pos,
    size_t&         out_cursor,
    std::string&    err) {
    size_t ccc_idx = 0;
    if (!find_gadget_ccc_in_range(tableau, ancilla_qubit, cursor, search_to, ccc_idx, err)) {
        return false;
    }

    bool const had_clifford =
        ccc_idx > 1 && std::holds_alternative<StabilizerTableau>(tableau[ccc_idx - 1]);
    size_t cliff_idx = had_clifford ? ccc_idx - 1 : 0;

    if (slot == 0) {
        out_emit_start = ccc_idx;
        out_ccc_pos    = ccc_idx;
        out_cursor     = ccc_idx + 1 + (had_clifford ? 1 : 0);
        return true;
    }

    if (had_clifford) {
        out_emit_start = cliff_idx;
        out_ccc_pos    = ccc_idx;
        out_cursor     = ccc_idx + 1;
        return true;
    }

    out_emit_start = ccc_idx;
    out_ccc_pos    = ccc_idx;
    out_cursor     = ccc_idx + 1;
    return true;
}

bool place_prs_with_gadget_search_swap_along(
    Tableau&                                              tableau,
    ParsedGadgetOrdering const&                           ord,
    size_t const                                          G,
    size_t const                                          num_gadgets,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::vector<std::vector<size_t>> const&               paulis_at_gap,
    std::vector<size_t>&                                  emit_start_by_rank,
    std::vector<size_t>&                                  ccc_pos_by_rank,
    std::string&                                          err) {
    emit_start_by_rank.assign(G, 0);
    ccc_pos_by_rank.assign(G, 0);

    auto unified_opt = find_unified_pr_index_for_sat(tableau);
    if (!unified_opt.has_value()) {
        err = "unified PR block not found";
        return false;
    }
    size_t unified_pr_idx = unified_opt.value();

    size_t cursor = 1;

    for (size_t slot = 0; slot < G; ++slot) {
        size_t const search_to = unified_pr_idx;
        if (cursor >= search_to) {
            err = fmt::format(
                "slot {} gid {}: no search room at cursor {} (search_to={})",
                slot,
                ord.gadget_order_gids[slot],
                cursor,
                search_to);
            return false;
        }
        size_t const gid     = ord.gadget_order_gids[slot];
        size_t const ancilla = gadgets[gid].ancilla_qubit;

        size_t emit_start = 0;
        size_t ccc_pos    = 0;
        if (!find_slot_gadget_anchor(
                tableau, slot, ancilla, cursor, search_to, emit_start, ccc_pos, cursor, err)) {
            err = fmt::format("slot {} gid {}: {}", slot, gid, err);
            return false;
        }
        emit_start_by_rank[slot] = emit_start;
        ccc_pos_by_rank[slot]    = ccc_pos;

        auto const& slot_pr_vertices = paulis_at_gap[slot];
        if (slot_pr_vertices.empty()) {
            continue;
        }

        if (unified_pr_idx >= tableau.size()) {
            err = fmt::format("unified PR index {} out of range for slot {}", unified_pr_idx, slot);
            return false;
        }

        auto* unified_pr = std::get_if<std::vector<PauliRotation>>(&tableau[unified_pr_idx]);
        if (unified_pr == nullptr) {
            err = fmt::format("expected unified PR at index {}, slot {}", unified_pr_idx, slot);
            return false;
        }

        std::vector<size_t> indices_to_remove;
        indices_to_remove.reserve(slot_pr_vertices.size());
        for (size_t const pr_vertex : slot_pr_vertices) {
            if (pr_vertex < num_gadgets) {
                continue;
            }
            indices_to_remove.push_back(pr_vertex - num_gadgets);
        }
        std::sort(indices_to_remove.begin(), indices_to_remove.end());

        std::vector<PauliRotation> slot_rotations;
        slot_rotations.reserve(indices_to_remove.size());
        for (auto it = indices_to_remove.rbegin(); it != indices_to_remove.rend(); ++it) {
            size_t const original_idx = *it;
            size_t const current_pos =
                compute_residual_pr_index_in_unified(original_idx, slot, num_gadgets, paulis_at_gap);
            if (current_pos >= unified_pr->size()) {
                err = fmt::format(
                    "PR index {} out of range (unified size {}) for slot {}",
                    current_pos,
                    unified_pr->size(),
                    slot);
                return false;
            }
            slot_rotations.push_back(std::move((*unified_pr)[current_pos]));
            unified_pr->erase(unified_pr->begin() + static_cast<std::ptrdiff_t>(current_pos));
        }
        std::reverse(slot_rotations.begin(), slot_rotations.end());

        size_t const insert_idx = unified_pr_idx;
        tableau.insert(
            tableau.begin() + static_cast<std::ptrdiff_t>(insert_idx),
            SubTableau{std::move(slot_rotations)});
        unified_pr_idx += 1;

        size_t const target_idx = emit_start_by_rank[slot];
        if (insert_idx < target_idx) {
            err = fmt::format(
                "slot {} PR block at {} is left of target {} (expected unified PR on the right)",
                slot,
                insert_idx,
                target_idx);
            return false;
        }
        if (insert_idx != target_idx) {
            try {
                swap_along(tableau, insert_idx, target_idx);
            } catch (std::exception const& e) {
                err = fmt::format(
                    "slot {} swap_along PR {} -> {} failed: {}",
                    slot,
                    insert_idx,
                    target_idx,
                    e.what());
                return false;
            }
            update_emit_positions_after_pr_move(emit_start_by_rank, ccc_pos_by_rank, G, target_idx, insert_idx);
            if (unified_pr_idx >= target_idx && unified_pr_idx < insert_idx) {
                ++unified_pr_idx;
            }
        }
        if (cursor >= target_idx && cursor < insert_idx) {
            ++cursor;
        }
    }

    return true;
}

bool permute_pmcs_on_tableau_for_sat_order(
    Tableau&                                              tableau,
    size_t const                                          G,
    ParsedGadgetOrdering const&                           ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::vector<size_t> const&                            emit_start_by_rank,
    std::vector<size_t> const&                            ccc_pos_by_rank) {
    if (G == 0) {
        return true;
    }

    size_t const inner_end = tableau.size() - 1;
    if (inner_end < 1) {
        spdlog::error("permute_pmcs_on_tableau_for_sat_order: tableau too small ({})", tableau.size());
        return false;
    }

    std::vector<size_t> pmc_pos(G, 0);
    for (size_t gid = 0; gid < G; ++gid) {
        std::string find_err;
        if (!find_pmc_in_range(
                tableau,
                gadgets[gid].ancilla_qubit,
                1,
                inner_end,
                pmc_pos[gid],
                find_err)) {
            spdlog::error(
                "permute_pmcs_on_tableau_for_sat_order: PMC for gid {} (anc q{}): {}",
                gid,
                gadgets[gid].ancilla_qubit,
                find_err);
            return false;
        }
    }

    std::vector<size_t> emit_start = emit_start_by_rank;
    std::vector<size_t> ccc_pos    = ccc_pos_by_rank;

    std::vector<size_t> movers;
    movers.reserve(G);
    for (size_t gid = 0; gid < G; ++gid) {
        auto const rank_opt = gid_rank_in_gadget_order(ord, gid);
        if (!rank_opt.has_value()) {
            spdlog::error("permute_pmcs_on_tableau_for_sat_order: gid {} missing from gadget_order", gid);
            return false;
        }
        size_t const rank = rank_opt.value();
        auto const target_opt =
            pmc_target_index_for_span(ord, gid, rank, ccc_pos, emit_start);
        if (target_opt.has_value() && pmc_pos[gid] != target_opt.value()) {
            movers.push_back(gid);
        }
    }
    std::sort(movers.begin(), movers.end(), [&](size_t a, size_t b) {
        auto const span_a = ord.span_by_gid.find(a);
        auto const span_b = ord.span_by_gid.find(b);
        size_t const max_a = span_a != ord.span_by_gid.end() ? span_a->second.second : G;
        size_t const max_b = span_b != ord.span_by_gid.end() ? span_b->second.second : G;
        if (max_a != max_b) {
            return max_a < max_b;
        }
        if (gadgets[a].ancilla_qubit != gadgets[b].ancilla_qubit) {
            return gadgets[a].ancilla_qubit < gadgets[b].ancilla_qubit;
        }
        return a < b;
    });

    for (size_t gid : movers) {
        auto const rank_opt = gid_rank_in_gadget_order(ord, gid);
        if (!rank_opt.has_value()) {
            return false;
        }
        size_t const rank = rank_opt.value();
        size_t const p    = pmc_pos[gid];
        auto const target_opt =
            pmc_target_index_for_span(ord, gid, rank, ccc_pos, emit_start);
        if (!target_opt.has_value()) {
            continue;
        }
        size_t const t = target_opt.value();
        if (p < t) {
            spdlog::error(
                "permute_pmcs_on_tableau_for_sat_order: PMC gid {} at {} already left of target {}",
                gid,
                p,
                t);
            return false;
        }
        if (p == t) {
            continue;
        }

        auto const span_it = ord.span_by_gid.find(gid);
        size_t const span_max =
            span_it != ord.span_by_gid.end() ? span_it->second.second : rank;
        size_t const update_i =
            (t == ccc_pos[rank] + 1) ? rank : span_max;

        try {
            swap_along(tableau, p, t);
        } catch (std::exception const& e) {
            spdlog::error(
                "permute_pmcs_on_tableau_for_sat_order: swap_along failed for gid {} ({} -> {}): {}",
                gid,
                p,
                t,
                e.what());
            return false;
        }

        update_pmc_emit_positions_after_move(emit_start, ccc_pos, pmc_pos, G, update_i, gid, t, p);
    }

    return true;
}

bool validate_tableau_schedule_spans(
    Tableau const&                                        tableau,
    size_t const                                          inner_start,
    size_t const                                          schedule_end,
    size_t const                                          G,
    ParsedGadgetOrdering const&                           ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    size_t                                                data_qubit_end,
    size_t                                                ancilla_qubit_hi,
    std::unordered_set<size_t> const&                     degadgetizable_gids) {
    std::vector<SubTableau> blocks;
    blocks.reserve(schedule_end - inner_start);
    for (size_t idx = inner_start; idx < schedule_end; ++idx) {
        blocks.push_back(tableau[idx]);
    }
    return validate_middle_against_span(
        blocks,
        G,
        ord,
        gadgets,
        data_qubit_end,
        ancilla_qubit_hi,
        degadgetizable_gids);
}

bool remap_tableau_schedule_to_physical_ancillae(
    Tableau&                                      tableau,
    size_t const                                  inner_start,
    size_t const                                  schedule_end,
    std::unordered_map<size_t, size_t> const&     logical_to_physical,
    size_t const                                  target_n_qubits,
    std::string&                                  err) {
    std::vector<SubTableau> blocks;
    blocks.reserve(schedule_end - inner_start);
    for (size_t idx = inner_start; idx < schedule_end; ++idx) {
        blocks.push_back(std::move(tableau[idx]));
    }
    if (!remap_middle_to_physical_ancillae(blocks, logical_to_physical, target_n_qubits, err)) {
        return false;
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
        tableau[inner_start + i] = std::move(blocks[i]);
    }
    return true;
}

std::string middle_pmc_perm_signature(std::vector<SubTableau> const& middle) {
    std::ostringstream os;
    for (size_t idx = 0; idx < middle.size(); ++idx) {
        if (auto const* pr = std::get_if<std::vector<PauliRotation>>(&middle[idx])) {
            os << idx << ":PR(" << pr->size() << ");";
            continue;
        }
        auto const* cct = std::get_if<ClassicalControlTableau>(&middle[idx]);
        if (cct == nullptr) {
            if (std::holds_alternative<StabilizerTableau>(middle[idx])) {
                os << idx << ":ST;";
            } else {
                os << idx << ":?;";
            }
            continue;
        }
        auto const ops = extract_clifford_operators(cct->operations());
        if (cct->is_gadget()) {
            os << idx << ":CCC(a=" << cct->ancilla_qubit() << ",r=" << cct->reference_qubit()
               << "):" << clifford_ops_to_string(ops) << ';';
        } else {
            os << idx << ":PMC(a=" << cct->ancilla_qubit() << ",r=" << cct->reference_qubit()
               << "):" << clifford_ops_to_string(ops) << ';';
        }
    }
    return os.str();
}

std::string describe_middle_block(std::vector<SubTableau> const& middle, size_t idx) {
    if (idx >= middle.size()) {
        return fmt::format("[{}] <out of range>", idx);
    }
    if (auto const* pr = std::get_if<std::vector<PauliRotation>>(&middle[idx])) {
        return fmt::format("[{}] PR ({} cols)", idx, pr->size());
    }
    auto const* cct = std::get_if<ClassicalControlTableau>(&middle[idx]);
    if (cct == nullptr) {
        return fmt::format("[{}] StabilizerTableau", idx);
    }
    if (cct->is_gadget()) {
        return fmt::format("[{}] CCC anc=q{} ref=q{}", idx, cct->ancilla_qubit(), cct->reference_qubit());
    }
    return fmt::format("[{}] PMC anc=q{} ref=q{}", idx, cct->ancilla_qubit(), cct->reference_qubit());
}

std::string pmc_ops_string(ClassicalControlTableau const& pmc) {
    return clifford_ops_to_string(extract_clifford_operators(pmc.operations()));
}

std::string pr_ancilla_z_summary(std::vector<PauliRotation> const& pr, size_t ancilla_lo, size_t ancilla_hi) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (auto const& rot : pr) {
        for (size_t q = ancilla_lo; q < ancilla_hi && q < rot.n_qubits(); ++q) {
            if (rot.is_i(q)) {
                continue;
            }
            if (!first) {
                os << ',';
            }
            first = false;
            os << "q" << q << ':' << (rot.is_z(q) ? 'Z' : (rot.is_x(q) ? 'X' : (rot.is_y(q) ? 'Y' : '?')));
        }
    }
    os << '}';
    return os.str();
}

bool permute_pmcs_swap_along_one(std::vector<SubTableau>& middle,
                                 size_t                      p,
                                 size_t                      t,
                                 size_t                      gid,
                                 std::optional<size_t>       trace_ancilla_qubit,
                                 std::filesystem::path const* trace_out_path,
                                 size_t                      ancilla_lo,
                                 size_t                      ancilla_hi) {
    if (p == t) {
        return true;
    }

    auto* mover_ptr = std::get_if<ClassicalControlTableau>(&middle[p]);
    if (!mover_ptr || !mover_ptr->is_classical_control()) {
        spdlog::error("permute_pmcs_swap_along_one: PMC gid {} no longer a CCT at {}", gid, p);
        return false;
    }

    bool const trace_this =
        trace_ancilla_qubit.has_value() && mover_ptr->ancilla_qubit() == trace_ancilla_qubit.value();
    std::unique_ptr<std::ofstream> trace_file;

    auto write_trace_line = [&](std::string const& line) {
        if (!trace_this) {
            return;
        }
        if (trace_file) {
            (*trace_file) << line << '\n';
        }
    };

    if (trace_this && trace_out_path != nullptr) {
        trace_file = std::make_unique<std::ofstream>(*trace_out_path, std::ios::app);
        if (!(*trace_file)) {
            spdlog::warn("permute_pmcs_swap_along_one: could not open trace file {}", trace_out_path->string());
            trace_file.reset();
        }
    }

    if (trace_this) {
        write_trace_line(fmt::format(
            "=== PMC gid {} anc=q{} move {} -> {} (swap_along bubble steps) ===",
            gid,
            mover_ptr->ancilla_qubit(),
            p,
            t));
        write_trace_line(fmt::format("initial PMC at middle[{}] ops:\n{}", p, pmc_ops_string(*mover_ptr)));
    }

    size_t step = 0;
    if (p > t) {
        for (size_t k = p; k > t; --k) {
            size_t const neighbor_idx = k - 1;
            SubTableau&  neighbor     = middle[neighbor_idx];
            auto*        pmc_at_p       = std::get_if<ClassicalControlTableau>(&middle[p]);
            if (!pmc_at_p || !pmc_at_p->is_classical_control()) {
                spdlog::error(
                    "permute_pmcs_swap_along_one: PMC gid {} lost at index {} before step {}", gid, p, step);
                return false;
            }

            if (trace_this) {
                write_trace_line(fmt::format(
                    "--- step {}: swap {} with PMC at middle[{}] ---",
                    step,
                    describe_middle_block(middle, neighbor_idx),
                    p));
                write_trace_line(fmt::format("PMC ops BEFORE step {}:\n{}", step, pmc_ops_string(*pmc_at_p)));
                if (auto* pr = std::get_if<std::vector<PauliRotation>>(&neighbor)) {
                    write_trace_line(fmt::format(
                        "PR ancilla Pauli before: {}",
                        pr_ancilla_z_summary(*pr, ancilla_lo, ancilla_hi)));
                }
            }

            try {
                swap(neighbor, middle[p]);
            } catch (std::exception const& e) {
                spdlog::error(
                    "permute_pmcs_swap_along_one: swap failed for gid {} step {} ({} <-> {}): {}",
                    gid,
                    step,
                    neighbor_idx,
                    p,
                    e.what());
                return false;
            }

            if (trace_this) {
                auto* pmc_after = std::get_if<ClassicalControlTableau>(&middle[p]);
                if (pmc_after != nullptr && pmc_after->is_classical_control()) {
                    write_trace_line(fmt::format("PMC ops AFTER step {}:\n{}", step, pmc_ops_string(*pmc_after)));
                }
                if (auto* pr = std::get_if<std::vector<PauliRotation>>(&neighbor)) {
                    write_trace_line(fmt::format(
                        "PR ancilla Pauli after: {}",
                        pr_ancilla_z_summary(*pr, ancilla_lo, ancilla_hi)));
                }
            }
            ++step;
        }
    } else {
        for (size_t k = p + 1; k <= t; ++k) {
            ++step;
            try {
                swap(middle[p], middle[k]);
            } catch (std::exception const& e) {
                spdlog::error("permute_pmcs_swap_along_one: swap failed for gid {}: {}", gid, e.what());
                return false;
            }
        }
    }

    if (trace_this) {
        write_trace_line(fmt::format("--- final: erase middle[{}], insert at middle[{}] ---", p, t));
    }

    SubTableau moved = std::move(middle[p]);
    middle.erase(middle.begin() + static_cast<std::ptrdiff_t>(p));
    middle.insert(middle.begin() + static_cast<std::ptrdiff_t>(t), std::move(moved));

    if (trace_this) {
        auto* final_pmc = std::get_if<ClassicalControlTableau>(&middle[t]);
        if (final_pmc != nullptr && final_pmc->is_classical_control()) {
            write_trace_line(fmt::format(
                "final PMC at middle[{}] ops:\n{}", t, pmc_ops_string(*final_pmc)));
        }
    }

    return true;
}

bool permute_pmcs_swap_along_one(std::vector<SubTableau>& middle, size_t p, size_t t, size_t gid) {
    return permute_pmcs_swap_along_one(middle, p, t, gid, std::nullopt, nullptr, 0, 0);
}

bool permute_pmcs_bubble_one(std::vector<SubTableau>& middle,
                             size_t                      p,
                             size_t                      t,
                             size_t                      gid) {
    auto* mover_ptr = std::get_if<ClassicalControlTableau>(&middle[p]);
    if (!mover_ptr || !mover_ptr->is_classical_control()) {
        spdlog::error("permute_pmcs_bubble_one: PMC gid {} no longer a CCT at {}", gid, p);
        return false;
    }
    ClassicalControlTableau& mover = *mover_ptr;

    for (size_t k = p; k > t; --k) {
        SubTableau& neighbor = middle[k - 1];
        if (auto* pr = std::get_if<std::vector<PauliRotation>>(&neighbor)) {
            swap(*pr, mover);
        } else if (auto* other = std::get_if<ClassicalControlTableau>(&neighbor)) {
            if (other->is_gadget()) {
                swap(*other, mover);
            } else if (!check_swap(*other, mover)) {
                spdlog::error(
                    "permute_pmcs_bubble_one: non-commuting PMCs encountered while moving gid {}", gid);
                return false;
            }
        } else if (auto* st = std::get_if<StabilizerTableau>(&neighbor)) {
            swap(*st, mover);
        } else {
            spdlog::error(
                "permute_pmcs_bubble_one: unexpected block while moving gid {} past index {}", gid, k - 1);
            return false;
        }
    }

    SubTableau moved = std::move(middle[p]);
    middle.erase(middle.begin() + static_cast<std::ptrdiff_t>(p));
    middle.insert(middle.begin() + static_cast<std::ptrdiff_t>(t), std::move(moved));
    return true;
}

void update_pmc_emit_positions_after_move(
    std::vector<size_t>& emit_start_by_rank,
    std::vector<size_t>& ccc_pos_by_rank,
    std::vector<size_t>& pmc_pos,
    size_t               G,
    size_t               i,
    size_t               gid,
    size_t               t,
    size_t               p) {
    for (size_t k = i; k < G; ++k) {
        ++emit_start_by_rank[k];
        ++ccc_pos_by_rank[k];
    }
    for (size_t other_gid = 0; other_gid < G; ++other_gid) {
        size_t& op = pmc_pos[other_gid];
        if (other_gid == gid) {
            op = t;
        } else if (op >= t && op < p) {
            ++op;
        }
    }
}

bool permute_pmcs_in_middle_for_sat_order(
    std::vector<SubTableau>& middle,
    size_t G,
    ParsedGadgetOrdering const& ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::vector<size_t> const& emit_start_by_rank,
    std::vector<size_t> const& ccc_pos_by_rank,
    bool use_swap_along,
    bool compare_bubble_vs_swap_along) {
    if (G == 0) {
        return true;
    }
    if (middle.size() < G) {
        spdlog::error("permute_pmcs_in_middle_for_sat_order: middle size {} < G {}", middle.size(), G);
        return false;
    }
    if (emit_start_by_rank.size() != G || ccc_pos_by_rank.size() != G) {
        spdlog::error(
            "permute_pmcs_in_middle_for_sat_order: emit_start/ccc_pos size mismatch ({} / {} vs G={})",
            emit_start_by_rank.size(),
            ccc_pos_by_rank.size(),
            G);
        return false;
    }

    size_t const tail_start = middle.size() - G;

    // Locate each PMC (by gid) at the tail and each gadget CCC in the prefix.
    std::vector<size_t> pmc_pos(G, 0);
    for (size_t k = 0; k < G; ++k) {
        size_t const idx = tail_start + k;
        auto*         pmc = std::get_if<ClassicalControlTableau>(&middle[idx]);
        if (!pmc || !pmc->is_classical_control()) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: expected {} tail PMCs at indices [{}..{})",
                G, tail_start, middle.size());
            return false;
        }
        pmc_pos[k] = idx;
    }

    std::vector<size_t> emit_start = emit_start_by_rank;
    std::vector<size_t> ccc_pos    = ccc_pos_by_rank;

    // Build processing order: ascending (span_max, ancilla_qubit, gid). Only gids with span_max < G move.
    std::vector<size_t> span_max(G, 0);
    for (size_t gid = 0; gid < G; ++gid) {
        auto const it = ord.span_by_gid.find(gid);
        if (it == ord.span_by_gid.end()) {
            spdlog::error("permute_pmcs_in_middle_for_sat_order: Span missing for gid {}", gid);
            return false;
        }
        size_t const max_i = it->second.second;
        if (max_i > G) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: Span max_i {} for gid {} out of range", max_i, gid);
            return false;
        }
        span_max[gid] = max_i;
    }

    std::vector<size_t> movers;
    movers.reserve(G);
    for (size_t gid = 0; gid < G; ++gid) {
        if (span_max[gid] < G) {
            movers.push_back(gid);
        }
    }
    std::sort(movers.begin(), movers.end(), [&](size_t a, size_t b) {
        if (span_max[a] != span_max[b]) {
            return span_max[a] < span_max[b];
        }
        if (gadgets[a].ancilla_qubit != gadgets[b].ancilla_qubit) {
            return gadgets[a].ancilla_qubit < gadgets[b].ancilla_qubit;
        }
        return a < b;
    });

    std::optional<size_t> const trace_ancilla_q =
        compare_bubble_vs_swap_along ? std::optional<size_t>{9} : std::nullopt;
    static std::filesystem::path const pmc_trace_path =
        "/home/ferayer/minimize_ancilla/pmc_anc9_swap_trace.txt";
    std::filesystem::path const* trace_path_ptr =
        compare_bubble_vs_swap_along ? &pmc_trace_path : nullptr;
    size_t const ancilla_lo = ord.qubit_count >= ord.ancilla_count ? ord.qubit_count - ord.ancilla_count : 0;
    size_t const ancilla_hi = ord.qubit_count;
    if (compare_bubble_vs_swap_along) {
        std::ofstream clear_trace(pmc_trace_path, std::ios::trunc);
        if (clear_trace) {
            clear_trace << "PMC anc=q9 swap trace (SAT reorder apply)\n";
            clear_trace << "ancilla qubits: [" << ancilla_lo << ", " << ancilla_hi << ")\n\n";
        }
    }

    for (size_t gid : movers) {
        size_t const p = pmc_pos[gid];
        size_t const i = span_max[gid];
        size_t const t = emit_start[i];
        if (p < t) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: PMC gid {} at index {} is already left of emit target {}",
                gid, p, t);
            return false;
        }
        if (p == t) {
            continue;
        }

        if (compare_bubble_vs_swap_along) {
            std::vector<SubTableau> bubble_copy = middle;
            std::vector<SubTableau> along_copy  = middle;
            std::vector<size_t>     bubble_ccc  = ccc_pos;
            std::vector<size_t>     along_ccc   = ccc_pos;
            std::vector<size_t>     bubble_emit = emit_start;
            std::vector<size_t>     along_emit  = emit_start;
            std::vector<size_t>     bubble_pmc  = pmc_pos;
            std::vector<size_t>     along_pmc   = pmc_pos;
            size_t const            bp          = bubble_pmc[gid];
            size_t const            ap          = along_pmc[gid];
            size_t const            bt          = bubble_emit[i];
            size_t const            at          = along_emit[i];
            if (!permute_pmcs_bubble_one(bubble_copy, bp, bt, gid)) {
                return false;
            }
            if (!permute_pmcs_swap_along_one(along_copy, ap, at, gid)) {
                return false;
            }
            auto const sig_bubble = middle_pmc_perm_signature(bubble_copy);
            auto const sig_along  = middle_pmc_perm_signature(along_copy);
            (void)sig_bubble;
            (void)sig_along;
        }

        bool const moved_ok =
            use_swap_along
                ? permute_pmcs_swap_along_one(
                      middle, p, t, gid, trace_ancilla_q, trace_path_ptr, ancilla_lo, ancilla_hi)
                : permute_pmcs_bubble_one(middle, p, t, gid);
        if (!moved_ok) {
            return false;
        }

        update_pmc_emit_positions_after_move(emit_start, ccc_pos, pmc_pos, G, i, gid, t, p);
    }

    return true;
}

std::unordered_set<size_t> allowed_ancillas_at_gap_from_span(
    size_t const                                              gap_i,
    ParsedGadgetOrdering const&                               ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets) {
    std::unordered_set<size_t> allowed_ancillas;
    allowed_ancillas.reserve(ord.span_by_gid.size());
    for (auto const& [gid, span] : ord.span_by_gid) {
        if (gid >= gadgets.size()) {
            continue;
        }
        if (ord.degadgetizable_gids.count(gid) != 0) {
            continue;
        }
        if (span.first <= gap_i && gap_i <= span.second) {
            allowed_ancillas.insert(gadgets[gid].ancilla_qubit);
        }
    }
    return allowed_ancillas;
}

bool validate_middle_against_span(
    std::vector<SubTableau> const&                          middle,
    size_t const                                            G,
    ParsedGadgetOrdering const&                             ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    size_t                                                  data_qubit_end,
    size_t                                                  ancilla_qubit_hi,
    std::unordered_set<size_t> const&                       degadgetizable_gids) {
    if (ord.span_by_gid.empty() && degadgetizable_gids.size() != G) {
        spdlog::error("validate_middle_against_span: empty Span section");
        return false;
    }

    size_t gadgets_seen = 0;

    for (size_t idx = 0; idx < middle.size(); ++idx) {
        size_t const gap_i = gadgets_seen;
        if (gap_i > G) {
            spdlog::error(
                "validate_middle_against_span: gap index {} > G {} at block {}",
                gap_i,
                G,
                idx);
            return false;
        }

        auto const allowed_ancillas = allowed_ancillas_at_gap_from_span(gap_i, ord, gadgets);

        if (auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&middle[idx])) {
            for (auto const& rotation : *pr_vec) {
                for (size_t q = data_qubit_end; q < ancilla_qubit_hi && q < rotation.n_qubits(); ++q) {
                    if (rotation.is_i(q)) {
                        continue;
                    }
                    if (allowed_ancillas.count(q) == 0) {
                        spdlog::error(
                            "sat_reorder_apply: span violation at middle[{}] PR, gap i={}, "
                            "ancilla q={} outside active span",
                            idx,
                            gap_i,
                            q);
                        return false;
                    }
                }
            }
            continue;
        }

        auto const* cct = std::get_if<ClassicalControlTableau>(&middle[idx]);
        if (cct == nullptr) {
            continue;
        }

        size_t const own_anc = cct->ancilla_qubit();
        if (gap_i < G) {
            for (size_t const q :
                 ancillae_touched_by_cct_ops(*cct, data_qubit_end, ancilla_qubit_hi)) {
                if (q == own_anc) {
                    continue;
                }
                if (allowed_ancillas.count(q) == 0) {
                    spdlog::error(
                        "sat_reorder_apply: span violation at middle[{}] CCT, gap i={}, "
                        "ancilla q={} touched outside active span",
                        idx,
                        gap_i,
                        q);
                    return false;
                }
            }
        }

        if (cct->is_classical_control() && !cct->is_gadget() && gap_i < G) {
            if (own_anc >= data_qubit_end && own_anc < ancilla_qubit_hi &&
                allowed_ancillas.count(own_anc) == 0) {
                bool own_early_pmc = false;
                for (size_t gid = 0; gid < gadgets.size(); ++gid) {
                    if (gadgets[gid].ancilla_qubit != own_anc) {
                        continue;
                    }
                    if (degadgetizable_gids.count(gid) != 0) {
                        own_early_pmc = true;
                        break;
                    }
                    auto const rank_opt = gid_rank_in_gadget_order(ord, gid);
                    if (!rank_opt.has_value()) {
                        break;
                    }
                    auto const span_it = ord.span_by_gid.find(gid);
                    if (span_it != ord.span_by_gid.end() &&
                        span_it->second.first == rank_opt.value()) {
                        own_early_pmc = true;
                    }
                    break;
                }
                if (!own_early_pmc) {
                    spdlog::error(
                        "sat_reorder_apply: span violation at middle[{}] PMC ancilla q={}, gap i={}",
                        idx,
                        own_anc,
                        gap_i);
                    return false;
                }
            }
        }

        if (cct->is_gadget()) {
            if (gadgets_seen >= G) {
                spdlog::error("validate_middle_against_span: too many gadget CCC blocks at middle[{}]", idx);
                return false;
            }
            size_t const expected_gid = ord.gadget_order_gids[gadgets_seen];
            if (cct->ancilla_qubit() != gadgets[expected_gid].ancilla_qubit) {
                spdlog::error(
                    "sat_reorder_apply: gadget order mismatch at middle[{}]: expected gid {} anc q{}, got anc q{}",
                    idx,
                    expected_gid,
                    gadgets[expected_gid].ancilla_qubit,
                    cct->ancilla_qubit());
                return false;
            }
            ++gadgets_seen;
        }
    }

    if (gadgets_seen != G) {
        spdlog::error(
            "validate_middle_against_span: saw {} gadget CCC blocks, expected {}",
            gadgets_seen,
            G);
        return false;
    }

    return true;
}

bool build_post_degadgetize_gid_ancillae(
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::unordered_set<size_t> const&                       skip_gids,
    std::vector<size_t> const&                              removed_ancillae,
    std::unordered_map<size_t, size_t>&                     out_gid_to_ancilla,
    std::string&                                            err) {
    (void)err;
    out_gid_to_ancilla.clear();
    out_gid_to_ancilla.reserve(gadgets.size() - skip_gids.size());

    for (size_t gid = 0; gid < gadgets.size(); ++gid) {
        if (skip_gids.count(gid) != 0) {
            continue;
        }
        out_gid_to_ancilla.emplace(gid, gadgets[gid].ancilla_qubit);
    }

    std::vector<size_t> removed = removed_ancillae;
    std::sort(removed.begin(), removed.end(), std::greater<size_t>());
    for (size_t const removed_anc : removed) {
        for (auto& [gid, anc] : out_gid_to_ancilla) {
            if (anc > removed_anc) {
                --anc;
            }
        }
    }
    return true;
}

bool build_ancilla_intervals_from_span(ParsedGadgetOrdering const& ord,
                                       std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
                                       std::unordered_set<size_t> const& skip_lane_gids,
                                       std::unordered_map<size_t, size_t> const& gid_to_current_ancilla,
                                       std::vector<AncillaInterval>& intervals,
                                       std::string& err) {
    size_t const G = gadgets.size();
    intervals.clear();
    intervals.reserve(G);

    for (size_t gid = 0; gid < G; ++gid) {
        if (skip_lane_gids.count(gid) != 0) {
            continue;
        }
        auto const it = ord.span_by_gid.find(gid);
        if (it == ord.span_by_gid.end()) {
            err = fmt::format("missing Span for non-degadgetizable gid {}", gid);
            return false;
        }
        auto const [min_i, max_i] = it->second;
        if (min_i > max_i) {
            err = fmt::format("invalid Span for gid {}: min_i {} > max_i {}", gid, min_i, max_i);
            return false;
        }
        auto const anc_it = gid_to_current_ancilla.find(gid);
        if (anc_it == gid_to_current_ancilla.end()) {
            err = fmt::format("missing post-degadgetize ancilla index for gid {}", gid);
            return false;
        }
        AncillaInterval interval;
        interval.gid                   = gid;
        interval.logical_ancilla_qubit = anc_it->second;
        interval.min_gap_i             = min_i;
        interval.max_gap_i             = max_i;
        intervals.push_back(interval);
    }
    std::sort(intervals.begin(), intervals.end(), [](AncillaInterval const& lhs, AncillaInterval const& rhs) {
        if (lhs.min_gap_i != rhs.min_gap_i) {
            return lhs.min_gap_i < rhs.min_gap_i;
        }
        if (lhs.max_gap_i != rhs.max_gap_i) {
            return lhs.max_gap_i < rhs.max_gap_i;
        }
        return lhs.gid < rhs.gid;
    });
    return true;
}

bool build_ancilla_occupancy_tableau(std::vector<AncillaInterval> const& intervals,
                                     ParsedGadgetOrdering const& ord,
                                     size_t const                          ancilla_base,
                                     AncillaOccupancyTableau& out,
                                     std::string& err) {
    size_t const G = ord.gadget_order_gids.size();
    if (G == 0) {
        out = {};
        return true;
    }
    size_t const width_w = ord.width_w > 0 ? ord.width_w : ord.ancilla_count;
    if (width_w == 0) {
        err = "width is zero";
        return false;
    }
    if (ord.ancilla_count == 0 || ord.qubit_count < ord.ancilla_count) {
        err = fmt::format("invalid qubit/ancilla counts: qubit_count={}, ancilla_count={}",
                          ord.qubit_count,
                          ord.ancilla_count);
        return false;
    }

    std::vector<std::vector<size_t>> starts(G + 1);
    std::vector<std::vector<size_t>> ends(G + 1);
    for (auto const& iv : intervals) {
        if (iv.gid >= G) {
            err = fmt::format("interval gid {} out of range [0,{})", iv.gid, G);
            return false;
        }
        if (iv.min_gap_i > iv.max_gap_i || iv.max_gap_i > G) {
            err = fmt::format("interval gid {} has out-of-range span [{},{}] for G={}",
                              iv.gid,
                              iv.min_gap_i,
                              iv.max_gap_i,
                              G);
            return false;
        }
        starts[iv.min_gap_i].push_back(iv.gid);
        ends[iv.max_gap_i].push_back(iv.gid);
    }

    out.width_w            = width_w;
    out.ancilla_base_qubit = ancilla_base;
    out.occupied_gid_by_time_lane.assign(G + 1, std::vector<int64_t>(width_w, -1));
    out.gid_to_lane.clear();
    out.gid_to_physical_ancilla.clear();
    out.gid_to_lane.reserve(G);
    out.gid_to_physical_ancilla.reserve(G);

    std::set<size_t> free_lanes;
    for (size_t lane = 0; lane < width_w; ++lane) {
        free_lanes.insert(lane);
    }
    std::vector<int64_t> lane_to_gid(width_w, -1);

    for (size_t t = 0; t <= G; ++t) {
        if (t > 0) {
            for (size_t gid : ends[t - 1]) {
                auto const lane_it = out.gid_to_lane.find(gid);
                if (lane_it == out.gid_to_lane.end()) {
                    err = fmt::format("release gid {} at t={} has no lane assignment", gid, t - 1);
                    return false;
                }
                size_t const lane = lane_it->second;
                lane_to_gid[lane] = -1;
                free_lanes.insert(lane);
            }
        }

        auto& starters = starts[t];
        std::sort(starters.begin(), starters.end());
        for (size_t gid : starters) {
            if (free_lanes.empty()) {
                err = fmt::format("insufficient ancilla lanes at t={} (need width > {})", t, width_w);
                return false;
            }
            size_t const lane = *free_lanes.begin();
            free_lanes.erase(free_lanes.begin());
            lane_to_gid[lane]               = static_cast<int64_t>(gid);
            out.gid_to_lane[gid]            = lane;
            out.gid_to_physical_ancilla[gid] = ancilla_base + lane;
        }

        for (size_t lane = 0; lane < width_w; ++lane) {
            out.occupied_gid_by_time_lane[t][lane] = lane_to_gid[lane];
        }
    }
    return true;
}

std::unordered_map<size_t, size_t> build_logical_to_physical_ancilla_map(
    std::vector<AncillaInterval> const& intervals,
    AncillaOccupancyTableau const&      ancilla_occupancy) {
    std::unordered_map<size_t, size_t> logical_to_physical;
    logical_to_physical.reserve(intervals.size());
    for (auto const& interval : intervals) {
        auto const it = ancilla_occupancy.gid_to_physical_ancilla.find(interval.gid);
        if (it == ancilla_occupancy.gid_to_physical_ancilla.end()) {
            continue;
        }
        logical_to_physical[interval.logical_ancilla_qubit] = it->second;
    }
    return logical_to_physical;
}

size_t remap_qubit(size_t q, std::unordered_map<size_t, size_t> const& logical_to_physical) {
    auto const map_it = logical_to_physical.find(q);
    return (map_it != logical_to_physical.end()) ? map_it->second : q;
}

void remap_clifford_ops_inplace(CliffordOperatorString& ops,
                                std::unordered_map<size_t, size_t> const& logical_to_physical) {
    for (auto& [type, qubits] : ops) {
        qubits[0] = remap_qubit(qubits[0], logical_to_physical);
        if (type == CliffordOperatorType::cx || type == CliffordOperatorType::cz ||
            type == CliffordOperatorType::swap || type == CliffordOperatorType::ecr) {
            qubits[1] = remap_qubit(qubits[1], logical_to_physical);
        }
    }
}

bool remap_pauli_rotation_qubits(PauliRotation& rotation,
                                 std::unordered_map<size_t, size_t> const& logical_to_physical,
                                 size_t target_n_qubits,
                                 std::string& err) {
    std::vector<Pauli> remapped(target_n_qubits, Pauli::i);
    for (size_t q = 0; q < rotation.n_qubits(); ++q) {
        auto const   p = rotation.get_pauli_type(q);
        size_t const dst = remap_qubit(q, logical_to_physical);
        if (p == Pauli::i) {
            continue;
        }
        if (dst >= remapped.size()) {
            err = fmt::format("Pauli remap out of range: src q{} -> dst q{} with n_qubits={}",
                              q,
                              dst,
                              remapped.size());
            return false;
        }
        if (remapped[dst] != Pauli::i) {
            err = fmt::format(
                "Pauli remap collision: src q{} and another source both map to dst q{} (non-identity overlap)",
                q,
                dst);
            return false;
        }
        remapped[dst] = p;
    }
    bool const was_cz = rotation.is_CZ();
    rotation          = PauliRotation(remapped.begin(), remapped.end(), rotation.phase());
    if (was_cz && rotation.phase() == dvlab::Phase(0)) {
        size_t z_count = 0;
        for (size_t q = 0; q < rotation.n_qubits(); ++q) {
            if (rotation.get_pauli_type(q) == Pauli::z) {
                ++z_count;
            }
        }
        rotation.set_is_CZ(z_count == 2);
    } else {
        rotation.set_is_CZ(false);
    }
    return true;
}

bool remap_cct_qubits(ClassicalControlTableau& cct,
                      std::unordered_map<size_t, size_t> const& logical_to_physical,
                      size_t target_n_qubits) {
    size_t const old_ancilla = cct.ancilla_qubit();
    size_t const new_ancilla = remap_qubit(old_ancilla, logical_to_physical);
    size_t const old_ref     = cct.reference_qubit();
    size_t const new_ref     = remap_qubit(old_ref, logical_to_physical);

    if (cct.is_gadget()) {
        cct.operations() = StabilizerTableau{target_n_qubits};
        cct.set_qubits(new_ancilla, new_ref);
        return true;
    }

    auto const ops_old = extract_clifford_operators(cct.operations());
    auto       ops_new = ops_old;
    remap_clifford_ops_inplace(ops_new, logical_to_physical);

    ClassicalControlTableau rebuilt(new_ancilla, new_ref, target_n_qubits, CCTType::ClassicalControl);
    rebuilt.operations() = StabilizerTableau{target_n_qubits};
    rebuilt.operations().apply(ops_new);
    rebuilt.set_measurement_type(cct.measurement_type());
    cct = std::move(rebuilt);
    return true;
}

bool remap_middle_to_physical_ancillae(std::vector<SubTableau>& middle,
                                       std::unordered_map<size_t, size_t> const& logical_to_physical,
                                       size_t target_n_qubits,
                                       std::string& err) {
    for (size_t idx = 0; idx < middle.size(); ++idx) {
        if (auto* st = std::get_if<StabilizerTableau>(&middle[idx])) {
            auto ops = extract_clifford_operators(*st);
            remap_clifford_ops_inplace(ops, logical_to_physical);
            StabilizerTableau rebuilt{target_n_qubits};
            rebuilt.apply(ops);
            *st = std::move(rebuilt);
            continue;
        }
        if (auto* pr = std::get_if<std::vector<PauliRotation>>(&middle[idx])) {
            for (auto& rotation : *pr) {
                if (!remap_pauli_rotation_qubits(rotation, logical_to_physical, target_n_qubits, err)) {
                    err = fmt::format("middle[{}] PR remap failed: {}", idx, err);
                    return false;
                }
            }
            continue;
        }
        if (auto* cct = std::get_if<ClassicalControlTableau>(&middle[idx])) {
            remap_cct_qubits(*cct, logical_to_physical, target_n_qubits);
            continue;
        }
        err = fmt::format("middle[{}] has unsupported block type for ancilla remap", idx);
        return false;
    }
    return true;
}

bool exported_signature_format_valid(std::filesystem::path const& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    bool seen_gadget_order = false;
    bool seen_block_left   = false;
    bool seen_block_right  = false;
    std::string line;
    while (std::getline(in, line)) {
        if (auto const hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.erase(line.begin());
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line == "gadget_order") {
            seen_gadget_order = true;
        } else if (line == "block_left") {
            seen_block_left = true;
        } else if (line == "block_right") {
            seen_block_right = true;
        }
    }
    return seen_gadget_order && seen_block_left && seen_block_right;
}

bool write_sat_signature_export(std::filesystem::path const& path, SatSignatureExport const& exp) {
    std::ofstream out(path);
    if (!out) {
        spdlog::error("sat_reorder_export: cannot open constraint file '{}'", path.string());
        return false;
    }

    out << "qubit_count: " << exp.qubit_count << "\n";
    out << "ancilla_count: " << exp.ancilla_count << "\n";
    out << "pauli_count: " << exp.pauli_count << "\n";

    out << "gadget_order\n";
    for (size_t rank = 0; rank < exp.gadget_order.size(); ++rank) {
        size_t const gid = exp.gadget_order[rank];
        if (gid >= exp.blocks_by_gid.size()) {
            spdlog::error("sat_reorder_export: gadget_order gid {} out of range", gid);
            return false;
        }
        out << rank << " " << gid << " " << exp.blocks_by_gid[gid].ancilla_qubit << "\n";
    }

    out << "block_left\n";
    for (size_t gid = 0; gid < exp.blocks_by_gid.size(); ++gid) {
        auto const& pids = exp.blocks_by_gid[gid].block_left;
        if (pids.empty()) {
            continue;
        }
        out << gid;
        for (size_t pid : pids) {
            out << " " << pid;
        }
        out << "\n";
    }

    out << "block_right\n";
    for (size_t gid = 0; gid < exp.blocks_by_gid.size(); ++gid) {
        auto const& pids = exp.blocks_by_gid[gid].block_right;
        if (pids.empty()) {
            continue;
        }
        out << gid;
        for (size_t pid : pids) {
            out << " " << pid;
        }
        out << "\n";
    }

    return true;
}

bool export_constraint_for_sat_reorder(Tableau const& tableau, std::filesystem::path const& path) {
    // block_right: Z on gadget ancilla in unified PR; block_left: Z on ancilla after swap_along_test(pr, 1).
    SatSignatureExport const exp = compute_sat_signature_blocks(tableau);
    return write_sat_signature_export(path, exp);
}

}  // namespace

bool sat_reorder_export(Tableau& tableau, std::filesystem::path const& work_dir) {
    std::error_code ec;
    std::filesystem::create_directories(work_dir, ec);
    if (ec) {
        spdlog::error("sat_reorder_export: cannot create work_dir {}: {}", work_dir.string(), ec.message());
        return false;
    }

    std::filesystem::path const constraint = work_dir / "gadget_constraint.txt";
    if (!export_constraint_for_sat_reorder(tableau, constraint)) {
        return false;
    }
    if (!exported_signature_format_valid(constraint)) {
        spdlog::error(
            "sat_reorder_export: exported constraint file '{}' missing required signature sections "
            "(gadget_order, block_left, block_right)",
            constraint.string());
        return false;
    }

    // Also export a stable SMT input snapshot for debugging/reruns.
    {
        std::filesystem::path const export_input_dir("/home/ferayer/TODD/run_qsyn/input");
        std::filesystem::path const export_input = export_input_dir / "gadget_constraint.txt";
        std::error_code ec2;
        std::filesystem::create_directories(export_input_dir, ec2);
        if (ec2) {
            spdlog::error(
                "sat_reorder_export: failed to create input export dir {}: {}",
                export_input_dir.string(),
                ec2.message());
            return false;
        }
        std::filesystem::copy_file(constraint, export_input, std::filesystem::copy_options::overwrite_existing, ec2);
        if (ec2) {
            spdlog::error(
                "sat_reorder_export: failed to export input {} -> {}: {}",
                constraint.string(),
                export_input.string(),
                ec2.message());
            return false;
        }
    }
    return true;
}

bool sat_reorder_run_solver(std::filesystem::path const& work_dir, std::filesystem::path const& sat_formulation_py) {
    if (!std::filesystem::is_regular_file(sat_formulation_py)) {
        spdlog::error("sat_reorder_run_solver: missing script {}", sat_formulation_py.string());
        return false;
    }
    std::filesystem::path const input = work_dir / "gadget_constraint.txt";
    std::filesystem::path const ordering_out = work_dir / "gadget_ordering.txt";
    std::filesystem::path const export_input_dir("/home/ferayer/TODD/run_qsyn/input");
    std::filesystem::path const export_output_dir("/home/ferayer/TODD/run_qsyn/output");
    if (!std::filesystem::is_regular_file(input)) {
        spdlog::error("sat_reorder_run_solver: missing {}", input.string());
        return false;
    }

    {
        std::error_code ec;
        std::filesystem::create_directories(export_input_dir, ec);
        if (ec) {
            spdlog::error(
                "sat_reorder_run_solver: failed to create input export dir {}: {}",
                export_input_dir.string(),
                ec.message());
        } else {
            std::filesystem::path const export_input = export_input_dir / input.filename();
            std::filesystem::copy_file(input, export_input, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                spdlog::error(
                    "sat_reorder_run_solver: failed to export input {} -> {}: {}",
                    input.string(),
                    export_input.string(),
                    ec.message());
            }
        }
    }

    std::ostringstream cmd;
    cmd << "python3 " << shell_single_quote(sat_formulation_py) << " --input " << shell_single_quote(input)
        << " --ordering-out " << shell_single_quote(ordering_out) << " --quiet";

    std::string const cmd_str = cmd.str();
    int const rc = std::system(cmd_str.c_str());
    if (rc != 0) {
        spdlog::warn("sat_reorder_run_solver: exit code {}", rc);
        return false;
    }
    if (!std::filesystem::is_regular_file(ordering_out)) {
        spdlog::warn("sat_reorder_run_solver: expected output missing {}", ordering_out.string());
        return false;
    }
    {
        std::error_code ec;
        std::filesystem::create_directories(export_output_dir, ec);
        if (ec) {
            spdlog::error(
                "sat_reorder_run_solver: failed to create output export dir {}: {}",
                export_output_dir.string(),
                ec.message());
        } else {
            std::filesystem::path const export_output = export_output_dir / ordering_out.filename();
            std::filesystem::copy_file(ordering_out, export_output, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                spdlog::error(
                    "sat_reorder_run_solver: failed to export output {} -> {}: {}",
                    ordering_out.string(),
                    export_output.string(),
                    ec.message());
            }
        }
    }
    return true;
}

bool sat_reorder_apply(Tableau& tableau,
                       std::filesystem::path const& ordering_path) {
    std::string perr;
    ParsedGadgetOrdering ord;
    if (!parse_gadget_ordering_file(ordering_path, ord, perr)) {
        spdlog::error("sat_reorder_apply: parse failed: {}", perr);
        return false;
    }

    auto gadgets = export_hadamard_gadget_pairs(tableau);
    size_t const num_gadgets = gadgets.size();

    SatSignatureExport const sig_pre = compute_sat_signature_blocks(tableau);
    (void)sig_pre;

    auto const structure = inspect_degadgetization_structure(tableau);
    if (!structure.is_valid) {
        spdlog::error(
            "sat_reorder_apply: invalid post T-opt layout (expected "
            "{{ST0, (CCC|ST)*, PR, [CX-ST], PMCs, ST_back}})");
        return false;
    }
    if (structure.ccc_count != num_gadgets) {
        spdlog::error(
            "sat_reorder_apply: gadget count mismatch (structure {} vs tableau {})",
            structure.ccc_count,
            num_gadgets);
        return false;
    }

    size_t const G = num_gadgets;
    std::vector<std::vector<size_t>> paulis_at_gap(G + 1);
    for (auto const& [pid, gap] : ord.column_slot) {
        if (gap > G) {
            spdlog::error("sat_reorder_apply: column_slot pid {} gap {} > G={}", pid, gap, G);
            return false;
        }
        paulis_at_gap[gap].push_back(pid);
    }
    for (auto& bucket : paulis_at_gap) {
        std::sort(bucket.begin(), bucket.end());
    }

    if (tableau.size() < 2) {
        spdlog::error("sat_reorder_apply: tableau too small");
        return false;
    }

    std::vector<size_t> emit_start_by_rank(G, 0);
    std::vector<size_t> ccc_pos_by_rank(G, 0);
    std::string         place_err;
    if (!place_prs_with_gadget_search_swap_along(
            tableau,
            ord,
            G,
            num_gadgets,
            gadgets,
            paulis_at_gap,
            emit_start_by_rank,
            ccc_pos_by_rank,
            place_err)) {
        spdlog::error("sat_reorder_apply: PR placement failed: {}", place_err);
        return false;
    }

    size_t const data_qubits      = tableau.n_qubits() - tableau.n_ancilla();
    size_t const sat_width_w      = ord.width_w > 0 ? ord.width_w : ord.ancilla_count;
    size_t const ancilla_hi       = ord.qubit_count > 0 ? ord.qubit_count : tableau.n_qubits();
    size_t const inner_start      = 1;
    size_t const schedule_end     = tableau.size() - 1;

    if (!permute_pmcs_on_tableau_for_sat_order(
            tableau, G, ord, gadgets, emit_start_by_rank, ccc_pos_by_rank)) {
        return false;
    }

    if (!validate_tableau_schedule_spans(
            tableau,
            inner_start,
            schedule_end,
            G,
            ord,
            gadgets,
            data_qubits,
            ancilla_hi,
            ord.degadgetizable_gids)) {
        spdlog::error("sat_reorder_apply: span check failed after reorder");
        return false;
    }
    spdlog::info(
        "sat_reorder_apply: span check passed (PR, CCC/gadget, and PMC blocks in schedule middle)");

    size_t const orig_n_ancilla = tableau.n_ancilla();

    std::vector<size_t> degadgetize_ancillae;
    degadgetize_ancillae.reserve(ord.degadgetizable_gids.size());
    for (size_t const gid : ord.degadgetizable_gids) {
        degadgetize_ancillae.push_back(gadgets[gid].ancilla_qubit);
    }

    size_t degadgetized_count = 0;
    if (degadgetize_ancillae.empty()) {
        spdlog::info(
            "sat_reorder_apply: degadgetization passed (0 SMT-exported degadgetizable gadgets)");
    } else {
        degadgetized_count = hadamard_degadgetize(tableau, degadgetize_ancillae);
        spdlog::info(
            "sat_reorder_apply: degadgetized {}/{} SMT-exported degadgetizable gadgets",
            degadgetized_count,
            degadgetize_ancillae.size());
        if (degadgetized_count == degadgetize_ancillae.size()) {
            spdlog::info("sat_reorder_apply: degadgetization passed");
        } else {
            spdlog::warn(
                "sat_reorder_apply: degadgetization failed ({}/{}; PMC must be single X on "
                "reference after PMC move)",
                degadgetized_count,
                degadgetize_ancillae.size());
        }
    }

    std::unordered_map<size_t, size_t> gid_to_current_ancilla;
    std::string                        ancilla_refresh_err;
    if (!build_post_degadgetize_gid_ancillae(
            gadgets,
            ord.degadgetizable_gids,
            degadgetize_ancillae,
            gid_to_current_ancilla,
            ancilla_refresh_err)) {
        spdlog::error("sat_reorder_apply: post-degadgetize ancilla refresh failed: {}", ancilla_refresh_err);
        return false;
    }

    size_t const data_qubits_post  = tableau.n_qubits() - tableau.n_ancilla();
    size_t const target_n_qubits_post = data_qubits_post + sat_width_w;
    size_t const ancilla_base_post = data_qubits_post;
    size_t const schedule_end_post = tableau.size() - 1;

    std::vector<AncillaInterval> ancilla_intervals;
    std::string                  interval_err;
    if (!build_ancilla_intervals_from_span(
            ord,
            gadgets,
            ord.degadgetizable_gids,
            gid_to_current_ancilla,
            ancilla_intervals,
            interval_err)) {
        spdlog::error("sat_reorder_apply: build_ancilla_intervals_from_span failed: {}", interval_err);
        return false;
    }
    AncillaOccupancyTableau ancilla_occupancy;
    std::string             occupancy_err;
    if (!build_ancilla_occupancy_tableau(
            ancilla_intervals, ord, ancilla_base_post, ancilla_occupancy, occupancy_err)) {
        spdlog::error("sat_reorder_apply: build_ancilla_occupancy_tableau failed: {}", occupancy_err);
        return false;
    }
    auto const logical_to_physical = build_logical_to_physical_ancilla_map(ancilla_intervals, ancilla_occupancy);

    std::string remap_err;
    if (!remap_tableau_schedule_to_physical_ancillae(
            tableau,
            inner_start,
            schedule_end_post,
            logical_to_physical,
            target_n_qubits_post,
            remap_err)) {
        spdlog::error("sat_reorder_apply: ancilla remap failed: {}", remap_err);
        return false;
    }

    tableau.set_n_qubits(target_n_qubits_post);
    tableau.set_n_ancilla(sat_width_w);

    remove_identities(tableau);
    spdlog::info(
        "sat_reorder_apply: reduced {} ancilla ({} -> {})",
        orig_n_ancilla - tableau.n_ancilla(),
        orig_n_ancilla,
        tableau.n_ancilla());
    return true;
}

void sat_reorder(Tableau& tableau) {
    int const pid =
#ifdef __unix__
        static_cast<int>(getpid());
#else
        0;
#endif
    std::filesystem::path const work_dir =
        std::filesystem::temp_directory_path() / fmt::format("qsyn_sat_{}", pid);

    std::error_code ec;
    std::filesystem::create_directories(work_dir, ec);
    if (ec) {
        spdlog::error("sat_reorder: mkdir {} failed: {}", work_dir.string(), ec.message());
        spdlog::info("sat_reorder_apply: failed");
        return;
    }

    std::filesystem::path const script = resolve_sat_formulation_script();
    if (script.empty()) {
        spdlog::warn(
            "sat_reorder: set QSYN_SAT_FORMULATION to sat_formulation.py or run from a directory "
            "that contains ancilla-minimization-with-sat/; falling back to topological reorder");
        spdlog::info("sat_reorder_apply: failed");
        return;
    }

    if (!sat_reorder_export(tableau, work_dir)) {
        spdlog::info("sat_reorder_apply: failed");
        return;
    }
    if (!sat_reorder_run_solver(work_dir, script)) {
        spdlog::info("sat_reorder_apply: failed");
        return;
    }
    std::filesystem::path const ordering = work_dir / "gadget_ordering.txt";
    if (!sat_reorder_apply(tableau, ordering)) {
        spdlog::info("sat_reorder_apply: failed");
    }
}

}  // namespace qsyn::experimental
