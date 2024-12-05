/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = PivotBoundaryRule::MatchType;

/**
 * @brief Find matchings of the pivot boundary rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @return std::vector<MatchType>
 */
std::vector<MatchType> PivotBoundaryRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    auto const match_boundary = [&](ZXVertex* v) {
        ZXVertex* vs = graph.get_first_neighbor(v).first;
        if (!candidates->contains(vs)) return;

        if (!vs->is_z()) {
            return;
        }

        // vs, vt should have only graph-like connections,
        // vs should connect to exactly one boundary vertex and graph-like
        // vt should be interiorly graph-like

        if (!is_graph_like_at(graph, vs->get_id())) return;
        if (std::ranges::count_if(
                graph.get_neighbors(vs) | std::views::keys,
                &ZXVertex::is_boundary) != 1) {
            return;
        }

        auto const it = std::ranges::find_if(
            graph.get_neighbors(vs),
            [&](auto const& nb_pair) {
                auto const& [nb, etype] = nb_pair;
                return candidates->contains(nb) &&
                       !nb->is_boundary() &&
                       nb->has_n_pi_phase() &&
                       !graph.has_dangling_neighbors(nb);
            });
        if (it == graph.get_neighbors(vs).end()) return;
        ZXVertex* vt = it->first;

        if (!is_interiorly_graph_like_at(graph, vt->get_id())) return;

        if (_allow_overlapping_candidates) return;

        candidates->erase(vs);
        candidates->erase(vt);

        for (auto& [nb, _] : graph.get_neighbors(vs)) candidates->erase(nb);
        for (auto& [nb, _] : graph.get_neighbors(vt)) candidates->erase(nb);
        matches.emplace_back(
            vs->get_id(), vt->get_id(),
            std::vector<size_t>{}, std::vector<size_t>{});
    };

    for (auto& v : graph.get_inputs()) match_boundary(v);
    for (auto& v : graph.get_outputs()) match_boundary(v);

    return matches;
}

bool PivotBoundaryRule::is_candidate(
    ZXGraph const& graph, ZXVertex* v0, ZXVertex* v1) const {
    if (!is_graph_like(graph)) {
        spdlog::error("The graph is not graph like!");
        return false;
    }
    if (!v0->is_z()) {
        spdlog::error("Vertex {} is not a Z vertex", v0->get_id());
        return false;
    }
    size_t has_boundary = 0;
    for (const auto& [nb, etype] : graph.get_neighbors(v0)) {
        if (nb->is_boundary()) {
            has_boundary++;
        }
    }
    if (has_boundary == 0) {
        spdlog::error(
            "Vertex {} is not connected to a boundary", v0->get_id());
        return false;
    }
    if (has_boundary > 1) {
        spdlog::error(
            "Vertex {} is connected to more than one boundaries", v0->get_id());
        return false;
    }
    if (!v1->has_n_pi_phase()) {
        spdlog::error(
            "Vertex {} is not a Z vertex with phase n π", v1->get_id());
        return false;
    }
    if (!graph.is_neighbor(v0, v1)) {
        spdlog::error(
            "Vertices {} and {} are not connected", v0->get_id(), v1->get_id());
        return false;
    }

    return true;
}
