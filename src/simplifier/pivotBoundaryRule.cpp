/****************************************************************************
  FileName     [ pivotBoundaryRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRulesTemplate.hpp"

using MatchType = PivotBoundaryRule::MatchType;

extern size_t verbose;

std::vector<MatchType> PivotBoundaryRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;
    auto matchBoundary = [&taken, &graph, &matches, this](ZXVertex* v) {
        ZXVertex* vs = v->getFirstNeighbor().first;
        if (taken.contains(vs)) return;

        if (!vs->isZ()) {
            taken.insert(vs);
            return;
        }

        ZXVertex* vt = nullptr;
        for (auto& [nb, etype] : vs->getNeighbors()) {
            if (taken.contains(nb)) continue;  // do not choose the one in taken
            if (nb->isBoundary()) continue;
            if (!nb->hasNPiPhase()) continue;
            if (etype != EdgeType::HADAMARD) continue;
            if (graph.hasDanglingNeighbors(nb)) continue;  // nb is the axel of a phase gadget
            vt = nb;
            break;
        }
        if (vt == nullptr) return;

        bool foundOne = false;
        // check vs is only connected to boundary, or connected to Z-spider by H-edge
        for (auto& [nb, etype] : vs->getNeighbors()) {
            if (nb->isBoundary()) {
                if (foundOne) return;
                foundOne = true;
                continue;
            }
            if (!nb->isZ() || etype != EdgeType::HADAMARD) return;
        }

        // check vt is only connected to Z-spider by H-edge
        for (auto& [nb, etype] : vt->getNeighbors()) {
            if (!nb->isZ() || etype != EdgeType::HADAMARD) return;
        }

        taken.insert(vs);
        taken.insert(vt);

        for (auto& [nb, _] : vs->getNeighbors()) taken.insert(nb);
        for (auto& [nb, _] : vt->getNeighbors()) taken.insert(nb);
        matches.push_back({vs, vt});  // NOTE: cannot emplace_back -- std::array does not have a constructor!;
    };

    for (auto& v : graph.getInputs()) matchBoundary(v);
    for (auto& v : graph.getOutputs()) matchBoundary(v);

    return matches;
}

void PivotBoundaryRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    for (auto& [vs, _] : matches) {
        for (auto& [nb, etype] : vs->getNeighbors()) {
            if (nb->isBoundary()) {
                graph.addBuffer(nb, vs, etype);
                break;
            }
            if (!nb->isZ() || etype != EdgeType::HADAMARD) return;
        }
    }
    for (auto& m : matches) {
        if (!m[0]->hasNPiPhase()) graph.transferPhase(m[0]);
        if (!m[1]->hasNPiPhase()) graph.transferPhase(m[1]);
    }

    PivotRuleInterface::apply(graph, matches);
}
