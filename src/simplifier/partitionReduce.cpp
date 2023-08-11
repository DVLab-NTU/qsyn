#include <algorithm>

#include "simplify.h"
#include "zxPartition.h"
#include "zxRulesTemplate.hpp"

void scopedFullReduce(ZXGraph* graph, const ZXVertexList& scope);
void scopedDynamicReduce(ZXGraph* graph, const ZXVertexList& scope);
int scopedInteriorCliffordSimp(ZXGraph* graph, const ZXVertexList& scope);
int scopedCliffordSimp(ZXGraph* graph, const ZXVertexList& scope);

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void Simplifier::partitionReduce(size_t numPartitions, size_t iterations = 1) {
    // ZXGraph _copiedGraph = *_simpGraph;
    // {
    //     Simplifier simplifier = Simplifier(&_copiedGraph);
    //     simplifier.fullReduce();
    // }
    // size_t tOptimal = _copiedGraph.TCount();

    std::vector<ZXVertexList> partitions = klPartition(*_simpGraph, numPartitions);
    auto [subgraphs, cuts] = _simpGraph->createSubgraphs(partitions);

    ZXVertexList cutPartition;
    for (auto& [b1, b2, _] : cuts) {
        cutPartition.insert(b1->getFirstNeighbor().first);
        cutPartition.insert(b2->getFirstNeighbor().first);
    }

    for (auto& graph : subgraphs) {
        Simplifier simplifier(graph);
        simplifier.dynamicReduce();
    }
    ZXGraph* tempGraph = ZXGraph::fromSubgraphs(subgraphs, cuts);
    _simpGraph->swap(*tempGraph);
    delete tempGraph;

    {
        std::vector<ZXVertex*> frontier(cutPartition.begin(), cutPartition.end());
        for (size_t degree = 0; degree < 1; degree++) {
            std::vector<ZXVertex*> new_frontier;
            for (auto& v : frontier) {
                for (auto& [neighbor, _] : v->getNeighbors()) {
                    if (cutPartition.contains(neighbor)) continue;
                    new_frontier.push_back(neighbor);
                    cutPartition.insert(neighbor);
                }
            }
            frontier = new_frontier;
        }
    }

    scopedDynamicReduce(_simpGraph, cutPartition);
}

void scopedDynamicReduce(ZXGraph* graph, const ZXVertexList& scope) {
    ZXGraph _copiedGraph = *graph;
    scopedFullReduce(&_copiedGraph, scope);
    size_t tOptimal = _copiedGraph.TCount();

    Simplifier simplifier(graph);

    int a1 = scopedInteriorCliffordSimp(graph, scope);

    if (a1 == -1) {
        return;
    }

    int a2 = simplifier.scopedSimplify(PivotGadgetRule(), scope);
    if (a2 == -1 && tOptimal == graph->TCount()) {
        return;
    }

    while (!cli.stop_requested()) {
        int a3 = scopedCliffordSimp(graph, scope);
        if (a3 == -1 && tOptimal == graph->TCount()) {
            return;
        }

        int a4 = simplifier.scopedSimplify(PhaseGadgetRule(), scope);
        if (a4 == -1 && tOptimal == graph->TCount()) {
            return;
        }

        int a5 = scopedInteriorCliffordSimp(graph, scope);
        if (a5 == -1 && tOptimal == graph->TCount()) {
            return;
        }

        int a6 = simplifier.scopedSimplify(PivotGadgetRule(), scope);
        if (a6 == -1 && tOptimal == graph->TCount()) {
            return;
        }

        if (a4 + a6 == 0) break;
    }
}

void scopedFullReduce(ZXGraph* graph, const ZXVertexList& scope) {
    Simplifier simplifier(graph);

    scopedInteriorCliffordSimp(graph, scope);
    simplifier.scopedSimplify(PivotGadgetRule(), scope);
    while (!cli.stop_requested()) {
        simplifier.cliffordSimp();
        int i = simplifier.scopedSimplify(PhaseGadgetRule(), scope);
        if (i == -1) i = 0;
        scopedInteriorCliffordSimp(graph, scope);
        int j = simplifier.scopedSimplify(PivotGadgetRule(), scope);
        if (j == -1) j = 0;
        if (i + j == 0) break;
    }
}

int scopedInteriorCliffordSimp(ZXGraph* graph, const ZXVertexList& scope) {
    Simplifier simplifier(graph);

    simplifier.scopedSimplify(SpiderFusionRule(), scope);
    simplifier.toGraph();
    int i = 0;
    while (true) {
        int i1 = simplifier.scopedSimplify(IdRemovalRule(), scope);
        if (i1 == -1) return -1;
        int i2 = simplifier.scopedSimplify(SpiderFusionRule(), scope);
        if (i2 == -1) return -1;
        int i3 = simplifier.scopedSimplify(PivotRule(), scope);
        if (i3 == -1) return -1;
        int i4 = simplifier.scopedSimplify(LocalComplementRule(), scope);
        if (i4 == -1) return -1;
        if (i1 + i2 + i3 + i4 == 0) break;
        i += 1;
    }
    return i;
}

int scopedCliffordSimp(ZXGraph* graph, const ZXVertexList& scope) {
    Simplifier simplifier(graph);

    int i = 0;
    while (true) {
        int i1 = scopedInteriorCliffordSimp(graph, scope);
        if (i1 == -1) return -1;
        i += i1;
        int i2 = simplifier.scopedSimplify(PivotBoundaryRule(), scope);
        if (i2 == -1) return -1;
        if (i2 == 0) break;
    }
    return i;
}
