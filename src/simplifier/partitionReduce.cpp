#include "simplify.h"
#include "zxPartition.h"

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void Simplifier::partitionReduce(size_t numPartitions, size_t iterations = 1) {
    ZXGraph _copiedGraph = *_simpGraph;
    Simplifier simp = Simplifier(&_copiedGraph);
    simp.fullReduce();
    size_t tOptimal = _copiedGraph.TCount();

    std::vector<ZXVertexList> partitions = klPartition(*_simpGraph, numPartitions);
    auto [subgraphs, cuts] = _simpGraph->createSubgraphs(partitions);
    for (auto& graph : subgraphs) {
        Simplifier simplifier(graph);
        simplifier.dynamicReduce();
    }
    ZXGraph* tempGraph = ZXGraph::fromSubgraphs(subgraphs, cuts);
    _simpGraph->swap(*tempGraph);
    delete tempGraph;

    simp = Simplifier(_simpGraph);
    simp.dynamicReduce(tOptimal);
    return;
}
