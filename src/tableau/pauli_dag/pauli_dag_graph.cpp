/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Pauli DAG construction and merge-edge folding. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pauli_dag_graph.hpp"

#include <algorithm>
#include <ranges>

#include "./rotation_merge.hpp"
#include "./staq_fold.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

namespace {

[[nodiscard]] PauliRotation rotation_in_frame(PauliRotation const& r,
                                            StabilizerTableau const& prefix) {
    return commute_left(r, prefix);
}

[[nodiscard]] bool can_reach_merge(PauliRotation const& later, PauliRotation const& earlier,
                                 StabilizerTableau const& between) {
    auto moving = commute_left(later, between);
    return try_merge_into_earlier(moving, earlier).has_value();
}

}  // namespace

PauliDagGraph build_pauli_dag_graph(Timeline const& timeline) {
    PauliDagGraph graph;
    graph.n_qubits = timeline.n_qubits;

    StabilizerTableau prefix{timeline.n_qubits};

    for (std::size_t t = 0; t < timeline.entries.size(); ++t) {
        if (auto const* st = std::get_if<StabilizerTableau>(&timeline.entries[t])) {
            prefix.apply(extract_clifford_operators(*st));
            continue;
        }
        auto const* r = std::get_if<PauliRotation>(&timeline.entries[t]);
        if (r == nullptr) continue;

        PauliDagNode node;
        node.timeline_index = t;
        node.rotation       = rotation_in_frame(*r, prefix);
        node.label_key      = node.rotation.pauli_product().to_bit_string();
        graph.nodes.push_back(std::move(node));
    }

    auto const n = graph.nodes.size();
    // For a fixed `i`, the Clifford block `between(i, j)` (the Cliffords strictly
    // between node i and node j) only grows as j increases:
    //     between(i, j+1) = between(i, j) . [Cliffords in (t_j, t_{j+1})].
    // So we accumulate it incrementally instead of rebuilding it from scratch
    // for every pair. This turns the O(n^3) all-pairs scan into O(n^2) while
    // producing byte-for-byte identical graphs.
    for (std::size_t i = 0; i < n; ++i) {
        StabilizerTableau between{timeline.n_qubits};
        std::size_t pos = graph.nodes[i].timeline_index + 1;  // next timeline entry to fold into `between`
        for (std::size_t j = i + 1; j < n; ++j) {
            auto const tj = graph.nodes[j].timeline_index;
            for (; pos < tj; ++pos) {
                if (auto const* st = std::get_if<StabilizerTableau>(&timeline.entries[pos])) {
                    between.apply(extract_clifford_operators(*st));
                }
            }
            // `pos == tj` here (a rotation entry, never a Clifford), so the next
            // iteration resumes accumulation right after node j.

            if (can_reach_merge(graph.nodes[j].rotation, graph.nodes[i].rotation, between)) {
                graph.merge_edges.emplace_back(i, j);
            }
            // NOTE: `dependency_edges` (ordered non-commuting pairs) are never
            // consumed by the merge passes (graph_component_merge /
            // max_matching_merge only read `merge_edges`), so we skip the
            // expensive per-pair `ordered_commute` conjugation entirely.
        }
    }

    return graph;
}

std::size_t graph_component_merge(Timeline& timeline, PauliDagGraph const& graph) {
    if (graph.merge_edges.empty()) return 0;

    // Apply each merge edge by staq backward-fold on the later gate. Process
    // later timeline indices first so erasures do not invalidate indices.
    auto edges = graph.merge_edges;
    std::ranges::sort(edges, [&](auto const& a, auto const& b) {
        return graph.nodes[a.second].timeline_index > graph.nodes[b.second].timeline_index;
    });

    std::size_t applied = 0;
    for (auto const& [i, j] : edges) {
        (void)i;
        auto const tj = graph.nodes[j].timeline_index;
        if (tj < timeline.entries.size() &&
            fold_rotation_at(timeline.entries, tj)) {
            ++applied;
        }
    }
    return applied;
}

namespace {

[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>>
select_max_matching_edges(PauliDagGraph const& graph) {
    auto const& edges = graph.merge_edges;
    auto const  n     = edges.size();
    if (n == 0) return {};

    auto try_mask = [&](unsigned long long mask) -> std::optional<std::vector<std::pair<std::size_t, std::size_t>>> {
        std::vector<bool> used(graph.nodes.size(), false);
        std::vector<std::pair<std::size_t, std::size_t>> picked;
        picked.reserve(n);
        for (std::size_t e = 0; e < n; ++e) {
            if ((mask & (1ULL << e)) == 0) continue;
            auto const [a, b] = edges[e];
            if (used[a] || used[b]) return std::nullopt;
            used[a] = used[b] = true;
            picked.emplace_back(a, b);
        }
        return picked;
    };

    if (n <= 24) {
        std::vector<std::pair<std::size_t, std::size_t>> best;
        for (unsigned long long mask = 0; mask < (1ULL << n); ++mask) {
            if (auto picked = try_mask(mask); picked.has_value() && picked->size() > best.size()) {
                best = std::move(*picked);
            }
        }
        return best;
    }

    // Greedy fallback: prefer later timeline indices (larger j index).
    auto sorted = edges;
    std::ranges::sort(sorted, [&](auto const& x, auto const& y) {
        return graph.nodes[x.second].timeline_index > graph.nodes[y.second].timeline_index;
    });
    std::vector<bool> used(graph.nodes.size(), false);
    std::vector<std::pair<std::size_t, std::size_t>> picked;
    for (auto const& e : sorted) {
        auto const [a, b] = e;
        if (used[a] || used[b]) continue;
        used[a] = used[b] = true;
        picked.push_back(e);
    }
    return picked;
}

}  // namespace

std::size_t max_matching_merge(Timeline& timeline, PauliDagGraph const& graph) {
    auto const chosen = select_max_matching_edges(graph);
    if (chosen.empty()) return 0;

    std::vector<std::size_t> timeline_indices;
    timeline_indices.reserve(chosen.size());
    for (auto const& [i, j] : chosen) {
        (void)i;
        timeline_indices.push_back(graph.nodes[j].timeline_index);
    }
    std::ranges::sort(timeline_indices, std::ranges::greater{});

    std::size_t applied = 0;
    for (auto const tj : timeline_indices) {
        if (tj < timeline.entries.size() && fold_rotation_at(timeline.entries, tj)) {
            ++applied;
        }
    }
    return applied;
}

}  // namespace qsyn::experimental::cpf::pauli_dag
