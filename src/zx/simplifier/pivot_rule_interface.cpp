/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Common Interface to Pivot-like Rule ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

extern size_t VERBOSE;

using namespace qsyn::zx;

void PivotRuleInterface::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto& m : matches) {
        auto [m0, m1] = m;

        if (VERBOSE >= 8) {
            std::cout << "> rewrite...\n";
            std::cout << "vs: " << m0->get_id() << "\tvt: " << m1->get_id() << std::endl;
        }

        std::vector<ZXVertex*> n0, n1, n2;
        std::vector<ZXVertex*> m0_neighbors = m0->get_copied_neighbors();
        std::vector<ZXVertex*> m1_neighbors = m1->get_copied_neighbors();

        std::erase(m0_neighbors, m1);
        std::erase(m1_neighbors, m0);

        auto vid_less_than = [](ZXVertex* const& a, ZXVertex* const& b) {
            return a->get_id() < b->get_id();
        };

        std::sort(m0_neighbors.begin(), m0_neighbors.end(), vid_less_than);
        std::sort(m1_neighbors.begin(), m1_neighbors.end(), vid_less_than);
        set_intersection(m0_neighbors.begin(), m0_neighbors.end(), m1_neighbors.begin(), m1_neighbors.end(), back_inserter(n2), vid_less_than);

        std::sort(n2.begin(), n2.end(), vid_less_than);
        set_difference(m0_neighbors.begin(), m0_neighbors.end(), n2.begin(), n2.end(), back_inserter(n0), vid_less_than);
        set_difference(m1_neighbors.begin(), m1_neighbors.end(), n2.begin(), n2.end(), back_inserter(n1), vid_less_than);

        // Add edge table
        for (auto const& s : n0) {
            for (auto const& t : n1) {
                assert(s->get_id() != t->get_id());
                op.edgesToAdd.emplace_back(std::make_pair(s, t), EdgeType::hadamard);
            }
            for (auto const& t : n2) {
                assert(s->get_id() != t->get_id());
                op.edgesToAdd.emplace_back(std::make_pair(s, t), EdgeType::hadamard);
            }
        }
        for (auto const& s : n1) {
            for (auto const& t : n2) {
                assert(s->get_id() != t->get_id());
                op.edgesToAdd.emplace_back(std::make_pair(s, t), EdgeType::hadamard);
            }
        }

        // REVIEW - check if not ground
        for (auto const& v : n0) v->set_phase(v->get_phase() + m1->get_phase());
        for (auto const& v : n1) v->set_phase(v->get_phase() + m0->get_phase());
        for (auto const& v : n2) v->set_phase(v->get_phase() + m0->get_phase() + m1->get_phase() + Phase(1));

        op.vertices_to_remove.emplace_back(m0);
        op.vertices_to_remove.emplace_back(m1);
    }

    _update(graph, op);
}
