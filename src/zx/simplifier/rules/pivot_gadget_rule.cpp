/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = PivotGadgetRule::MatchType;

std::vector<MatchType> PivotGadgetRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;

    graph.for_each_edge([&graph, &taken, &matches](EdgePair const& epair) {
        if (epair.second != EdgeType::hadamard) return;

        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if (taken.contains(vs) || taken.contains(vt)) return;

        if (!vs->is_z()) {
            taken.insert(vs);
            return;
        }
        if (!vt->is_z()) {
            taken.insert(vt);
            return;
        }

        auto const vs_is_n_pi = (vs->get_phase().denominator() == 1);
        auto const vt_is_n_pi = (vt->get_phase().denominator() == 1);

        // if both n*pi --> ordinary pivot rules
        // if both not, --> maybe pivot double-boundary
        if (vs_is_n_pi == vt_is_n_pi) return;

        if (!vs_is_n_pi && vt_is_n_pi) std::swap(vs, vt);  // if vs is not n*pi but vt is, should extract vs as gadget instead

        // REVIEW - check ground conditions

        if (graph.get_num_neighbors(vt) == 1) {  // early return: (vs, vt) is a phase gadget
            taken.insert(vs);
            taken.insert(vt);
            return;
        }

        for (const auto& [v, _] : graph.get_neighbors(vs)) {
            if (!v->is_z()) return;                 // vs is not internal or not graph-like
            if (graph.get_num_neighbors(v) == 1) {  // (vs, v) is a phase gadget
                taken.insert(vs);
                taken.insert(v);
                return;
            }
        }
        for (const auto& [v, _] : graph.get_neighbors(vt)) {
            if (!v->is_z()) return;  // vt is not internal or not graph-like
        }

        // Both vs and vt are interior vertices
        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : graph.get_neighbors(vs)) taken.insert(v);
        for (auto& [v, _] : graph.get_neighbors(vt)) taken.insert(v);

        matches.emplace_back(vs, vt);
    });

    return matches;
}

void PivotGadgetRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    for (auto& [_, v] : matches) {
        // REVIEW - scalar add power
        if (v->get_phase().denominator() != 1) {
            graph.gadgetize_phase(v);
        }
    }

    PivotRuleInterface::apply(graph, matches);
}
