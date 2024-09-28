/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;

using MatchType = PivotBoundaryRule::MatchType;

/**
 * @brief Find matchings of the pivot boundary rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> PivotBoundaryRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates,
    bool allow_overlapping_candidates  //
) const {
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

        ZXVertex* vt = nullptr;
        for (auto& [nb, etype] : graph.get_neighbors(vs)) {
            if (!candidates->contains(nb)) continue;
            if (nb->is_boundary()) continue;
            if (!nb->has_n_pi_phase()) continue;
            if (etype != EdgeType::hadamard) continue;
            if (graph.has_dangling_neighbors(nb)) continue;  // nb is the axel of a phase gadget
            vt = nb;
            break;
        }
        if (vt == nullptr) return;

        bool found_one = false;
        // check vs is only connected to boundary, or connected to Z-spider by H-edge
        for (auto& [nb, etype] : graph.get_neighbors(vs)) {
            if (nb->is_boundary()) {
                if (found_one) return;
                found_one = true;
                continue;
            }
            if (!nb->is_z() || etype != EdgeType::hadamard) return;
        }

        // check vt is only connected to Z-spider by H-edge
        for (auto& [nb, etype] : graph.get_neighbors(vt)) {
            if (!nb->is_z() || etype != EdgeType::hadamard) return;
        }

        if (allow_overlapping_candidates) return;

        candidates->erase(vs);
        candidates->erase(vt);

        for (auto& [nb, _] : graph.get_neighbors(vs)) candidates->erase(nb);
        for (auto& [nb, _] : graph.get_neighbors(vt)) candidates->erase(nb);
        matches.emplace_back(vs, vt);
    };

    for (auto& v : graph.get_inputs()) match_boundary(v);
    for (auto& v : graph.get_outputs()) match_boundary(v);

    return matches;
}

void PivotBoundaryRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    for (auto& [vs, _] : matches) {
        for (auto& [nb, etype] : graph.get_neighbors(vs)) {
            if (nb->is_boundary()) {
                zx::add_identity_vertex(
                    graph, vs->get_id(), nb->get_id(), EdgeType::hadamard);
                break;
            }
            if (!nb->is_z() || etype != EdgeType::hadamard) return;
        }
    }
    for (auto& [v0, v1] : matches) {
        if (!v0->has_n_pi_phase()) {
            gadgetize_phase(graph, v0->get_id());
        }
        if (!v1->has_n_pi_phase()) {
            gadgetize_phase(graph, v1->get_id());
        }
    }

    PivotRuleInterface::apply(graph, matches);
}

bool PivotBoundaryRule::is_candidate(ZXGraph& graph, ZXVertex* v0, ZXVertex* v1) {
    if (!graph.is_graph_like()) {
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
        spdlog::error("Vertex {} is not connected to a boundary", v0->get_id());
        return false;
    }
    if (has_boundary > 1) {
        spdlog::error("Vertex {} is connected to more than one boundaries", v0->get_id());
        return false;
    }
    if (!v1->has_n_pi_phase()) {
        spdlog::error("Vertex {} is not a Z vertex with phase n π", v1->get_id());
        return false;
    }
    if (!graph.is_neighbor(v0, v1)) {
        spdlog::error("Vertices {} and {} are not connected", v0->get_id(), v1->get_id());
        return false;
    }
    // if (graph.has_dangling_neighbors(vn)) {
    //     spdlog::error("Vertex {} is the axel of a phase gadget", vn->get_id());
    //     return false;
    // }
    return true;
}
