/**
 * @file reorder_smt.cpp
 * @brief SAT/SMT-driven gadget / Pauli column reorder (export → Z3 → apply); may degadgetize when schedule + blocking allow.
 */

#include "../tableau_optimization.hpp"

#include "./reorder_smt.hpp"

#include "tableau/classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <z3++.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qsyn::experimental {

namespace {

using PosMap = std::unordered_map<size_t, size_t>;

bool contains_pid(std::vector<size_t> const& pids, size_t const pid) {
    return std::find(pids.begin(), pids.end(), pid) != pids.end();
}

PauliClassKind classify_signature_class(AncillaSmtInstance const& inst, size_t const sample_pid) {
    bool has_left  = false;
    bool has_right = false;
    for (size_t gid = 0; gid < inst.block_left.size(); ++gid) {
        has_left  = has_left || contains_pid(inst.block_left[gid], sample_pid);
        has_right = has_right || contains_pid(inst.block_right[gid], sample_pid);
    }
    if (!has_left) {
        return PauliClassKind::Fix0;
    }
    if (!has_right) {
        return PauliClassKind::FixG;
    }
    return PauliClassKind::Sat;
}

int signature_code(AncillaSmtInstance const& inst, size_t const pid, size_t const gid) {
    bool const in_left  = contains_pid(inst.block_left[gid], pid);
    bool const in_right = contains_pid(inst.block_right[gid], pid);
    if (in_left && in_right) {
        return 3;
    }
    if (in_left) {
        return 1;
    }
    if (in_right) {
        return 2;
    }
    return 0;
}

void build_signature_reduction(AncillaSmtInstance& inst) {
    size_t const G      = inst.gadget_order_gids.size();
    size_t const pid_lo = G;
    size_t const pid_hi = G + inst.pauli_count;

    std::map<std::vector<int>, std::vector<size_t>> buckets;
    for (size_t pid = pid_lo; pid < pid_hi; ++pid) {
        std::vector<int> sig;
        sig.reserve(G);
        for (size_t gid = 0; gid < G; ++gid) {
            sig.push_back(signature_code(inst, pid, gid));
        }
        buckets[std::move(sig)].push_back(pid);
    }

    PauliColumnReduction reduction;
    reduction.G = G;

    for (auto& [_sig, pids] : buckets) {
        std::sort(pids.begin(), pids.end());
        size_t const rep = pids.front();
        PauliEquivClass eq;
        eq.members = pids;
        eq.rep     = rep;
        eq.kind    = classify_signature_class(inst, rep);
        reduction.classes.push_back(eq);
        reduction.kind_by_rep.emplace(rep, eq.kind);
        if (eq.kind == PauliClassKind::Sat) {
            reduction.sat_reps.push_back(rep);
        }
        for (size_t pid : pids) {
            reduction.pid_to_rep.emplace(pid, rep);
        }
    }

    auto const kind_order = [](PauliClassKind kind) {
        switch (kind) {
            case PauliClassKind::Fix0:
                return 0;
            case PauliClassKind::Sat:
                return 1;
            case PauliClassKind::FixG:
                return 2;
        }
        return 3;
    };
    std::sort(reduction.classes.begin(), reduction.classes.end(), [&](auto const& lhs, auto const& rhs) {
        if (kind_order(lhs.kind) != kind_order(rhs.kind)) {
            return kind_order(lhs.kind) < kind_order(rhs.kind);
        }
        return lhs.rep < rhs.rep;
    });
    std::sort(reduction.sat_reps.begin(), reduction.sat_reps.end());

    inst.reduction = std::move(reduction);
}

std::vector<size_t> distinct_reps_in_block(std::vector<size_t> const& pids,
                                           PauliColumnReduction const& red) {
    std::set<size_t> reps;
    for (size_t const pid : pids) {
        reps.insert(red.rep_for(pid));
    }
    return {reps.begin(), reps.end()};
}

z3::expr z3_nary(z3::context& ctx, std::vector<z3::expr> const& terms, bool const is_and) {
    if (terms.empty()) {
        return ctx.bool_val(is_and);
    }
    z3::expr_vector vec(ctx);
    for (auto const& term : terms) {
        vec.push_back(term);
    }
    return is_and ? z3::mk_and(vec) : z3::mk_or(vec);
}

z3::expr rep_gap_cmp(z3::context& ctx,
                     PauliColumnReduction const& red,
                     std::map<size_t, z3::expr> const& pos,
                     size_t const rep,
                     size_t const cut,
                     bool const lt) {
    if (auto const fixed = red.fixed_gap_for_rep(rep); fixed.has_value()) {
        return ctx.bool_val(lt ? *fixed < cut : *fixed > cut);
    }
    auto const it = pos.find(rep);
    if (it == pos.end()) {
        throw std::logic_error("missing Z3 pos variable for SAT Pauli class");
    }
    return lt ? it->second < static_cast<int>(cut) : it->second > static_cast<int>(cut);
}

std::unordered_map<size_t, size_t> build_rank_by_gid(std::vector<size_t> const& ordered_gids) {
    std::unordered_map<size_t, size_t> rank_by_gid;
    rank_by_gid.reserve(ordered_gids.size());
    for (size_t rank = 0; rank < ordered_gids.size(); ++rank) {
        rank_by_gid.emplace(ordered_gids[rank], rank);
    }
    return rank_by_gid;
}

bool clause_good_at_gap(AncillaSmtInstance const& inst,
                        PosMap const& pos_map,
                        std::unordered_map<size_t, size_t> const& rank_by_gid,
                        size_t const i,
                        size_t const gid) {
    size_t const rank = rank_by_gid.at(gid);
    auto const right_reps = distinct_reps_in_block(inst.block_right[gid], inst.reduction);
    auto const left_reps  = distinct_reps_in_block(inst.block_left[gid], inst.reduction);

    bool after_br = false;
    if (i > rank) {
        after_br = std::all_of(right_reps.begin(), right_reps.end(), [&](size_t const rep) {
            return pos_map.at(rep) < i;
        });
    }

    bool before_br = false;
    if (i <= rank) {
        before_br = std::all_of(left_reps.begin(), left_reps.end(), [&](size_t const rep) {
            return pos_map.at(rep) > i;
        });
    }

    return after_br || before_br;
}

struct WidthSolve {
    bool ok = false;
    PosMap pos_map;
};

WidthSolve solve_width(AncillaSmtInstance const& inst, size_t const w) {
    size_t const G = inst.gadget_order_gids.size();

    z3::context ctx;
    z3::solver solver(ctx);

    std::map<size_t, z3::expr> pos;
    for (size_t const rep : inst.reduction.sat_reps) {
        auto [it, inserted] = pos.emplace(rep, ctx.int_const(fmt::format("pos_class{}", rep).c_str()));
        (void)inserted;
        solver.add(it->second >= 0);
        solver.add(it->second <= static_cast<int>(G));
    }

    auto const rank_by_gid = build_rank_by_gid(inst.gadget_order_gids);

    std::vector<std::vector<z3::expr>> hard;
    hard.reserve(G + 1);
    for (size_t i = 0; i <= G; ++i) {
        hard.emplace_back();
        hard.back().reserve(G);
        for (size_t gid = 0; gid < G; ++gid) {
            size_t const rank = rank_by_gid.at(gid);
            auto const right_reps = distinct_reps_in_block(inst.block_right[gid], inst.reduction);
            auto const left_reps  = distinct_reps_in_block(inst.block_left[gid], inst.reduction);

            std::vector<z3::expr> right_cmp;
            right_cmp.reserve(right_reps.size());
            for (size_t const rep : right_reps) {
                right_cmp.push_back(rep_gap_cmp(ctx, inst.reduction, pos, rep, i, true));
            }
            std::vector<z3::expr> left_cmp;
            left_cmp.reserve(left_reps.size());
            for (size_t const rep : left_reps) {
                left_cmp.push_back(rep_gap_cmp(ctx, inst.reduction, pos, rep, i, false));
            }

            z3::expr const after_br  = z3_nary(ctx, right_cmp, true);
            z3::expr const before_br = z3_nary(ctx, left_cmp, true);
            z3::expr const good = (ctx.int_val(static_cast<int>(i)) > ctx.int_val(static_cast<int>(rank)) && after_br) ||
                                  (ctx.int_val(static_cast<int>(i)) <= ctx.int_val(static_cast<int>(rank)) && before_br);
            hard.back().push_back(z3::ite(good, ctx.int_val(0), ctx.int_val(1)));
        }

        z3::expr_vector gap_hard(ctx);
        for (auto const& h : hard.back()) {
            gap_hard.push_back(h);
        }
        solver.add(z3::sum(gap_hard) <= static_cast<int>(w));
    }

    for (size_t gl_gid = 0; gl_gid < G; ++gl_gid) {
        for (size_t gr_gid = gl_gid + 1; gr_gid < G; ++gr_gid) {
            size_t const rank_r = rank_by_gid.at(gr_gid);
            for (size_t i = 0; i <= G; ++i) {
                if (i > rank_r) {
                    solver.add(hard[i][gr_gid] >= hard[i][gl_gid]);
                }
            }
        }
    }

    if (solver.check() != z3::sat) {
        return {};
    }

    z3::model model = solver.get_model();
    WidthSolve out;
    out.ok = true;
    for (auto const& cls : inst.reduction.classes) {
        if (auto const fixed = inst.reduction.fixed_gap_for_rep(cls.rep); fixed.has_value()) {
            out.pos_map.emplace(cls.rep, *fixed);
            continue;
        }
        auto const it = pos.find(cls.rep);
        if (it == pos.end()) {
            throw std::logic_error("missing decoded pos variable for SAT Pauli class");
        }
        z3::expr const v = model.eval(it->second, true);
        out.pos_map.emplace(cls.rep, static_cast<size_t>(v.get_numeral_int()));
    }
    return out;
}

AncillaScheduleResult build_result(AncillaSmtInstance const& inst, size_t const width_w, PosMap const& pos_map) {
    size_t const G = inst.gadget_order_gids.size();
    auto const rank_by_gid = build_rank_by_gid(inst.gadget_order_gids);

    AncillaScheduleResult result;
    result.ok                = true;
    result.width_w           = width_w;
    result.gadget_order_gids = inst.gadget_order_gids;

    size_t const pid_lo = G;
    size_t const pid_hi = G + inst.pauli_count;
    for (size_t pid = pid_lo; pid < pid_hi; ++pid) {
        size_t const rep = inst.reduction.rep_for(pid);
        result.column_slot.emplace(pid, pos_map.at(rep));
    }

    for (size_t gid = 0; gid < G; ++gid) {
        bool any_bad = false;
        size_t min_i = std::numeric_limits<size_t>::max();
        size_t max_i = 0;
        for (size_t i = 0; i <= G; ++i) {
            bool const good = clause_good_at_gap(inst, pos_map, rank_by_gid, i, gid);
            if (good) {
                continue;
            }
            any_bad = true;
            min_i = std::min(min_i, i);
            max_i = std::max(max_i, i);
        }
        if (!any_bad) {
            result.degadgetizable_gids.insert(gid);
        } else {
            if (min_i == std::numeric_limits<size_t>::max()) {
                min_i = rank_by_gid.at(gid);
            }
            result.span_by_gid.emplace(gid, std::pair<size_t, size_t>{min_i, max_i});
        }
    }

    SatSignatureExport sig_for_overlap;
    sig_for_overlap.pauli_count = inst.pauli_count;
    sig_for_overlap.blocks_by_gid.resize(G);
    for (size_t gid = 0; gid < G; ++gid) {
        sig_for_overlap.blocks_by_gid[gid].gid         = gid;
        sig_for_overlap.blocks_by_gid[gid].block_left  = inst.block_left[gid];
        sig_for_overlap.blocks_by_gid[gid].block_right = inst.block_right[gid];
    }
    auto const overlap = compute_gadget_overlap_constraints(sig_for_overlap);
    for (size_t const gid :
         collect_lower_ancilla_overlap_excluded_gids(inst.gadget_ancilla_qubit, overlap)) {
        if (result.degadgetizable_gids.erase(gid) == 0) {
            continue;
        }
        if (result.span_by_gid.count(gid) == 0) {
            size_t const rank = rank_by_gid.at(gid);
            result.span_by_gid.emplace(gid, std::pair<size_t, size_t>{rank, rank});
        }
    }

    return result;
}

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
    (void)emit_start_by_rank;
    if (ord.degadgetizable_gids.count(gid) != 0) {
        return ccc_pos_by_rank[rank] + 1;
    }
    auto const it = ord.span_by_gid.find(gid);
    if (it == ord.span_by_gid.end()) {
        return std::nullopt;
    }
    size_t const j = it->second.second;
    size_t const G = ord.gadget_order_gids.size();
    if (j >= G) {
        return std::nullopt;
    }
    if (rank == j) {
        return ccc_pos_by_rank[rank] + 1;
    }
    return ccc_pos_by_rank[j];
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
    std::unordered_set<size_t> const& degadgetizable_gids,
    std::unordered_map<size_t, size_t> const* gid_to_ancilla = nullptr);

bool remap_middle_to_physical_ancillae(
    std::vector<SubTableau>& middle,
    std::unordered_map<size_t, size_t> const& logical_to_physical,
    size_t target_n_qubits,
    std::string& err);

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
    std::unordered_set<size_t> const&                     degadgetizable_gids,
    std::unordered_map<size_t, size_t> const*             gid_to_ancilla = nullptr) {
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
        degadgetizable_gids,
        gid_to_ancilla);
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

size_t compute_expected_reorder_count(size_t const orig_n_ancilla,
                                      size_t const sat_width_w,
                                      size_t const n_degadgetizable) {
    size_t const surviving =
        orig_n_ancilla > n_degadgetizable ? (orig_n_ancilla - n_degadgetizable) : 0;
    return surviving > sat_width_w ? (surviving - sat_width_w) : 0;
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
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
    std::unordered_map<size_t, size_t> const*                 gid_to_ancilla = nullptr) {
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
            if (gid_to_ancilla != nullptr) {
                auto const it = gid_to_ancilla->find(gid);
                if (it != gid_to_ancilla->end()) {
                    allowed_ancillas.insert(it->second);
                }
            } else {
                allowed_ancillas.insert(gadgets[gid].ancilla_qubit);
            }
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
    std::unordered_set<size_t> const&                       degadgetizable_gids,
    std::unordered_map<size_t, size_t> const*               gid_to_ancilla) {
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

        auto const allowed_ancillas = allowed_ancillas_at_gap_from_span(gap_i, ord, gadgets, gid_to_ancilla);

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

        if (cct->is_classical_control() && !cct->is_gadget()) {
            if (own_anc >= data_qubit_end && own_anc < ancilla_qubit_hi &&
                allowed_ancillas.count(own_anc) == 0) {
                bool own_early_pmc = false;
                for (size_t gid = 0; gid < gadgets.size(); ++gid) {
                    size_t const gid_anc =
                        gid_to_ancilla != nullptr && gid_to_ancilla->count(gid) != 0
                            ? gid_to_ancilla->at(gid)
                            : gadgets[gid].ancilla_qubit;
                    if (gid_anc != own_anc) {
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
                    if (span_it != ord.span_by_gid.end()) {
                        size_t const rank = rank_opt.value();
                        if (span_it->second.first == rank) {
                            own_early_pmc = true;
                        }
                        if (span_it->second.second == rank && gap_i == rank + 1) {
                            own_early_pmc = true;
                        }
                        if (span_it->second.second == G && gap_i == G) {
                            own_early_pmc = true;
                        }
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
            size_t const expected_anc =
                gid_to_ancilla != nullptr && gid_to_ancilla->count(expected_gid) != 0
                    ? gid_to_ancilla->at(expected_gid)
                    : gadgets[expected_gid].ancilla_qubit;
            if (cct->ancilla_qubit() != expected_anc) {
                spdlog::error(
                    "sat_reorder_apply: gadget order mismatch at middle[{}]: expected gid {} anc q{}, got anc q{}",
                    idx,
                    expected_gid,
                    expected_anc,
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
    if (G == 0 || intervals.empty()) {
        out = {};
        return true;
    }

    size_t const width_w = ord.has_width ? ord.width_w : ord.ancilla_count;
    if (width_w == 0) {
        err = "width is zero";
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
    out.gid_to_lane.reserve(intervals.size());
    out.gid_to_physical_ancilla.reserve(intervals.size());

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

void log_smt_gadget_spans(ParsedGadgetOrdering const& ord) {
    size_t const G = ord.gadget_order_gids.size();
    spdlog::info(
        "sat_reorder_apply: SMT gadget spans ({} gadgets, width={}):",
        G,
        ord.width_w);
    for (size_t gid = 0; gid < G; ++gid) {
        if (ord.degadgetizable_gids.count(gid) != 0) {
            spdlog::info("sat_reorder_apply:   g{}: degadgetizable", gid);
            continue;
        }
        auto const it = ord.span_by_gid.find(gid);
        if (it != ord.span_by_gid.end()) {
            spdlog::info(
                "sat_reorder_apply:   g{}: t=[{},{}]",
                gid,
                it->second.first,
                it->second.second);
        } else {
            spdlog::info("sat_reorder_apply:   g{}: (no span)", gid);
        }
    }
}

void log_ancilla_lane_remap_intervals(
    AncillaOccupancyTableau const&          occupancy,
    std::unordered_map<size_t, size_t> const& gid_to_current_ancilla) {
    if (occupancy.width_w == 0 || occupancy.occupied_gid_by_time_lane.empty()) {
        return;
    }
    size_t const G = occupancy.occupied_gid_by_time_lane.size() - 1;
    spdlog::info(
        "sat_reorder_apply: ancilla lane remap ({} lines q{}..q{}):",
        occupancy.width_w,
        occupancy.ancilla_base_qubit,
        occupancy.ancilla_base_qubit + occupancy.width_w - 1);

    for (size_t lane = 0; lane < occupancy.width_w; ++lane) {
        size_t const phys = occupancy.ancilla_base_qubit + lane;
        std::vector<std::string> segments;
        for (size_t t = 0; t <= G;) {
            int64_t gid = -1;
            if (lane < occupancy.occupied_gid_by_time_lane[t].size()) {
                gid = occupancy.occupied_gid_by_time_lane[t][lane];
            }
            if (gid < 0) {
                ++t;
                continue;
            }
            size_t const start = t;
            while (t <= G && lane < occupancy.occupied_gid_by_time_lane[t].size() &&
                   occupancy.occupied_gid_by_time_lane[t][lane] == gid) {
                ++t;
            }
            size_t const end = t - 1;
            size_t const gid_u = static_cast<size_t>(gid);
            auto const   anc_it = gid_to_current_ancilla.find(gid_u);
            size_t const old_anc =
                anc_it != gid_to_current_ancilla.end() ? anc_it->second : gid_u;
            segments.push_back(fmt::format("g{} q{}->q{} t=[{},{}]", gid_u, old_anc, phys, start, end));
        }
        if (!segments.empty()) {
            spdlog::info("sat_reorder_apply:   q{}: {}", phys, fmt::join(segments, ", "));
        }
    }
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

void remap_cct_qubits_preserve_type(ClassicalControlTableau&                  cct,
                                    std::unordered_map<size_t, size_t> const& logical_to_physical,
                                    size_t                                    target_n_qubits) {
    size_t const new_ancilla = remap_qubit(cct.ancilla_qubit(), logical_to_physical);
    size_t const new_ref     = remap_qubit(cct.reference_qubit(), logical_to_physical);
    CCTType const cct_type   = cct.is_gadget() ? CCTType::Gadget : CCTType::ClassicalControl;

    auto       ops_old = extract_clifford_operators(cct.operations());
    remap_clifford_ops_inplace(ops_old, logical_to_physical);

    ClassicalControlTableau rebuilt(new_ancilla, new_ref, target_n_qubits, cct_type);
    rebuilt.operations() = StabilizerTableau{target_n_qubits};
    rebuilt.operations().apply(ops_old);
    if (!cct.is_gadget()) {
        rebuilt.set_measurement_type(cct.measurement_type());
    }
    cct = std::move(rebuilt);
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
            remap_cct_qubits_preserve_type(*cct, logical_to_physical, target_n_qubits);
            continue;
        }
        err = fmt::format("middle[{}] has unsupported block type for ancilla remap", idx);
        return false;
    }
    return true;
}

}  // namespace

size_t PauliColumnReduction::rep_for(size_t const pid) const {
    auto const it = pid_to_rep.find(pid);
    if (it == pid_to_rep.end()) {
        throw std::out_of_range("unknown Pauli pid in reduction");
    }
    return it->second;
}

std::optional<size_t> PauliColumnReduction::fixed_gap_for_rep(size_t const rep) const {
    auto const it = kind_by_rep.find(rep);
    if (it == kind_by_rep.end()) {
        throw std::out_of_range("unknown Pauli class rep");
    }
    if (it->second == PauliClassKind::Fix0) {
        return 0;
    }
    if (it->second == PauliClassKind::FixG) {
        return G;
    }
    return std::nullopt;
}

AncillaSmtInstance build_ancilla_smt_instance(SatSignatureExport const& sig) {
    AncillaSmtInstance inst;
    inst.qubit_count        = sig.qubit_count;
    inst.ancilla_count      = sig.ancilla_count;
    inst.pauli_count        = sig.pauli_count;
    inst.gadget_order_gids  = sig.gadget_order;

    size_t const G = sig.blocks_by_gid.size();
    if (inst.gadget_order_gids.size() != G) {
        throw std::invalid_argument("SAT signature gadget_order size does not match block count");
    }

    inst.gadget_ancilla_qubit.assign(G, 0);
    inst.block_left.resize(G);
    inst.block_right.resize(G);

    std::unordered_set<size_t> seen_gids;
    for (size_t rank = 0; rank < inst.gadget_order_gids.size(); ++rank) {
        size_t const gid = inst.gadget_order_gids[rank];
        if (gid >= G || !seen_gids.insert(gid).second) {
            throw std::invalid_argument("SAT signature gadget_order is not a valid gid permutation");
        }
    }

    size_t const pid_lo = G;
    size_t const pid_hi = G + sig.pauli_count;
    for (size_t gid = 0; gid < G; ++gid) {
        auto const& lists = sig.blocks_by_gid[gid];
        if (lists.gid != gid) {
            throw std::invalid_argument("SAT signature blocks_by_gid is not indexed by gid");
        }
        inst.gadget_ancilla_qubit[gid] = lists.ancilla_qubit;
        inst.block_left[gid]           = lists.block_left;
        inst.block_right[gid]          = lists.block_right;

        for (auto* block : {&inst.block_left[gid], &inst.block_right[gid]}) {
            std::sort(block->begin(), block->end());
            block->erase(std::unique(block->begin(), block->end()), block->end());
            for (size_t const pid : *block) {
                if (pid < pid_lo || pid >= pid_hi) {
                    throw std::invalid_argument("SAT signature pid out of expected range");
                }
            }
        }
    }

    build_signature_reduction(inst);

    auto const overlap = compute_gadget_overlap_constraints(sig);
    inst.max_column_overlap = overlap.max_overlap_among_columns();

    return inst;
}

void finalize_parsed_gadget_ordering(ParsedGadgetOrdering& ord, std::string& err) {
    err.clear();
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

ParsedGadgetOrdering to_parsed_ordering(AncillaSmtInstance const& inst,
                                        AncillaScheduleResult const& result) {
    ParsedGadgetOrdering ord;
    ord.qubit_count                  = inst.qubit_count;
    ord.ancilla_count                = inst.ancilla_count;
    ord.width_w                      = result.width_w;
    ord.has_width                    = true;
    ord.gadget_order_gids            = result.gadget_order_gids;
    ord.column_slot                  = result.column_slot;
    ord.degadgetizable_gids          = result.degadgetizable_gids;
    ord.span_by_gid                  = result.span_by_gid;
    ord.has_degadgetizable_section   = true;

    std::string err;
    finalize_parsed_gadget_ordering(ord, err);
    if (!err.empty()) {
        throw std::runtime_error(err);
    }
    return ord;
}

AncillaScheduleResult solve_ancilla_schedule(AncillaSmtInstance const& inst) {
    size_t const G = inst.gadget_order_gids.size();
    if (G == 0) {
        return {.ok = false, .error = "empty gadget order"};
    }

    size_t const max_overlap = inst.max_column_overlap;
    size_t const hi_cap      = inst.ancilla_count;

    spdlog::info(
        "solve_ancilla_schedule: max_column_overlap={} (minimum achievable width)",
        max_overlap);

    auto log_width_result = [](size_t const w, bool const ok, char const* note = nullptr) {
        if (note != nullptr) {
            spdlog::info("solve_ancilla_schedule: width={} {} ({})", w, ok ? "SAT" : "UNSAT", note);
            return;
        }
        spdlog::info("solve_ancilla_schedule: width={} {}", w, ok ? "SAT" : "UNSAT");
    };

    std::optional<size_t> best_w;
    PosMap best_pos;

    size_t lo = 0;
    size_t hi = hi_cap;
    std::optional<WidthSolve> probe;

    if (max_overlap > 0) {
        size_t const probe_w = max_overlap - 1;
        probe = solve_width(inst, probe_w);
        log_width_result(probe_w, probe->ok, "max_overlap-1 probe");

        if (probe->ok) {
            spdlog::info(
                "solve_ancilla_schedule: max_overlap-1={} works (SAT); searching [1, {}]",
                probe_w,
                max_overlap - 1);
            best_w   = probe_w;
            best_pos = probe->pos_map;
            lo       = 1;
            hi       = max_overlap - 1;
        } else {
            spdlog::info(
                "solve_ancilla_schedule: max_overlap-1={} does not work (UNSAT); searching [{}, {}]",
                probe_w,
                max_overlap,
                hi_cap);
            lo = max_overlap;
            hi = hi_cap;
        }
    } else {
        spdlog::info(
            "solve_ancilla_schedule: max_column_overlap=0; searching [0, {}]",
            hi_cap);
        lo = 0;
        hi = hi_cap;
    }

    if (lo > hi) {
        if (best_w.has_value()) {
            return build_result(inst, *best_w, best_pos);
        }
        return {
            .ok    = false,
            .error = fmt::format(
                "SMT search range empty (max_column_overlap={} ancilla_count={})",
                max_overlap,
                hi_cap),
        };
    }

    while (lo <= hi) {
        size_t const mid = lo + (hi - lo) / 2;
        WidthSolve r;
        if (probe.has_value() && max_overlap > 0 && mid == max_overlap - 1) {
            r = *probe;
        } else {
            r = solve_width(inst, mid);
            log_width_result(mid, r.ok);
        }
        if (r.ok) {
            best_w   = mid;
            best_pos = std::move(r.pos_map);
            if (mid == 0) {
                break;
            }
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    if (!best_w.has_value()) {
        return {
            .ok    = false,
            .error = fmt::format("SMT UNSAT even at ancilla_count={}", hi_cap),
        };
    }
    return build_result(inst, *best_w, best_pos);
}

bool sat_reorder_apply_ordering(Tableau& tableau,
                                ParsedGadgetOrdering const& ord) {
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

    std::string place_err;
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
    size_t const sat_width_w      = ord.has_width ? ord.width_w : ord.ancilla_count;
    size_t const ancilla_hi       = ord.qubit_count > 0 ? ord.qubit_count : tableau.n_qubits();
    size_t const inner_start      = 1;
    size_t const schedule_end     = tableau.size() - 1;
    if (!permute_pmcs_on_tableau_for_sat_order(
            tableau, G, ord, gadgets, emit_start_by_rank, ccc_pos_by_rank)) {
        return false;
    }

    size_t const orig_n_ancilla = tableau.n_ancilla();
    size_t const expected_reorder_count = compute_expected_reorder_count(
        orig_n_ancilla,
        sat_width_w,
        ord.degadgetizable_gids.size());
    spdlog::info(
        "sat_reorder_apply: expected reorder count={} (orig_ancilla={} width={} degadgetizable={})",
        expected_reorder_count,
        orig_n_ancilla,
        sat_width_w,
        ord.degadgetizable_gids.size());
    log_smt_gadget_spans(ord);

    if (expected_reorder_count > 0) {
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
        spdlog::info("sat_reorder_apply: span check passed (gaps 0..{})", G);
    } else {
        spdlog::info("sat_reorder_apply: skipping span check (expected reorder count is 0)");
    }
    std::vector<size_t>      removed_ancillae;
    size_t                   degadgetized_count = 0;
    std::unordered_set<size_t> removed_gids;
    if (!ord.degadgetizable_gids.empty()) {
        std::vector<size_t> degadgetize_ancillae;
        degadgetize_ancillae.reserve(ord.degadgetizable_gids.size());
        for (size_t const gid : ord.degadgetizable_gids) {
            degadgetize_ancillae.push_back(gadgets[gid].ancilla_qubit);
        }
        degadgetized_count = hadamard_degadgetize(tableau, degadgetize_ancillae, &removed_ancillae);
        spdlog::info(
            "sat_reorder_apply: degadgetized {}/{} SMT-exported degadgetizable gadgets",
            degadgetized_count,
            degadgetize_ancillae.size());
        for (size_t const gid : ord.degadgetizable_gids) {
            if (std::find(removed_ancillae.begin(), removed_ancillae.end(), gadgets[gid].ancilla_qubit) !=
                removed_ancillae.end()) {
                removed_gids.insert(gid);
            }
        }
        if (!removed_gids.empty()) {
            std::vector<size_t> sorted_removed(removed_gids.begin(), removed_gids.end());
            std::sort(sorted_removed.begin(), sorted_removed.end());
            std::vector<std::string> removed_gid_entries;
            removed_gid_entries.reserve(sorted_removed.size());
            for (size_t const gid : sorted_removed) {
                removed_gid_entries.push_back(fmt::format("g{}", gid));
            }
            spdlog::info(
                "sat_reorder_apply: removed gids: {}",
                fmt::join(removed_gid_entries, ", "));
        }
    }

    size_t reorder_reduction = 0;
    if (expected_reorder_count > 0) {
        std::unordered_map<size_t, size_t> gid_to_current_ancilla;
        std::string                        ancilla_refresh_err;
        if (!build_post_degadgetize_gid_ancillae(
                gadgets,
                removed_gids,
                removed_ancillae,
                gid_to_current_ancilla,
                ancilla_refresh_err)) {
            spdlog::error("sat_reorder_apply: post-degadgetize ancilla refresh failed: {}", ancilla_refresh_err);
            return false;
        }

        size_t const data_qubits_post  = tableau.n_qubits() - tableau.n_ancilla();
        size_t const ancilla_base_post = data_qubits_post;
        size_t const schedule_end_post = tableau.size() - 1;

        std::vector<AncillaInterval> ancilla_intervals;
        std::string                  interval_err;
        if (!build_ancilla_intervals_from_span(
                ord,
                gadgets,
                removed_gids,
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
        log_ancilla_lane_remap_intervals(ancilla_occupancy, gid_to_current_ancilla);

        auto const logical_to_physical =
            build_logical_to_physical_ancilla_map(ancilla_intervals, ancilla_occupancy);
        size_t const actual_width_w       = ancilla_occupancy.width_w;
        size_t const target_n_qubits_post = data_qubits_post + actual_width_w;
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
        tableau.set_n_ancilla(actual_width_w);

    } else {
        spdlog::info("sat_reorder_apply: skipping ancilla remap (expected reorder count is 0)");
    }

    remove_identities(tableau);
    size_t const total_reduction =
        orig_n_ancilla >= tableau.n_ancilla() ? (orig_n_ancilla - tableau.n_ancilla()) : 0;
    size_t const degadgetize_reduction =
        std::min(total_reduction, degadgetized_count);
    reorder_reduction = total_reduction > degadgetize_reduction ? (total_reduction - degadgetize_reduction) : 0;
    spdlog::info("sat_reorder_apply: resulting width={}", tableau.n_ancilla());
    spdlog::info(
        "sat_reorder_apply: reduction by reorder count: {}, degadgetize count: {}",
        reorder_reduction,
        degadgetize_reduction);
    spdlog::info(
        "sat_reorder_apply: reduced {} ancilla ({} -> {})",
        total_reduction,
        orig_n_ancilla,
        tableau.n_ancilla());
    return true;
}

void sat_reorder(Tableau& tableau) {
    try {
        SatSignatureExport const sig = compute_sat_signature_blocks(tableau);
        log_sat_reorder_preprocess(sig);
        AncillaSmtInstance inst = build_ancilla_smt_instance(sig);
        AncillaScheduleResult const result = solve_ancilla_schedule(inst);
        if (!result.ok) {
            spdlog::error("sat_reorder: native SMT solve failed: {}", result.error);
            spdlog::info("sat_reorder_apply: failed");
            return;
        }
        ParsedGadgetOrdering const ord = to_parsed_ordering(inst, result);
        if (!sat_reorder_apply_ordering(tableau, ord)) {
            spdlog::info("sat_reorder_apply: failed");
        }
    } catch (std::exception const& e) {
        spdlog::error("sat_reorder: native SMT path threw: {}", e.what());
        spdlog::info("sat_reorder_apply: failed");
    }
    spdlog::info("sat_reorder: finished");
}

}  // namespace qsyn::experimental
