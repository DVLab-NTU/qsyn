/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ staq-style backward rotation folding. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./staq_fold.hpp"

#include <utility>
#include <vector>

#include "./rotation_merge.hpp"
#include "tableau/cpf/angle_utils.hpp"
#include "tableau/cpf/merge_log.hpp"
#include "tableau/tableau_optimization.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

namespace {

thread_local std::vector<int>* tls_entry_ids = nullptr;
thread_local int tls_pass_tag                = 0;

[[nodiscard]] std::string ops_between(std::vector<TimelineEntry> const& entries,
                                      std::size_t earlier_idx,
                                      std::size_t later_idx) {
    CliffordOperatorString ops;
    for (std::size_t k = earlier_idx + 1; k < later_idx; ++k) {
        if (auto const* st = std::get_if<StabilizerTableau>(&entries[k])) {
            auto const part = extract_clifford_operators(*st);
            ops.insert(ops.end(), part.begin(), part.end());
        }
    }
    return format_clifford_ops(ops);
}

}  // namespace

void merge_track_begin(Timeline& timeline, int pass_tag) {
    if (!merge_log_enabled()) {
        tls_entry_ids = nullptr;
        return;
    }
    // Owned by a static so it survives across staq + graph merge on the same
    // timeline within one dag_fold pass.
    static thread_local std::vector<int> ids;
    ids.assign(timeline.entries.size(), -1);
    int next = 0;
    for (std::size_t i = 0; i < timeline.entries.size(); ++i) {
        if (std::holds_alternative<PauliRotation>(timeline.entries[i])) {
            ids[i] = next++;
        }
    }
    tls_entry_ids = &ids;
    tls_pass_tag  = pass_tag;

    merge_log_line(fmt::format(
        "{{\"event\":\"track_begin\",\"pass\":{},\"n_entries\":{},\"n_rotations\":{}}}",
        pass_tag, timeline.entries.size(), next));

    // Collapse-frame labels: conjugate each rotation by the Clifford suffix
    // after it (same convention as `collapse`).
    std::vector<std::string> collapsed(timeline.entries.size());
    CliffordOperatorString suffix;
    for (std::size_t i = timeline.entries.size(); i-- > 0;) {
        if (auto const* st = std::get_if<StabilizerTableau>(&timeline.entries[i])) {
            StabilizerTableau tmp = *st;
            tmp.apply(suffix);
            suffix = extract_clifford_operators(tmp);
            continue;
        }
        if (auto const* r = std::get_if<PauliRotation>(&timeline.entries[i])) {
            PauliRotation rc = *r;
            rc.apply(suffix);
            collapsed[i] = rc.pauli_product().to_string('+');
        }
    }

    for (std::size_t i = 0; i < timeline.entries.size(); ++i) {
        if (auto const* r = std::get_if<PauliRotation>(&timeline.entries[i])) {
            merge_log_line(fmt::format(
                "{{\"event\":\"init\",\"pass\":{},\"id\":{},\"pauli\":\"{}\","
                "\"collapsed_pauli\":\"{}\",\"phase\":\"{}\"}}",
                pass_tag, ids[i], r->pauli_product().to_string('+'),
                collapsed[i], r->phase().get_print_string()));
        }
    }
}

void merge_track_end() {
    if (tls_entry_ids != nullptr) {
        merge_log_line(fmt::format("{{\"event\":\"track_end\",\"pass\":{}}}", tls_pass_tag));
    }
    tls_entry_ids = nullptr;
}

bool fold_rotation_at(std::vector<TimelineEntry>& entries, std::size_t index) {
    if (index >= entries.size()) return false;
    auto* cur = std::get_if<PauliRotation>(&entries[index]);
    if (cur == nullptr) return false;

    PauliRotation const src_at_site = *cur;
    int src_id =
        (tls_entry_ids != nullptr && index < tls_entry_ids->size()) ? (*tls_entry_ids)[index] : -1;

    PauliRotation moving = *cur;
    std::size_t pos      = index;
    bool merged          = false;

    CliffordOperatorString barrier_ops;

    while (pos > 0) {
        --pos;
        if (auto* st = std::get_if<StabilizerTableau>(&entries[pos])) {
            auto const part = extract_clifford_operators(*st);
            barrier_ops.insert(barrier_ops.end(), part.begin(), part.end());
            moving = commute_left(moving, *st);
            continue;
        }
        if (auto* earlier = std::get_if<PauliRotation>(&entries[pos])) {
            if (auto const m = try_merge_into_earlier(moving, *earlier)) {
                int const dst_id =
                    (tls_entry_ids != nullptr && pos < tls_entry_ids->size()) ? (*tls_entry_ids)[pos]
                                                                              : -1;
                auto const& p_l   = moving.pauli_product();
                auto const& p_e   = earlier->pauli_product();
                int sign          = 1;
                char const* klass = "same_pauli";
                if (p_l != p_e) {
                    sign  = -1;
                    klass = "propagation_negated";
                } else if (!barrier_ops.empty()) {
                    klass = "propagation_aligned";
                }

                if (merge_log_enabled()) {
                    merge_log_line(fmt::format(
                        "{{\"event\":\"merge\",\"pass\":{},\"kind\":\"staq\","
                        "\"class\":\"{}\",\"sign\":{},"
                        "\"src_id\":{},\"dst_id\":{},"
                        "\"src_pauli_at_site\":\"{}\",\"src_phase\":\"{}\","
                        "\"src_pauli_propagated\":\"{}\","
                        "\"dst_pauli\":\"{}\",\"dst_phase_before\":\"{}\","
                        "\"result_phase\":\"{}\","
                        "\"clifford_ops\":\"{}\"}}",
                        tls_pass_tag, klass, sign, src_id, dst_id,
                        src_at_site.pauli_product().to_string('+'),
                        src_at_site.phase().get_print_string(),
                        moving.pauli_product().to_string('+'),
                        earlier->pauli_product().to_string('+'),
                        earlier->phase().get_print_string(),
                        m->second.phase().get_print_string(),
                        format_clifford_ops(barrier_ops)));
                }

                *earlier = m->second;
                entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
                if (tls_entry_ids != nullptr) {
                    tls_entry_ids->erase(tls_entry_ids->begin() +
                                         static_cast<std::ptrdiff_t>(index));
                }
                merged = true;
                // Continue folding the merged rotation further left.
                moving = m->second;
                index  = pos;
                pos    = index;
                barrier_ops.clear();
                src_id = dst_id;  // further chained merges start from destination
                continue;
            }
            if (rotations_commute(moving, *earlier)) {
                continue;
            }
            break;
        }
    }

    if (!merged && is_zero_phase(moving.phase())) {
        if (merge_log_enabled()) {
            merge_log_line(fmt::format(
                "{{\"event\":\"drop_zero\",\"pass\":{},\"kind\":\"staq\","
                "\"src_id\":{},\"src_pauli_at_site\":\"{}\",\"src_phase\":\"{}\","
                "\"src_pauli_propagated\":\"{}\",\"clifford_ops\":\"{}\"}}",
                tls_pass_tag, src_id,
                src_at_site.pauli_product().to_string('+'),
                src_at_site.phase().get_print_string(),
                moving.pauli_product().to_string('+'),
                format_clifford_ops(barrier_ops)));
        }
        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
        if (tls_entry_ids != nullptr) {
            tls_entry_ids->erase(tls_entry_ids->begin() + static_cast<std::ptrdiff_t>(index));
        }
        merged = true;
    }

    return merged;
}

StaqFoldStats staq_fold_timeline(Timeline& timeline) {
    StaqFoldStats stats;
    stats.n_rotations_before = count_rotations(timeline);

    bool changed = true;
    while (changed) {
        changed = false;
        ++stats.n_passes;
        for (std::size_t i = 0; i < timeline.entries.size();) {
            if (!std::holds_alternative<PauliRotation>(timeline.entries[i])) {
                ++i;
                continue;
            }
            if (fold_rotation_at(timeline.entries, i)) {
                ++stats.n_merges;
                changed = true;
                if (i > 0) --i;
                continue;
            }
            ++i;
        }
        if (stats.n_passes >= 64) break;
    }

    // Drop any zero-phase rotations left behind.
    if (tls_entry_ids != nullptr) {
        std::vector<TimelineEntry> kept_e;
        std::vector<int> kept_i;
        kept_e.reserve(timeline.entries.size());
        kept_i.reserve(tls_entry_ids->size());
        for (std::size_t i = 0; i < timeline.entries.size(); ++i) {
            auto const* r = std::get_if<PauliRotation>(&timeline.entries[i]);
            if (r != nullptr && is_zero_phase(r->phase())) {
                if (merge_log_enabled()) {
                    merge_log_line(fmt::format(
                        "{{\"event\":\"drop_zero\",\"pass\":{},\"kind\":\"staq_cleanup\","
                        "\"src_id\":{},\"src_pauli_at_site\":\"{}\",\"src_phase\":\"{}\"}}",
                        tls_pass_tag, (*tls_entry_ids)[i],
                        r->pauli_product().to_string('+'), r->phase().get_print_string()));
                }
                continue;
            }
            kept_e.push_back(std::move(timeline.entries[i]));
            kept_i.push_back((*tls_entry_ids)[i]);
        }
        timeline.entries = std::move(kept_e);
        *tls_entry_ids   = std::move(kept_i);
    } else {
        timeline.entries.erase(
            std::remove_if(timeline.entries.begin(), timeline.entries.end(),
                           [](TimelineEntry const& e) {
                               auto const* r = std::get_if<PauliRotation>(&e);
                               return r != nullptr && is_zero_phase(r->phase());
                           }),
            timeline.entries.end());
    }

    stats.n_rotations_after = count_rotations(timeline);
    return stats;
}

}  // namespace qsyn::experimental::cpf::pauli_dag
