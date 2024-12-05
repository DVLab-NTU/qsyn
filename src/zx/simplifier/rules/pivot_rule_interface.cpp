/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Common Interface to Pivot-like Rule ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

void PivotRuleInterface::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto& m : matches) {
        auto [m0, m1] = m;

        std::vector<ZXVertex*> n0, n1, n2;
        std::vector<ZXVertex*> m0_neighbors = graph.get_copied_neighbors(m0);
        std::vector<ZXVertex*> m1_neighbors = graph.get_copied_neighbors(m1);

        std::erase(m0_neighbors, m1);
        std::erase(m1_neighbors, m0);

        auto vid_less_than = [](ZXVertex* const& a, ZXVertex* const& b) {
            return a->get_id() < b->get_id();
        };

        std::ranges::sort(m0_neighbors, vid_less_than);
        std::ranges::sort(m1_neighbors, vid_less_than);
        set_intersection(std::begin(m0_neighbors), std::end(m0_neighbors), std::begin(m1_neighbors), std::end(m1_neighbors), back_inserter(n2), vid_less_than);

        std::ranges::sort(n2, vid_less_than);
        set_difference(std::begin(m0_neighbors), std::end(m0_neighbors), std::begin(n2), std::end(n2), back_inserter(n0), vid_less_than);
        set_difference(std::begin(m1_neighbors), std::end(m1_neighbors), std::begin(n2), std::end(n2), back_inserter(n1), vid_less_than);

        // Add edge table
        for (auto const& s : n0) {
            for (auto const& t : n1) {
                assert(s->get_id() != t->get_id());
                op.edges_to_add.emplace_back(std::make_pair(s, t), EdgeType::hadamard);
            }
            for (auto const& t : n2) {
                assert(s->get_id() != t->get_id());
                op.edges_to_add.emplace_back(std::make_pair(s, t), EdgeType::hadamard);
            }
        }
        for (auto const& s : n1) {
            for (auto const& t : n2) {
                assert(s->get_id() != t->get_id());
                op.edges_to_add.emplace_back(std::make_pair(s, t), EdgeType::hadamard);
            }
        }

        // REVIEW - check if not ground
        for (auto const& v : n0) v->phase() += m1->phase();
        for (auto const& v : n1) v->phase() += m0->phase();
        for (auto const& v : n2) v->phase() += m0->phase() + m1->phase() + Phase(1);

        op.vertices_to_remove.emplace_back(m0);
        op.vertices_to_remove.emplace_back(m1);
    }

    _update(graph, op);
}
