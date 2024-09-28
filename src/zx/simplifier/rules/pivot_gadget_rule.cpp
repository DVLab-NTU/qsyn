/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zxgraph_action.hpp"

using namespace qsyn::zx;

using MatchType = PivotGadgetRule::MatchType;

/**
 * @brief Find matchings of the pivot gadget rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @param allow_overlapping_candidates whether to allow overlapping candidates. If true, needs to manually check for overlapping candidates.
 * @return std::vector<MatchType>
 */
std::vector<MatchType> PivotGadgetRule::find_matches(
    ZXGraph const& graph,
    std::optional<ZXVertexList> candidates,
    bool allow_overlapping_candidates  //
) const {
    std::vector<MatchType> matches;

    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    graph.for_each_edge([&](EdgePair const& epair) {
        if (epair.second != EdgeType::hadamard) return;

        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if (!candidates->contains(vs) || !candidates->contains(vt)) return;

        if (!vs->is_z() || !vt->is_z()) {
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
            return;
        }

        for (const auto& [v, _] : graph.get_neighbors(vs)) {
            if (!v->is_z()) return;                 // vs is not internal or not graph-like
            if (graph.get_num_neighbors(v) == 1) {  // (vs, v) is a phase gadget
                return;
            }
        }
        for (const auto& [v, _] : graph.get_neighbors(vt)) {
            if (!v->is_z()) return;  // vt is not internal or not graph-like
        }

        if (allow_overlapping_candidates) return;

        // Both vs and vt are interior vertices
        candidates->erase(vs);
        candidates->erase(vt);
        for (auto& [v, _] : graph.get_neighbors(vs)) candidates->erase(v);
        for (auto& [v, _] : graph.get_neighbors(vt)) candidates->erase(v);

        matches.emplace_back(vs, vt);
    });

    return matches;
}

void PivotGadgetRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    for (auto& [_, v] : matches) {
        // REVIEW - scalar add power
        if (v->get_phase().denominator() != 1) {
            zx::gadgetize_phase(graph, v->get_id());
        }
    }

    PivotRuleInterface::apply(graph, matches);
}
