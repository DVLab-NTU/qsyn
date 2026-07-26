#include "ncf/ncf_analysis.hpp"

#include <algorithm>
#include <map>
#include <numeric>

#include "ncf/ncf_block.hpp"
#include "ncf/ncf_program.hpp"

namespace qsyn::experimental {

namespace {

bool blocks_pauli_commute(NcfBlock const* a, NcfBlock const* b) {
    if (!a || !b) return false;
    for (auto const& pa : a->pauli_before()) {
        for (auto const& pb : b->pauli_before()) {
            if (!pa.is_commutative(pb)) return false;
        }
    }
    return true;
}

using CxKey = std::pair<size_t, size_t>;

std::map<CxKey, size_t> cx_edge_counts(std::vector<NcfCxEdge> const& edges) {
    std::map<CxKey, size_t> counts;
    for (auto const& e : edges) {
        counts[{e.control, e.target}]++;
    }
    return counts;
}

size_t count_matching_cx(std::vector<NcfCxEdge> const& left, std::vector<NcfCxEdge> const& right) {
    auto const lc = cx_edge_counts(left);
    auto const rc = cx_edge_counts(right);
    size_t matched = 0;
    for (auto const& [key, lcnt] : lc) {
        auto it = rc.find(key);
        if (it != rc.end()) matched += std::min(lcnt, it->second);
    }
    return matched;
}

void append_cancel_pairs(std::vector<NcfCxEdge> const& suffix, std::vector<NcfCxEdge> const& prefix,
                         std::vector<std::pair<NcfCxEdge, NcfCxEdge>>& out) {
    auto suffix_avail = suffix;
    auto prefix_avail = prefix;
    for (auto& e_l : suffix_avail) {
        for (auto& e_r : prefix_avail) {
            if (e_l.control == e_r.control && e_l.target == e_r.target) {
                out.emplace_back(e_l, e_r);
                e_r.control = e_r.target = SIZE_MAX;
                break;
            }
        }
    }
}

size_t gross_shell_cx(NcfProgram const& program, std::vector<size_t> const& order) {
    size_t total = 0;
    for (size_t id : order) {
        if (auto const* b = program.block(id)) total += b->stats().cx_total;
    }
    return total;
}

}  // namespace

NcfJunction analyze_junction(NcfBlock const& left, NcfBlock const& right) {
    NcfJunction j;
    j.left  = left.id();
    j.right = right.id();

    auto parity_interface = left.c_forward().parity();
    parity_interface.prepend(right.c_dagger().ops());
    j.exactly_identity = parity_interface.is_identity();

    j.cx_count           = left.stats().cx_suffix + right.stats().cx_prefix;
    j.cx_saved_if_merged = j.exactly_identity ? j.cx_count : 0;

    auto const& suffix_edges = left.c_forward().cx_graph().edges;
    auto const& prefix_edges = right.c_dagger().cx_graph().edges;
    append_cancel_pairs(suffix_edges, prefix_edges, j.cancel_pairs);

    if (!j.exactly_identity) {
        size_t const matched = count_matching_cx(suffix_edges, prefix_edges);
        j.partial_cx_saved   = matched * 2;
    }

    if (j.cx_count > 0) {
        j.structural_overlap =
            static_cast<double>(j.cx_saved_if_merged + j.partial_cx_saved) / static_cast<double>(j.cx_count);
    }
    return j;
}

std::vector<NcfJunction> junctions_along_order(NcfProgram const& program, std::vector<size_t> const& order) {
    std::vector<NcfJunction> out;
    for (size_t k = 0; k + 1 < order.size(); ++k) {
        auto const* left  = program.block(order[k]);
        auto const* right = program.block(order[k + 1]);
        if (!left || !right) continue;
        out.push_back(analyze_junction(*left, *right));
    }
    return out;
}

size_t seam_cx_saved(NcfProgram const& program, std::vector<size_t> const& order) {
    size_t saved = 0;
    for (auto const& j : junctions_along_order(program, order)) {
        saved += j.cx_saved_if_merged + j.partial_cx_saved;
    }
    return saved;
}

size_t predicted_shell_cx(NcfProgram const& program, std::vector<size_t> const& order) {
    auto gross = gross_shell_cx(program, order);
    auto saved = seam_cx_saved(program, order);
    return gross > saved ? gross - saved : 0;
}

bool reorder_commute_valid(NcfProgram const& program, NcfCommuteGraph const& cg, std::vector<size_t> const& order) {
    if (order.size() != program.n_blocks()) return false;
    auto pos_in = [&](size_t id) {
        auto it = std::find(program.order().begin(), program.order().end(), id);
        return static_cast<size_t>(std::distance(program.order().begin(), it));
    };
    for (size_t i = 0; i < order.size(); ++i) {
        for (size_t j = i + 1; j < order.size(); ++j) {
            if (!cg.commute_pauli(order[i], order[j]) && pos_in(order[i]) > pos_in(order[j])) {
                return false;
            }
        }
    }
    return true;
}

NcfCommuteGraph::NcfCommuteGraph(NcfProgram const& program) {
    _n = program.n_blocks();
    _commute.assign(_n, std::vector<bool>(_n, false));
    for (size_t i = 0; i < _n; ++i) {
        _commute[i][i] = true;
        for (size_t j = i + 1; j < _n; ++j) {
            bool c = blocks_pauli_commute(program.block(i), program.block(j));
            _commute[i][j] = _commute[j][i] = c;
        }
    }
}

bool NcfCommuteGraph::commute_pauli(size_t a, size_t b) const {
    if (a >= _n || b >= _n) return false;
    return _commute[a][b];
}

std::vector<std::vector<size_t>> NcfCommuteGraph::layers() const {
    std::vector<std::vector<size_t>> layers;
    std::vector<size_t> order(_n);
    std::iota(order.begin(), order.end(), 0);
    size_t i = 0;
    while (i < _n) {
        std::vector<size_t> layer{order[i]};
        size_t j = i + 1;
        while (j < _n) {
            bool ok = true;
            for (size_t k : layer) {
                if (!_commute[order[j]][k]) {
                    ok = false;
                    break;
                }
            }
            if (!ok) break;
            layer.push_back(order[j]);
            ++j;
        }
        layers.push_back(std::move(layer));
        i = j;
    }
    return layers;
}

std::vector<std::vector<bool>> NcfCommuteGraph::matrix() const { return _commute; }

NcfCommuteGraph const& NcfAnalysisCache::commute_graph(NcfProgram const& program) {
    if (!_commute) _commute = std::make_unique<NcfCommuteGraph>(program);
    return *_commute;
}

std::vector<NcfJunction> const& NcfAnalysisCache::junctions(NcfProgram const& program) {
    if (!_junctions) {
        _junctions = std::make_unique<std::vector<NcfJunction>>();
        *_junctions = junctions_along_order(program, program.order());
    }
    return *_junctions;
}

std::vector<NcfScheduleCandidate> const& NcfAnalysisCache::schedules(NcfProgram const& program, std::string const& mode) {
    if (!_schedules || _schedule_mode != mode) {
        _schedule_mode = mode;
        _schedules     = std::make_unique<std::vector<NcfScheduleCandidate>>();
        auto const& cg = commute_graph(program);

        auto make_candidate = [&](std::string const& name, std::vector<size_t> order) {
            NcfScheduleCandidate c;
            c.mode           = name;
            c.order          = std::move(order);
            c.seam_cx_saved  = seam_cx_saved(program, c.order);
            c.predicted_cx   = predicted_shell_cx(program, c.order);
            c.commute_valid  = reorder_commute_valid(program, cg, c.order);
            return c;
        };

        _schedules->push_back(make_candidate("naive", program.order()));

        if (mode == "commute" || mode == "all" || mode == "min-cx") {
            auto c = make_candidate("commute", {});
            for (auto const& layer : cg.layers()) {
                for (size_t id : layer) c.order.push_back(id);
            }
            c.seam_cx_saved = seam_cx_saved(program, c.order);
            c.predicted_cx  = predicted_shell_cx(program, c.order);
            _schedules->push_back(std::move(c));
        }

        if (mode == "min-cx" || mode == "all") {
            std::vector<size_t> ids = program.order();
            std::sort(ids.begin(), ids.end());
            NcfScheduleCandidate best;
            best.mode          = "min-cx";
            best.predicted_cx  = SIZE_MAX;
            best.commute_valid = true;
            do {
                if (!reorder_commute_valid(program, cg, ids)) continue;
                auto const saved = seam_cx_saved(program, ids);
                auto const pred  = predicted_shell_cx(program, ids);
                if (pred < best.predicted_cx) {
                    best.order         = ids;
                    best.predicted_cx  = pred;
                    best.seam_cx_saved = saved;
                }
            } while (std::next_permutation(ids.begin(), ids.end()));
            if (best.predicted_cx != SIZE_MAX) _schedules->push_back(std::move(best));
        }
    }
    return *_schedules;
}

}  // namespace qsyn::experimental
