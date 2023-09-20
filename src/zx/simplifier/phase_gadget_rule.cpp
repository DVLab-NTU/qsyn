/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <ranges>

#include "./zx_rules_template.hpp"
#include "util/logger.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = PhaseGadgetRule::MatchType;

extern dvlab::Logger LOGGER;

struct ZXVerticesHash {
    size_t operator()(std::vector<ZXVertex*> const& k) const {
        size_t ret = std::hash<ZXVertex*>()(k[0]);
        for (size_t i = 1; i < k.size(); i++) {
            ret ^= std::hash<ZXVertex*>()(k[i]);
        }

        return ret;
    }
};

/**
 * @brief Determine which phase gadgets act on the same vertices, so that they can be fused together.
 *
 * @param graph The graph to find matches in.
 */
std::vector<MatchType> PhaseGadgetRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<ZXVertex*, ZXVertex*> axel2leaf;
    std::unordered_multimap<std::vector<ZXVertex*>, ZXVertex*, ZXVerticesHash> group2axel;
    std::unordered_set<std::vector<ZXVertex*>, ZXVerticesHash> done;

    std::vector<ZXVertex*> axels;
    std::vector<ZXVertex*> leaves;
    for (auto const& v : graph.get_vertices()) {
        if (v->get_phase().denominator() <= 2 || v->get_num_neighbors() != 1) continue;

        ZXVertex* nb = v->get_first_neighbor().first;

        if (nb->get_phase().denominator() != 1) continue;
        if (nb->is_boundary()) continue;
        if (axel2leaf.contains(nb)) continue;

        axel2leaf[nb] = v;

        std::vector<ZXVertex*> group;

        for (auto& [nb2, _] : nb->get_neighbors()) {
            if (nb2 != v) group.emplace_back(nb2);
        }

        if (group.size() > 0) {
            sort(group.begin(), group.end());
            group2axel.emplace(group, nb);
        }

        LOGGER.trace("{} axel added: {}", fmt::join(group | std::views::transform([](ZXVertex* v) { return v->get_id(); }), " "), nb->get_id());
    }
    auto itr = group2axel.begin();
    while (itr != group2axel.end()) {
        auto [groupBegin, groupEnd] = group2axel.equal_range(itr->first);
        itr                         = groupEnd;

        axels.clear();
        leaves.clear();

        Phase total_phase = Phase(0);
        bool flip_axel    = false;
        for (auto& [_, axel] : std::ranges::subrange(groupBegin, groupEnd)) {
            ZXVertex* const& leaf = axel2leaf[axel];
            if (axel->get_phase() == Phase(1)) {
                flip_axel = true;
                axel->set_phase(Phase(0));
                leaf->set_phase((-1) * axel2leaf[axel]->get_phase());
            }
            total_phase += axel2leaf[axel]->get_phase();
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
        leaf->set_phase(new_phase);
        op.vertices_to_remove.insert(op.vertices_to_remove.end(), rm_axels.begin() + 1, rm_axels.end());
        op.vertices_to_remove.insert(op.vertices_to_remove.end(), rm_leaves.begin() + 1, rm_leaves.end());
    }

    _update(graph, op);
}
