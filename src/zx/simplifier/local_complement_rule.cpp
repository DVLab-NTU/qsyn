/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Local Complementary Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <unordered_set>
#include <utility>

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = LocalComplementRule::MatchType;

/**
 * @brief Find noninteracting matchings of the local complementation rule.
 *
 * @param graph The graph to find matches in.
 */
std::vector<MatchType> LocalComplementRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    std::unordered_set<ZXVertex*> taken;

    for (auto const& v : graph.get_vertices()) {
        if (v->get_type() == VertexType::z && (v->get_phase() == Phase(1, 2) || v->get_phase() == Phase(3, 2))) {
            bool match_condition = true;
            if (taken.contains(v)) continue;

            for (auto const& [nb, etype] : v->get_neighbors()) {
                if (etype != EdgeType::hadamard || nb->get_type() != VertexType::z || taken.contains(nb)) {
                    match_condition = false;
                    break;
                }
            }
            if (match_condition) {
                std::vector<ZXVertex*> neighbors;
                for (auto const& [nb, _] : v->get_neighbors()) {
                    if (v == nb) continue;
                    neighbors.emplace_back(nb);
                    taken.insert(nb);
                }
                taken.insert(v);
                matches.emplace_back(make_pair(v, neighbors));
            }
        }
    }

    return matches;
}

void LocalComplementRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    ZXOperation op;

    for (auto const& [v, neighbors] : matches) {
        op.vertices_to_remove.emplace_back(v);
        size_t h_edge_count = 0;
        for (auto& [nb, etype] : v->get_neighbors()) {
            if (nb == v && etype == EdgeType::hadamard) {
                h_edge_count++;
            }
        }
        Phase p = v->get_phase() + Phase(gsl::narrow<int>(h_edge_count / 2));
        // TODO: global scalar ignored
        for (size_t n = 0; n < neighbors.size(); n++) {
            neighbors[n]->set_phase(neighbors[n]->get_phase() - p);
            for (size_t j = n + 1; j < neighbors.size(); j++) {
                op.edges_to_add.emplace_back(std::make_pair(neighbors[n], neighbors[j]), EdgeType::hadamard);
            }
        }
    }

    _update(graph, op);
}
