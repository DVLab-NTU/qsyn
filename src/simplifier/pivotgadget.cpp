// /****************************************************************************
//   FileName     [ pivotgadget.cpp ]
//   PackageName  [ simplifier ]
//   Synopsis     [ Pivot Gadget Rule Definition ]
//   Author       [ Cheng-Hua Lu ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include <iostream>
#include <numbers>
#include <vector>

#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Preprocess the matches so that it conforms with the rewrite functions
 *
 * @param g
 */
void PivotGadget::preprocess(ZXGraph* g) {
    for (auto& m : this->_matchTypeVec) {
        // REVIEW - scalar add power
        if (m[1]->getPhase().getRational().denominator() != 1) {
            g->transferPhase(m[1]);
        }
    }
}

// FIXME wrong brief
/**
 * @brief Determines which phase gadgets act on the same vertices, so that they can be fused together.
 *        (Check PyZX/pyzx/rules.py/match_phase_gadgets for more details)
 *
 * @param g
 */
void PivotGadget::match(ZXGraph* g) {
    this->_matchTypeVec.clear();
    if (verbose >= 8) g->printVertices();
    if (verbose >= 5) cout << "> match...\n";

    size_t cnt = 0;

    unordered_set<ZXVertex*> taken;

    g->forEachEdge([&cnt, &taken, this](const EdgePair& epair) {
        if (epair.second != EdgeType::HADAMARD) return;

        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if (taken.contains(vs) || taken.contains(vt)) return;

        if (verbose == 9) cout << "\n-----------\n\n"
                               << "Edge " << cnt << ": " << vs->getId() << " " << vt->getId() << "\n";

        if (!vs->isZ()) {
            taken.insert(vs);
            return;
        }
        if (!vt->isZ()) {
            taken.insert(vt);
            return;
        }

        if (verbose == 9) cout << "(1) type pass\n";

        bool vsIsNPi = (vs->getPhase().getRational().denominator() == 1);
        bool vtIsNPi = (vt->getPhase().getRational().denominator() == 1);

        // if both n*pi --> ordinary pivot rules
        // if both not, --> maybe pivot double-boundary
        if (vsIsNPi == vtIsNPi) return;

        if (!vsIsNPi && vtIsNPi) swap(vs, vt);  // if vs is not n*pi but vt is, should extract vs as gadget instead

        if (verbose == 9) cout << "(2) phase pass\n";

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

        if (verbose == 9) cout << "(3) good match\n";

        // Both vs and vt are interior
        if (verbose >= 5) cout << "Both vertices are both interior: " << vs->getId() << " " << vt->getId() << endl;

        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->getNeighbors()) taken.insert(v);
        for (auto& [v, _] : vt->getNeighbors()) taken.insert(v);

        this->_matchTypeVec.push_back({vs, vt});
    });

    setMatchTypeVecNum(this->_matchTypeVec.size());
}
