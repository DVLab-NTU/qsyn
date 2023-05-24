/****************************************************************************
  FileName     [ phasegadget.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t
#include <iostream>
#include <ranges>

#include "zxGraph.h"
#include "zxRules.h"

using namespace std;

extern size_t verbose;

/**
 * @brief Determine which phase gadgets act on the same vertices, so that they can be fused together.
 *
 * @param g
 */
void PhaseGadget::match(ZXGraph* g) {
    _matchTypeVec.clear();

    unordered_map<ZXVertex*, ZXVertex*> axel2leaf;
    unordered_multimap<vector<ZXVertex*>, ZXVertex*> group2axel;
    unordered_set<vector<ZXVertex*>> done;

    vector<ZXVertex*> axels;
    vector<ZXVertex*> leaves;
    for (const auto& v : g->getVertices()) {
        if (v->getPhase().getRational().denominator() <= 2 || v->getNumNeighbors() != 1) continue;

        ZXVertex* nb = v->getFirstNeighbor().first;

        if (nb->getPhase().getRational().denominator() != 1) continue;
        if (nb->isBoundary()) continue;
        if (axel2leaf.contains(nb)) continue;

        axel2leaf[nb] = v;

        vector<ZXVertex*> group;

        for (auto& [nb2, _] : nb->getNeighbors()) {
            if (nb2 != v) group.push_back(nb2);
        }

        if (group.size() > 0) {
            sort(group.begin(), group.end());
            group2axel.emplace(group, nb);
        }

        if (verbose >= 9) {
            for (auto& vertex : group) {
                cout << vertex->getId() << " ";
            }
            cout << " axel added: " << nb->getId() << endl;
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
#ifdef __APPLE__  // As of 2023-05-25, Apple Clang does not have proper support for ranges library
        for (auto itr = groupBegin; itr != groupEnd; ++itr) {
            auto axel = itr->second;
#else
        for (auto& [_, axel] : ranges::subrange(groupBegin, groupEnd)) {
#endif
            ZXVertex* const& leaf = axel2leaf[axel];
            if (axel->getPhase() == Phase(1)) {
                flipAxel = true;
                axel->setPhase(Phase(0));
                leaf->setPhase((-1) * axel2leaf[axel]->getPhase());
            }
            totalPhase += axel2leaf[axel]->getPhase();
            axels.push_back(axel);
            leaves.push_back(axel2leaf[axel]);
        }

        if (leaves.size() > 1 || flipAxel) {
            _matchTypeVec.emplace_back(totalPhase, axels, leaves);
        }
    }

    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *
 * @param g
 */
void PhaseGadget::rewrite(ZXGraph* g) {
    reset();

    for (auto& m : _matchTypeVec) {
        const Phase& newPhase = get<0>(m);
        const vector<ZXVertex*>& rmAxels = get<1>(m);
        const vector<ZXVertex*>& rmLeaves = get<2>(m);
        ZXVertex* leaf = rmLeaves[0];
        leaf->setPhase(newPhase);
        _removeVertices.insert(_removeVertices.end(), rmAxels.begin() + 1, rmAxels.end());
        _removeVertices.insert(_removeVertices.end(), rmLeaves.begin() + 1, rmLeaves.end());
    }
}