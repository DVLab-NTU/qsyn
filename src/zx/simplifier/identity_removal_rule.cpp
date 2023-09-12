/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Identity Removal Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

using MatchType = IdentityRemovalRule::MatchType;

/**
 * @brief Find all the matches of the identity removal rule.
 *
 * @param g The graph to be simplified.
 */
std::vector<MatchType> IdentityRemovalRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;

    for (auto const& v : graph.get_vertices()) {
        if (taken.contains(v)) continue;

        Neighbors const& neighbors = v->get_neighbors();
        if (v->get_phase() != Phase(0)) continue;
        if (v->get_type() != VertexType::z && v->get_type() != VertexType::x) continue;
        if (neighbors.size() != 2) continue;

        auto [n0, edgeType0] = *(neighbors.begin());
        auto [n1, edgeType1] = *next(neighbors.begin());

        EdgeType edge_type = (edgeType0 == edgeType1) ? EdgeType::simple : EdgeType::hadamard;

        matches.emplace_back(v, n0, n1, edge_type);
        taken.insert(v);
        taken.insert(n0);
        taken.insert(n1);
    }

    return matches;
}

/**
 * @brief Apply the identity removal rule to the graph.
 *
 * @param graph The graph to be simplified.
 * @param matches The matches of the identity removal rule.
 */
void IdentityRemovalRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto const& match : matches) {
        ZXVertex* v = std::get<0>(match);
        ZXVertex* n0 = std::get<1>(match);
        ZXVertex* n1 = std::get<2>(match);
        EdgeType edge_type = std::get<3>(match);

        op.vertices_to_remove.emplace_back(v);
        if (n0 == n1) {
            n0->set_phase(n0->get_phase() + Phase(1));
            continue;
        }
        op.edgesToAdd.emplace_back(std::make_pair(n0, n1), edge_type);
    }

    _update(graph, op);
}
