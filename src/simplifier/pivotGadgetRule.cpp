/****************************************************************************
  FileName     [ pivotgadget.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRulesTemplate.hpp"

using MatchType = PivotGadgetRule::MatchType;

extern size_t verbose;

std::vector<MatchType> PivotGadgetRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    if (verbose >= 8) std::cout << "> match...\n";

    size_t count = 0;

    std::unordered_set<ZXVertex*> taken;

    graph.forEachEdge([&count, &taken, &matches, this](const EdgePair& epair) {
        if (epair.second != EdgeType::HADAMARD) return;

        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if (taken.contains(vs) || taken.contains(vt)) return;

        if (verbose == 9) std::cout << "\n-----------\n\n"
                                    << "Edge " << count << ": " << vs->getId() << " " << vt->getId() << "\n";

        if (!vs->isZ()) {
            taken.insert(vs);
            return;
        }
        if (!vt->isZ()) {
            taken.insert(vt);
            return;
        }

        if (verbose == 9) std::cout << "(1) type pass\n";

        bool vsIsNPi = (vs->getPhase().denominator() == 1);
        bool vtIsNPi = (vt->getPhase().denominator() == 1);

        // if both n*pi --> ordinary pivot rules
        // if both not, --> maybe pivot double-boundary
        if (vsIsNPi == vtIsNPi) return;

        if (!vsIsNPi && vtIsNPi) std::swap(vs, vt);  // if vs is not n*pi but vt is, should extract vs as gadget instead

        if (verbose == 9) std::cout << "(2) phase pass\n";

        // REVIEW - check ground conditions

        if (vt->getNumNeighbors() == 1) {  // early return: (vs, vt) is a phase gadget
            taken.insert(vs);
            taken.insert(vt);
            return;
        }

        for (const auto& [v, _] : vs->getNeighbors()) {
            if (!v->isZ()) return;            // vs is not internal or not graph-like
            if (v->getNumNeighbors() == 1) {  // (vs, v) is a phase gadget
                taken.insert(vs);
                taken.insert(v);
                return;
            }
        }
        for (const auto& [v, _] : vt->getNeighbors()) {
            if (!v->isZ()) return;  // vt is not internal or not graph-like
        }

        if (verbose == 9) std::cout << "(3) good match\n";

        // Both vs and vt are interior
        if (verbose >= 8) std::cout << "Both vertices are both interior: " << vs->getId() << " " << vt->getId() << std::endl;

        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->getNeighbors()) taken.insert(v);
        for (auto& [v, _] : vt->getNeighbors()) taken.insert(v);

        matches.emplace_back(vs, vt);
    });

    return matches;
}

void PivotGadgetRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    for (auto& [_, v] : matches) {
        // REVIEW - scalar add power
        if (v->getPhase().denominator() != 1) {
            graph.transferPhase(v);
        }
    }

    PivotRuleInterface::apply(graph, matches);
}
