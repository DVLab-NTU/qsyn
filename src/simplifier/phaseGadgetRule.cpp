/****************************************************************************
  FileName     [ phaseGadgetRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRulesTemplate.hpp"

using MatchType = PhaseGadgetRule::MatchType;

extern size_t verbose;

/**
 * @brief Determine which phase gadgets act on the same vertices, so that they can be fused together.
 *
 * @param graph The graph to find matches in.
 */
std::vector<MatchType> PhaseGadgetRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<ZXVertex*, ZXVertex*> axel2leaf;
    std::unordered_multimap<std::vector<ZXVertex*>, ZXVertex*> group2axel;
    std::unordered_set<std::vector<ZXVertex*>> done;

    std::vector<ZXVertex*> axels;
    std::vector<ZXVertex*> leaves;
    for (const auto& v : graph.getVertices()) {
        if (v->getPhase().denominator() <= 2 || v->getNumNeighbors() != 1) continue;

        ZXVertex* nb = v->getFirstNeighbor().first;

        if (nb->getPhase().denominator() != 1) continue;
        if (nb->isBoundary()) continue;
        if (axel2leaf.contains(nb)) continue;

        axel2leaf[nb] = v;

        std::vector<ZXVertex*> group;

        for (auto& [nb2, _] : nb->getNeighbors()) {
            if (nb2 != v) group.emplace_back(nb2);
        }

        if (group.size() > 0) {
            sort(group.begin(), group.end());
            group2axel.emplace(group, nb);
        }

        if (verbose >= 9) {
            for (auto& vertex : group) {
                std::cout << vertex->getId() << " ";
            }
            std::cout << " axel added: " << nb->getId() << std::endl;
        }
    }
    auto itr = group2axel.begin();
    while (itr != group2axel.end()) {
        auto [groupBegin, groupEnd] = group2axel.equal_range(itr->first);
        itr = groupEnd;

        axels.clear();
        leaves.clear();

        Phase totalPhase = Phase(0);
        bool flipAxel = false;
        for (auto& [_, axel] : std::ranges::subrange(groupBegin, groupEnd)) {
            ZXVertex* const& leaf = axel2leaf[axel];
            if (axel->getPhase() == Phase(1)) {
                flipAxel = true;
                axel->setPhase(Phase(0));
                leaf->setPhase((-1) * axel2leaf[axel]->getPhase());
            }
            totalPhase += axel2leaf[axel]->getPhase();
            axels.emplace_back(axel);
            leaves.emplace_back(axel2leaf[axel]);
        }

        if (leaves.size() > 1 || flipAxel) {
            matches.emplace_back(totalPhase, axels, leaves);
        }
    }

    return matches;
}

void PhaseGadgetRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    for (auto& match : matches) {
        const Phase& newPhase = get<0>(match);
        const std::vector<ZXVertex*>& rmAxels = get<1>(match);
        const std::vector<ZXVertex*>& rmLeaves = get<2>(match);
        ZXVertex* leaf = rmLeaves[0];
        leaf->setPhase(newPhase);
        op.verticesToRemove.insert(op.verticesToRemove.end(), rmAxels.begin() + 1, rmAxels.end());
        op.verticesToRemove.insert(op.verticesToRemove.end(), rmLeaves.begin() + 1, rmLeaves.end());
    }

    update(graph, op);
}
