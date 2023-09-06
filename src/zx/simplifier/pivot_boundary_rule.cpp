/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using MatchType = PivotBoundaryRule::MatchType;

std::vector<MatchType> PivotBoundaryRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;
    auto match_boundary = [&taken, &graph, &matches, this](ZXVertex* v) {
        ZXVertex* vs = v->get_first_neighbor().first;
        if (taken.contains(vs)) return;

        if (!vs->is_z()) {
            taken.insert(vs);
            return;
        }

        ZXVertex* vt = nullptr;
        for (auto& [nb, etype] : vs->get_neighbors()) {
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
        for (auto& [nb, etype] : vs->get_neighbors()) {
            if (nb->is_boundary()) {
                if (found_one) return;
                found_one = true;
                continue;
            }
            if (!nb->is_z() || etype != EdgeType::hadamard) return;
        }

        // check vt is only connected to Z-spider by H-edge
        for (auto& [nb, etype] : vt->get_neighbors()) {
            if (!nb->is_z() || etype != EdgeType::hadamard) return;
        }

        taken.insert(vs);
        taken.insert(vt);

        for (auto& [nb, _] : vs->get_neighbors()) taken.insert(nb);
        for (auto& [nb, _] : vt->get_neighbors()) taken.insert(nb);
        matches.emplace_back(vs, vt);
    };

    for (auto& v : graph.get_inputs()) match_boundary(v);
    for (auto& v : graph.get_outputs()) match_boundary(v);

    return matches;
}

void PivotBoundaryRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    for (auto& [vs, _] : matches) {
        for (auto& [nb, etype] : vs->get_neighbors()) {
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
