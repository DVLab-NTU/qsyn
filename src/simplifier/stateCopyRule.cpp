/****************************************************************************
  FileName     [ stateCopyRule.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ State Copy Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zxRulesTemplate.hpp"

using MatchType = StateCopyRule::MatchType;

/**
 * @brief Find spiders with a 0 or pi phase that have a single neighbor, and copies them through. Assumes that all the spiders are green and maximally fused.
 *
 * @param graph The graph to be matched.
 */
std::vector<MatchType> StateCopyRule::findMatches(const ZXGraph& graph) const {
    std::vector<MatchType> matches;

    std::unordered_map<ZXVertex*, size_t> Vertex2idx;

    std::unordered_map<size_t, size_t> id2idx;
    size_t count = 0;
    for (const auto& v : graph.getVertices()) {
        Vertex2idx[v] = count;
        count++;
    }

    std::vector<bool> validVertex(graph.getNumVertices(), true);

    for (const auto& v : graph.getVertices()) {
        if (!validVertex[Vertex2idx[v]]) continue;

        if (v->getType() != VertexType::Z) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }
        if (v->getPhase() != Phase(0) && v->getPhase() != Phase(1)) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }
        if (v->getNumNeighbors() != 1) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }

        ZXVertex* PiNeighbor = v->getFirstNeighbor().first;
        if (PiNeighbor->getType() != VertexType::Z) {
            validVertex[Vertex2idx[v]] = false;
            continue;
        }
        std::vector<ZXVertex*> applyNeighbors;
        for (const auto& [nebOfPiNeighbor, _] : PiNeighbor->getNeighbors()) {
            if (nebOfPiNeighbor != v)
                applyNeighbors.emplace_back(nebOfPiNeighbor);
            validVertex[Vertex2idx[nebOfPiNeighbor]] = false;
        }
        matches.emplace_back(make_tuple(v, PiNeighbor, applyNeighbors));
    }

    return matches;
}

void StateCopyRule::apply(ZXGraph& graph, const std::vector<MatchType>& matches) const {
    ZXOperation op;

    // Need to update global scalar and phase
    for (auto& match : matches) {
        ZXVertex* npi = get<0>(match);
        ZXVertex* a = get<1>(match);
        std::vector<ZXVertex*> neighbors = get<2>(match);
        op.verticesToRemove.emplace_back(npi);
        op.verticesToRemove.emplace_back(a);
        for (auto neighbor : neighbors) {
            if (neighbor->getType() == VertexType::BOUNDARY) {
                ZXVertex* newV = graph.addVertex(neighbor->getQubit(), VertexType::Z, npi->getPhase());
                bool simpleEdge = neighbor->getFirstNeighbor().second == EdgeType::SIMPLE;

                op.edgesToRemove.emplace_back(std::make_pair(a, neighbor), neighbor->getFirstNeighbor().second);

                // new to Boundary
                op.edgesToAdd.emplace_back(std::make_pair(newV, neighbor), simpleEdge ? EdgeType::HADAMARD : EdgeType::SIMPLE);

                // a to new
                op.edgesToAdd.emplace_back(std::make_pair(a, newV), EdgeType::HADAMARD);

                // REVIEW - Floating
                newV->setCol((neighbor->getCol() + a->getCol()) / 2);

            } else {
                neighbor->setPhase(npi->getPhase() + neighbor->getPhase());
            }
        }
    }

    update(graph, op);
}
