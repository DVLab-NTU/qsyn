/****************************************************************************
  FileName     [ hadamardRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zxRulesTemplate.hpp"

using MatchType = HadamardRule::MatchType;

extern size_t verbose;

std::vector<MatchType> HadamardRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<size_t, size_t> id2idx;

    size_t count = 0;
    for (const auto& v : graph.getVertices()) {
        id2idx[v->getId()] = count;
        count++;
    }
    // Find all H-boxes
    std::vector<bool> taken(graph.getNumVertices(), false);
    std::vector<bool> inMatches(graph.getNumVertices(), false);

    for (const auto& v : graph.getVertices()) {
        if (v->getType() == VertexType::H_BOX && v->getNumNeighbors() == 2) {
            NeighborPair nbp0 = v->getFirstNeighbor();
            NeighborPair nbp1 = v->getSecondNeighbor();
            size_t n0 = id2idx[nbp0.first->getId()], n1 = id2idx[nbp1.first->getId()];
            if (taken[n0] || taken[n1]) continue;
            if (!inMatches[n0] && !inMatches[n1]) {
                matches.emplace_back(v);
                inMatches[id2idx[v->getId()]] = true;
                taken[n0] = true;
                taken[n1] = true;
            }
        }
    }

    return matches;
}

void HadamardRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op = {
        .verticesToRemove = matches,
    };

    for (auto& v : matches) {
        // NOTE: Only two neighbors which is ensured
        std::vector<ZXVertex*> neighborVertices;
        std::vector<EdgeType> neighborEdgeTypes;

        for (auto& [neighbor, edgeType] : v->getNeighbors()) {
            neighborVertices.emplace_back(neighbor);
            neighborEdgeTypes.emplace_back(edgeType);
        }

        op.edgesToAdd.emplace_back(
            std::make_pair(neighborVertices[0], neighborVertices[1]),
            neighborEdgeTypes[0] == neighborEdgeTypes[1] ? EdgeType::HADAMARD : EdgeType::SIMPLE);
        // TODO: Correct for the sqrt(2) difference in H-boxes and H-edges
    }

    update(graph, op);
}
