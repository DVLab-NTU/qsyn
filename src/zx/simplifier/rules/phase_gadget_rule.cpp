/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <ranges>
#include <tl/fold.hpp>

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = PhaseGadgetRule::MatchType;

struct ZXVerticesHash {
    size_t operator()(std::vector<ZXVertex*> const& k) const {
        return tl::fold_left(
            k, size_t{0},
            [](size_t acc, ZXVertex* v) {
                return acc ^ std::hash<decltype(v)>()(v);
            });
    }
};

/**
 * @brief Find matchings of the phase gadget fusion rule.
 *
 * @param graph
 * @param candidates the vertices to be considered
 * @return std::vector<MatchType>
 */
std::vector<MatchType> PhaseGadgetRule::find_matches(
    ZXGraph const& graph, std::optional<ZXVertexList> candidates) const {
    if (!candidates.has_value()) {
        candidates = graph.get_vertices();
    }

    std::vector<MatchType> matches;

    std::unordered_map<ZXVertex*, ZXVertex*> axel2leaf;
    dvlab::utils::ordered_hashmap<std::vector<ZXVertex*>, std::vector<ZXVertex*>, ZXVerticesHash> group2axel;

    std::vector<ZXVertex*> axels;
    std::vector<ZXVertex*> leaves;
    for (auto const& v : graph.get_vertices()) {
        if (!candidates->contains(v)) continue;
        if (v->phase().denominator() <= 2 || graph.num_neighbors(v) != 1) continue;

        ZXVertex* nb = graph.get_first_neighbor(v).first;

        if (nb->phase().denominator() != 1) continue;
        if (nb->is_boundary()) continue;
        if (axel2leaf.contains(nb)) continue;

        axel2leaf[nb] = v;

        std::vector<ZXVertex*> group;

        for (auto& [nb2, _] : graph.get_neighbors(nb)) {
            if (nb2 != v) group.emplace_back(nb2);
        }

        if (!group.empty()) {
            std::ranges::sort(group);
            if (group2axel.contains(group)) {
                group2axel.at(group).emplace_back(nb);
            } else {
                group2axel.emplace(group, std::vector<ZXVertex*>{nb});
            }
        }
    }
    for (auto const& [_, tmp_axels] : group2axel) {
        axels.clear();
        leaves.clear();

        auto total_phase = Phase(0);
        bool flip_axel   = false;
        for (auto const& axel : tmp_axels) {
            ZXVertex* const& leaf = axel2leaf[axel];
            if (axel->phase() == Phase(1)) {
                flip_axel     = true;
                axel->phase() = Phase(0);
                leaf->phase() = (-1) * axel2leaf[axel]->phase();
            }
            total_phase += axel2leaf[axel]->phase();
            axels.emplace_back(axel);
            leaves.emplace_back(axel2leaf[axel]);
        }

        if (leaves.size() > 1 || flip_axel) {
            matches.emplace_back(total_phase, axels, leaves);
        }
    }

    return matches;
}

void PhaseGadgetRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto& match : matches) {
        Phase const& new_phase                  = get<0>(match);
        std::vector<ZXVertex*> const& rm_axels  = get<1>(match);
        std::vector<ZXVertex*> const& rm_leaves = get<2>(match);
        ZXVertex* leaf                          = rm_leaves[0];
        leaf->phase()                           = new_phase;
        op.vertices_to_remove.insert(std::end(op.vertices_to_remove), std::begin(rm_axels) + 1, std::end(rm_axels));
        op.vertices_to_remove.insert(std::end(op.vertices_to_remove), std::begin(rm_leaves) + 1, std::end(rm_leaves));
    }

    _update(graph, op);
}
