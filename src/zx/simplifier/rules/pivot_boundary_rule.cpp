/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = PivotBoundaryRule::MatchType;

std::vector<MatchType> PivotBoundaryRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;
    auto match_boundary = [&taken, &graph, &matches](ZXVertex* v) {
        ZXVertex* vs = graph.get_first_neighbor(v).first;
        if (taken.contains(vs)) return;

        if (!vs->is_z()) {
            taken.insert(vs);
            return;
        }

        ZXVertex* vt = nullptr;
        for (auto& [nb, etype] : graph.get_neighbors(vs)) {
            if (taken.contains(nb)) continue;  // do not choose the one in taken
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

        taken.insert(vs);
        taken.insert(vt);

        for (auto& [nb, _] : graph.get_neighbors(vs)) taken.insert(nb);
        for (auto& [nb, _] : graph.get_neighbors(vt)) taken.insert(nb);
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
                graph.add_buffer(nb, vs, etype);
                break;
            }
            if (!nb->is_z() || etype != EdgeType::hadamard) return;
        }
    }
    for (auto& [v0, v1] : matches) {
        if (!v0->has_n_pi_phase()) graph.gadgetize_phase(v0);
        if (!v1->has_n_pi_phase()) graph.gadgetize_phase(v1);
    }

    PivotRuleInterface::apply(graph, matches);
}

bool PivotBoundaryRule::is_candidate(ZXGraph& graph, ZXVertex* vb, ZXVertex* vn) {
    if (!graph.is_graph_like()) {
        spdlog::error("The graph is not graph like!");
        return false;
    }
    if (!vb->is_z()) {
        spdlog::error("Vertex {} is not a Z vertex", vb->get_id());
        return false;
    }
    size_t has_boundary = 0;
    for (const auto& [nb, etype] : graph.get_neighbors(vb)) {
        if (nb->is_boundary()) {
            has_boundary++;
        }
    }
    if (has_boundary == 0) {
        spdlog::error("Vertex {} is not connected to a boundary", vb->get_id());
        return false;
    }
    if (has_boundary > 1) {
        spdlog::error("Vertex {} is connected to more than one boundaries", vb->get_id());
        return false;
    }
    if (!vn->has_n_pi_phase()) {
        spdlog::error("Vertex {} is not a Z vertex with phase n π", vn->get_id());
        return false;
    }
    if (!graph.is_neighbor(vb, vn)) {
        spdlog::error("Vertices {} and {} are not connected", vb->get_id(), vn->get_id());
        return false;
    }
    // if (graph.has_dangling_neighbors(vn)) {
    //     spdlog::error("Vertex {} is the axel of a phase gadget", vn->get_id());
    //     return false;
    // }
    return true;
}
