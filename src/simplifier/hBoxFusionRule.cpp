/****************************************************************************
  FileName     [ hBoxFusionRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Hadamard Cancellation Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zxRulesTemplate.hpp"

using MatchType = HBoxFusionRule::MatchType;

extern size_t verbose;

std::vector<MatchType> HBoxFusionRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<size_t, size_t> id2idx;
    size_t count = 0;
    for (const auto& v : graph.getVertices()) {
        id2idx[v->getId()] = count;
        count++;
    }

    // Matches Hadamard-edges that are connected to H-boxes
    std::vector<bool> taken(graph.getNumVertices(), false);

    graph.forEachEdge([&id2idx, &taken, &matches, this](const EdgePair& epair) {
        // NOTE - Only Hadamard Edges
        if (epair.second != EdgeType::HADAMARD) return;
        auto [neighbor_left, neighbor_right] = epair.first;
        size_t n0 = id2idx[neighbor_left->getId()], n1 = id2idx[neighbor_right->getId()];

        if ((taken[n0] && neighbor_left->getType() == VertexType::H_BOX) || (taken[n1] && neighbor_right->getType() == VertexType::H_BOX)) return;

        if (neighbor_left->getType() == VertexType::H_BOX) {
            matches.emplace_back(neighbor_left);
            taken[n0] = true;
            taken[n1] = true;

            Neighbors nebs = neighbor_left->getNeighbors();
            NeighborPair nbp0 = *(nebs.begin());
            NeighborPair nbp1 = *next(nebs.begin());
            size_t n2 = id2idx[nbp0.first->getId()], n3 = id2idx[nbp1.first->getId()];

            if (n2 != n0)
                taken[n2] = true;
            else
                taken[n3] = true;
        } else if (neighbor_right->getType() == VertexType::H_BOX) {
            matches.emplace_back(neighbor_right);
            taken[n0] = true;
            taken[n1] = true;

            Neighbors nebs = neighbor_left->getNeighbors();
            NeighborPair nbp0 = *(nebs.begin());
            NeighborPair nbp1 = *next(nebs.begin());
            size_t n2 = id2idx[nbp0.first->getId()], n3 = id2idx[nbp1.first->getId()];

            if (n2 != n0)
                taken[n2] = true;
            else
                taken[n3] = true;
        } else if (neighbor_left->getType() != VertexType::H_BOX || neighbor_right->getType() != VertexType::H_BOX) {
            return;
        }
    });

    graph.forEachEdge([&id2idx, &taken, &matches, this](const EdgePair& epair) {
        if (epair.second == EdgeType::HADAMARD) return;

        ZXVertex* neighbor_left = epair.first.first;
        ZXVertex* neighbor_right = epair.first.second;
        size_t n0 = id2idx[neighbor_left->getId()], n1 = id2idx[neighbor_right->getId()];

        if (!taken[n0] && !taken[n1]) {
            if (neighbor_left->getType() == VertexType::H_BOX && neighbor_right->getType() == VertexType::H_BOX) {
                matches.emplace_back(neighbor_left);
                matches.emplace_back(neighbor_right);
                taken[n0] = true;
                taken[n1] = true;
            }
        }
    });

    return matches;
}

void HBoxFusionRule::apply(ZXGraph& graph, const std::vector<MatchType>& _matchTypeVec) const {
    ZXOperation op = {
        .verticesToRemove = _matchTypeVec,
    };

    for (size_t i = 0; i < _matchTypeVec.size(); i++) {
        // NOTE: Only two neighbors which is ensured
        std::vector<ZXVertex*> ns;
        std::vector<EdgeType> ets;

        for (auto& itr : _matchTypeVec[i]->getNeighbors()) {
            ns.emplace_back(itr.first);
            ets.emplace_back(itr.second);
        }

        op.edgesToAdd.emplace_back(std::make_pair(ns[0], ns[1]), ets[0] == ets[1] ? EdgeType::HADAMARD : EdgeType::SIMPLE);
        // TODO: Correct for the sqrt(2) difference in H-boxes and H-edges
    }

    ZXRuleTemplate::update(graph, op);
}
