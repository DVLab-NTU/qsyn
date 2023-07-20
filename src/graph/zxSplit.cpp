/****************************************************************************
  FileName     [ zxSplit.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Implements the split function ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iomanip>
#include <stack>
#include <unordered_map>

#include "zxDef.h"
#include "zxGraph.h"

using SwapPair = std::pair<ZXVertex*, ZXVertex*>;

std::pair<ZXVertexList, ZXVertexList> _klPartition(ZXVertexList vertices);

std::vector<ZXVertexList> klPartition(ZXGraph* graph, size_t rounds) {
    std::vector<ZXVertexList> partitions = {graph->getVertices()};
    for (size_t i = 0; i < rounds; i++) {
        std::vector<ZXVertexList> newPartitions;
        for (auto& partition : partitions) {
            auto [p1, p2] = _klPartition(partition);
            partition = p1;
            newPartitions.push_back(p2);
        }
        partitions.insert(partitions.end(), newPartitions.begin(), newPartitions.end());
    }
    return partitions;
}

std::pair<ZXVertexList, ZXVertexList> _klPartition(ZXVertexList vertices) {
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

        std::cerr << "Iteration " << iteration++ << ": " << bestCumulativeGain << std::endl;

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
