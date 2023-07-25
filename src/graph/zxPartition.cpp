/****************************************************************************
  FileName     [ zxPartition.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Implements the split function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxPartition.h"

#include <iomanip>
#include <stack>
#include <unordered_map>

#include "zxDef.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"

extern ZXGraphMgr zxGraphMgr;

/*****************************************************/
/*   class ZXGraph partition functions.              */
/*****************************************************/

std::pair<std::vector<ZXGraph>, std::vector<ZXCut>> ZXGraph::createSubgraphs(ZXPartitionStrategy partitionStrategy, size_t numPartitions) {
    // TODO: Implement this function

    // NOTE: since deleting a subgraph also deletes the vertices in the subgraph,
    // which still have to be referenced by the merged graph, so we need to copy
    // merged graph back into the original graph
    // partition -> optimized subgraphs -> merged graph -> copy back to original graph

    std::vector<ZXVertexList> partitions = partitionStrategy(*this, numPartitions);
    ZXVertexList primaryInputs = getInputs();
    ZXVertexList primaryOutputs = getOutputs();

    // std::vector<ZXGraph*> subgraphs;
    // subgraphs.reserve(partitions.size());
    // std::unordered_set<EdgePair> cutEdges;
    //
    // for (auto& partition : partitions) {
    //     ZXGraph* graph = new ZXGraph(zxGraphMgr->getNextID());
    //     subgraphs.push_back(graph);
    //     graph->setVertices(partition);
    //
    //     ZXVertexList subgraphInputs;
    //     ZXVertexList subgraphOutputs;
    //     ZXVertexList boundaryVertices;
    //     for (auto& vertex : graph->getVertices()) {
    //         if (primaryInputs.contains(vertex)) subgraphInputs.insert(vertex);
    //         if (primaryOutputs.contains(vertex)) subgraphOutputs.insert(vertex);
    //         for (auto& [neighbor, edgeType] : vertex->getNeighbors()) {
    //             if (!partition.contains(neighbor)) {
    //                 cutEdges.insert({{vertex, neighbor}, edgeType});
    //                 boundaryVertices.insert(neighbor);
    //                 // NOTE: label all boundary verices as outputs if it is not an input,
    //                 // the code should only cares if a vertex is a boundary vertex
    //                 if (!subgraphInputs.contains(neighbor)) subgraphOutputs.insert(neighbor);
    //             }
    //         }
    //     }
    //
    //     graph->addVertices(boundaryVertices);
    //     graph->setInputs(subgraphInputs);
    //     graph->setOutputs(subgraphOutputs);
    //
    //     std::cerr << "==========" << std::endl;
    //     std::cerr << "before: " << std::endl;
    //     std::cerr << "inputs: " << std::endl;
    //     for (auto& vertex : graph->getInputs()) {
    //         vertex->printVertex();
    //     }
    //     std::cerr << "outputs: " << std::endl;
    //     for (auto& vertex : graph->getOutputs()) {
    //         vertex->printVertex();
    //     }
    //
    //     Simplifier simplifier(graph);
    //     simplifier.fullReduce();
    //
    //     std::cerr << "after: " << std::endl;
    //     std::cerr << "inputs: " << std::endl;
    //     for (auto& vertex : graph->getInputs()) {
    //         vertex->printVertex();
    //     }
    //     std::cerr << "outputs: " << std::endl;
    //     for (auto& vertex : graph->getOutputs()) {
    //         vertex->printVertex();
    //     }
    // }
    //
    // return;
    //
    // // merge the subgraphs back into the original graph
    // ZXGraph* mergedGraph = new ZXGraph(zxGraphMgr->getNextID());
    // for (auto& graph : subgraphs) {
    //     mergedGraph->addVertices(graph->getVertices());
    // }
    // for (auto& [edge, edgeType] : cutEdges) {
    //     mergedGraph->addEdge(edge.first, edge.second, edgeType);
    // }
    // mergedGraph->setInputs(primaryInputs);
    // mergedGraph->setOutputs(primaryOutputs);
    //
    // ZXGraph* oldGraph = _simpGraph;
    // _simpGraph = mergedGraph->copy();
    //
    // for (auto& g : subgraphs) {
    //     delete g;
    // }
    // delete oldGraph;
    // delete mergedGraph;

    return {std::vector<ZXGraph>(), std::vector<ZXCut>()};
}

ZXGraph ZXGraph::fromSubgraphs(const std::vector<ZXGraph>& subgraphs, const std::vector<ZXCut>& cuts) {
    return ZXGraph(zxGraphMgr.getNextID());
}

/*****************************************************/
/*  ZXGraph partition strategies.                    */
/*****************************************************/

std::pair<ZXVertexList, ZXVertexList> _klBiPartition(ZXVertexList vertices);

/* @brief Recursively partition the graph into numPartitions partitions using the Kernighan-Lin algorithm.
 * @param graph The graph to partition.
 * @param numPartitions The number of partitions to split the graph into.
 * @return A vector of vertex lists, each representing a partition.
 */
std::vector<ZXVertexList> klPartition(const ZXGraph& graph, size_t numPartitions) {
    std::vector<ZXVertexList> partitions = {graph.getVertices()};
    size_t count = 1;
    while (count < numPartitions) {
        std::vector<ZXVertexList> newPartitions;
        for (auto& partition : partitions) {
            auto [p1, p2] = _klBiPartition(partition);
            partition = p1;
            newPartitions.push_back(p2);
            if (++count == numPartitions) break;
        }
        partitions.insert(partitions.end(), newPartitions.begin(), newPartitions.end());
    }
    return partitions;
}

std::pair<ZXVertexList, ZXVertexList> _klBiPartition(ZXVertexList vertices) {
    using SwapPair = std::pair<ZXVertex*, ZXVertex*>;

    ZXVertexList partition1 = ZXVertexList();
    ZXVertexList partition2 = ZXVertexList();

    bool toggle = false;
    for (auto v : vertices) {
        if (toggle) {
            partition1.insert(v);
        } else {
            partition2.insert(v);
        }
        toggle = !toggle;
    }

    std::unordered_map<ZXVertex*, int> dValues;
    int cumulativeGain;
    std::stack<SwapPair> swapHistory;
    int bestCumulativeGain;
    size_t bestIteration;
    std::unordered_set<ZXVertex*> lockedVertices;

    auto computeD = [&]() {
        for (auto& v : vertices) {
            int internal_cost = 0;
            int external_cost = 0;

            ZXVertexList& myPartition = partition1.contains(v) ? partition1 : partition2;
            ZXVertexList& otherPartition = partition1.contains(v) ? partition2 : partition1;

            for (auto& [neighbor, edge] : v->getNeighbors()) {
                if (myPartition.contains(neighbor)) {
                    internal_cost++;
                } else if (otherPartition.contains(neighbor)) {
                    external_cost++;
                }
            }

            dValues[v] = external_cost - internal_cost;
        }
    };

    auto swapOnce = [&]() {
        SwapPair bestSwap = {nullptr, nullptr};
        int bestSwapGain = INT_MIN;

        for (auto& v1 : partition1) {
            if (lockedVertices.contains(v1)) continue;
            for (auto& v2 : partition2) {
                if (lockedVertices.contains(v2)) continue;
                int swapGain = dValues[v1] + dValues[v2] - 2 * (v1->isNeighbor(v2) ? 1 : 0);
                if (swapGain > bestSwapGain) {
                    bestSwapGain = swapGain;
                    bestSwap = {v1, v2};
                }
            }
        }

        auto [swap1, swap2] = bestSwap;
        partition1.erase(swap1);
        partition2.erase(swap2);
        partition1.insert(swap2);
        partition2.insert(swap1);
        lockedVertices.insert(swap1);
        lockedVertices.insert(swap2);

        for (auto& v1 : partition1) {
            if (lockedVertices.contains(v1)) continue;
            dValues[v1] += 2 * (v1->isNeighbor(swap1) ? 1 : 0) - 2 * (v1->isNeighbor(swap2) ? 1 : 0);
        }
        for (auto& v2 : partition2) {
            if (lockedVertices.contains(v2)) continue;
            dValues[v2] += 2 * (v2->isNeighbor(swap2) ? 1 : 0) - 2 * (v2->isNeighbor(swap1) ? 1 : 0);
        }

        cumulativeGain += bestSwapGain;
        swapHistory.push(bestSwap);
        if (cumulativeGain >= bestCumulativeGain) {
            bestCumulativeGain = cumulativeGain;
            bestIteration = swapHistory.size();
        }
    };

    size_t iteration = 0;
    while (true) {
        cumulativeGain = 0;
        swapHistory = std::stack<SwapPair>();
        bestCumulativeGain = INT_MIN;
        bestIteration = 0;
        lockedVertices.clear();
        computeD();

        // OPTIMIZE: decide a better stopping condition
        for (size_t _ = 0; _ < partition1.size() - 1; _++) {
            swapOnce();
        }
        // for (size_t _ = 0; _ < partition1.size() / 2; _++) {
        //     swapOnce();
        // }

        // OPTIMIZE: decide a better stopping condition
        if (bestCumulativeGain <= 0) {
            break;
        }
        // if (bestCumulativeGain <= partition1.size() / 10) {
        //     break;
        // }

        // NOTE: undo until best iteration
        while (swapHistory.size() > bestIteration) {
            auto [swap1, swap2] = swapHistory.top();
            swapHistory.pop();
            partition1.erase(swap2);
            partition2.erase(swap1);
            partition1.insert(swap1);
            partition2.insert(swap2);
        }
    }

    return std::make_pair(partition1, partition2);
}
