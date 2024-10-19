#include "./rule-matchers.hpp"

#include <ranges>

namespace qsyn::zx::simplify {

std::vector<IdentityFusionMatcher::MatchType>
IdentityFusionMatcher::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    auto matches = std::vector<MatchType>{};

    for (auto const& v : *candidates) {
        if (!IdentityFusion::is_applicable(graph, v->get_id())) {
            continue;
        }
        matches.emplace_back(v->get_id());
    }

    return matches;
}

namespace {

std::optional<std::vector<size_t>> get_boundary_ids_if_valid(
    ZXGraph const& graph, ZXVertex* v) {
    std::vector<size_t> boundary_ids;

    for (auto const& [nb, etype] : graph.get_neighbors(v)) {
        if (nb->is_boundary()) {
            boundary_ids.emplace_back(nb->get_id());
        } else if (nb->is_z() && etype == EdgeType::hadamard) {
            continue;
        } else {
            return std::nullopt;
        }
    }

    std::ranges::sort(boundary_ids);
    return boundary_ids;
}

tl::generator<std::vector<size_t>> unfuse_combinations(
    std::vector<size_t> const& neighbor_ids, size_t max_unfusions) {
    for (auto subset_size : std::views::iota(0ul, max_unfusions + 1)) {
        for (auto&& neighbors_to_unfuse :
             dvlab::combinations(neighbor_ids, subset_size)) {
            co_yield neighbors_to_unfuse;
        }
    }
}

}  // namespace

std::vector<LCompUnfusionMatcher::MatchType>
LCompUnfusionMatcher::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    auto matches = std::vector<MatchType>{};

    for (auto const& v : *candidates) {
        if (!v->is_z() || graph.num_neighbors(v) == 1) {
            continue;
        }

        auto boundary_ids = get_boundary_ids_if_valid(graph, v);
        if (!boundary_ids.has_value()) {
            continue;
        }

        // Keep at least two neighbors unfused to avoid indefinite loops.
        // Example: Unfusing all neighbors except n0 can lead to:
        //
        //  (n1)                        (n1)          (1) this lcomp gives (2)
        //  . \               unfuse    .  \           *
        //  .  (pi/2)--(n0)     ->      .  ( )--( )--(pi/2)--(n0)
        //  . /                         . /      *
        // (nk)                        (nk)     (2) this lcomp gives
        //                                          the original graph

        auto const max_unfusions =
            std::min(graph.num_neighbors(v) - 2,
                     num_max_unfusions());

        auto const neighbor_ids = graph.get_neighbor_ids(v);

        for (auto&& neighbors_to_unfuse :
             unfuse_combinations(neighbor_ids, max_unfusions)) {
            // both vector is sorted, can directly call std::ranges::includes
            if (!std::ranges::includes(neighbors_to_unfuse, *boundary_ids)) {
                continue;
            }

            if (neighbors_to_unfuse.empty() &&
                v->phase().denominator() != 2) {
                continue;
            }

            matches.emplace_back(v->get_id(), neighbors_to_unfuse);
        }
    }

    return matches;
}

std::vector<PivotUnfusionMatcher::MatchType>
PivotUnfusionMatcher::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    auto matches = std::vector<MatchType>{};

    graph.for_each_edge(*candidates, [&](EdgePair const& epair) {
        if (epair.second != EdgeType::hadamard) return;

        auto [v1, v2] = epair.first;
        if (!v1->is_z() || !v2->is_z()) return;
        if (!v1->has_n_pi_phase() || !v2->has_n_pi_phase()) return;

        if (graph.num_neighbors(v1) == 1 || graph.num_neighbors(v2) == 1) {
            return;
        }

        // Keep at least two neighbors unfused to avoid indefinite loops.
        // This is similar to the case in LCompUnfusionMatcher.

        auto const boundary_ids_1 = get_boundary_ids_if_valid(graph, v1);
        auto const boundary_ids_2 = get_boundary_ids_if_valid(graph, v2);

        if (!boundary_ids_1.has_value() || !boundary_ids_2.has_value())
            return;

        auto const max_unfusions_1 =
            std::min(graph.num_neighbors(v1) - 2,
                     num_max_unfusions());
        auto const max_unfusions_2 =
            std::min(graph.num_neighbors(v2) - 2,
                     num_max_unfusions());

        for (auto&& neighbors_to_unfuse_1 :
             unfuse_combinations(*boundary_ids_1, max_unfusions_1)) {
            if (dvlab::contains(neighbors_to_unfuse_1, v2->get_id())) {
                continue;
            }
            if (!std::ranges::includes(
                    neighbors_to_unfuse_1, *boundary_ids_1)) {
                continue;
            }

            if (neighbors_to_unfuse_1.empty() &&
                v1->phase().denominator() != 1) {
                continue;
            }

            for (auto&& neighbors_to_unfuse_2 :
                 unfuse_combinations(*boundary_ids_2, max_unfusions_2)) {
                if (dvlab::contains(neighbors_to_unfuse_2, v1->get_id())) {
                    continue;
                }
                if (!std::ranges::includes(
                        neighbors_to_unfuse_2, *boundary_ids_2)) {
                    continue;
                }

                if (neighbors_to_unfuse_2.empty() &&
                    v2->phase().denominator() != 1) {
                    continue;
                }

                matches.emplace_back(v1->get_id(), v2->get_id(),
                                     neighbors_to_unfuse_1,
                                     neighbors_to_unfuse_2);
            }
        }
    });

    return matches;
}

}  // namespace qsyn::zx::simplify
