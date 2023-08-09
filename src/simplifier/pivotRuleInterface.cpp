/****************************************************************************
  FileName     [ pivotRuleInterface.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Common Interface to Pivot-like Rule ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxRulesTemplate.hpp"

extern size_t verbose;

void PivotRuleInterface::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    for (auto& m : matches) {
        if (verbose >= 8) {
            std::cout << "> rewrite...\n";
            std::cout << "vs: " << m[0]->getId() << "\tvt: " << m[1]->getId() << std::endl;
        }

        std::vector<ZXVertex*> n0, n1, n2;
        std::vector<ZXVertex*> m0_neighbors = m[0]->getCopiedNeighbors();
        std::vector<ZXVertex*> m1_neighbors = m[1]->getCopiedNeighbors();

        std::erase(m0_neighbors, m[1]);
        std::erase(m1_neighbors, m[0]);

        auto vidLessThan = [](ZXVertex* const& a, ZXVertex* const& b) {
            return a->getId() < b->getId();
        };

        std::sort(m0_neighbors.begin(), m0_neighbors.end(), vidLessThan);
        std::sort(m1_neighbors.begin(), m1_neighbors.end(), vidLessThan);
        set_intersection(m0_neighbors.begin(), m0_neighbors.end(), m1_neighbors.begin(), m1_neighbors.end(), back_inserter(n2), vidLessThan);

        std::sort(n2.begin(), n2.end(), vidLessThan);
        set_difference(m0_neighbors.begin(), m0_neighbors.end(), n2.begin(), n2.end(), back_inserter(n0), vidLessThan);
        set_difference(m1_neighbors.begin(), m1_neighbors.end(), n2.begin(), n2.end(), back_inserter(n1), vidLessThan);

        // Add edge table
        for (const auto& s : n0) {
            for (const auto& t : n1) {
                assert(s->getId() != t->getId());
                op.edgesToAdd.emplace_back(std::make_pair(s, t), EdgeType::HADAMARD);
            }
            for (const auto& t : n2) {
                assert(s->getId() != t->getId());
                op.edgesToAdd.emplace_back(std::make_pair(s, t), EdgeType::HADAMARD);
            }
        }
        for (const auto& s : n1) {
            for (const auto& t : n2) {
                assert(s->getId() != t->getId());
                op.edgesToAdd.emplace_back(std::make_pair(s, t), EdgeType::HADAMARD);
            }
        }

        // REVIEW - check if not ground
        for (const auto& v : n0) v->setPhase(v->getPhase() + m[1]->getPhase());
        for (const auto& v : n1) v->setPhase(v->getPhase() + m[0]->getPhase());
        for (const auto& v : n2) v->setPhase(v->getPhase() + m[0]->getPhase() + m[1]->getPhase() + Phase(1));

        for (int i = 0; i < 2; ++i) {
            op.verticesToRemove.emplace_back(m[i]);
        }
        // break;
    }

    update(graph, op);
}
