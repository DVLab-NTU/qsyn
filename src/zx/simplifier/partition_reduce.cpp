#include <algorithm>
#include <cstddef>

#include "./simplify.hpp"
#include "./zx_rules_template.hpp"
#include "zx/zx_partition.hpp"

using namespace qsyn::zx;

void scoped_full_reduce(ZXGraph* graph, ZXVertexList const& scope);
void scoped_dynamic_reduce(ZXGraph* graph, ZXVertexList const& scope);
size_t scoped_interior_clifford_simp(ZXGraph* graph, ZXVertexList const& scope);
size_t scoped_clifford_simp(ZXGraph* graph, ZXVertexList const& scope);

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void Simplifier::partition_reduce(size_t n_partitions, size_t /*iterations*/) {
    ZXGraph copied_graph = *_simp_graph;
    {
        Simplifier simplifier = Simplifier(&copied_graph);
        simplifier.full_reduce();
    }
    size_t t_optimal = copied_graph.t_count();

    std::vector<ZXVertexList> partitions = kl_partition(*_simp_graph, n_partitions);
    auto [subgraphs, cuts] = _simp_graph->create_subgraphs(partitions);

    for (auto& graph : subgraphs) {
        Simplifier simplifier = Simplifier(graph);
        simplifier.dynamic_reduce();
    }

    ZXGraph* temp_graph = ZXGraph::from_subgraphs(subgraphs, cuts);
    _simp_graph->swap(*temp_graph);
    delete temp_graph;

    spider_fusion_simp();
}

void scoped_dynamic_reduce(ZXGraph* graph, ZXVertexList const& scope) {
    ZXGraph copied_graph = *graph;
    scoped_full_reduce(&copied_graph, scope);
    size_t t_optimal = copied_graph.t_count();

    Simplifier simplifier = Simplifier(graph);

    scoped_interior_clifford_simp(graph, scope);
    simplifier.scoped_simplify(PivotGadgetRule(), scope);

    while (!stop_requested()) {
        scoped_clifford_simp(graph, scope);
        size_t i1 = simplifier.scoped_simplify(PhaseGadgetRule(), scope);
        scoped_interior_clifford_simp(graph, scope);
        size_t i2 = simplifier.scoped_simplify(PivotGadgetRule(), scope);
        if (i1 + i2 == 0) break;
    }
}

void scoped_full_reduce(ZXGraph* graph, ZXVertexList const& scope) {
    Simplifier simplifier = Simplifier(graph);

    scoped_interior_clifford_simp(graph, scope);
    simplifier.scoped_simplify(PivotGadgetRule(), scope);
    while (!stop_requested()) {
        simplifier.clifford_simp();
        size_t i1 = simplifier.scoped_simplify(PhaseGadgetRule(), scope);
        scoped_interior_clifford_simp(graph, scope);
        size_t i2 = simplifier.scoped_simplify(PivotGadgetRule(), scope);
        if (i1 + i2 == 0) break;
    }
}

size_t scoped_interior_clifford_simp(ZXGraph* graph, ZXVertexList const& scope) {
    Simplifier simplifier = Simplifier(graph);

    simplifier.scoped_simplify(SpiderFusionRule(), scope);
    simplifier.to_z_graph();
    size_t iterations = 0;
    while (true) {
        size_t i1 = simplifier.scoped_simplify(IdentityRemovalRule(), scope);
        size_t i2 = simplifier.scoped_simplify(SpiderFusionRule(), scope);
        size_t i3 = simplifier.scoped_simplify(PivotRule(), scope);
        size_t i4 = simplifier.scoped_simplify(LocalComplementRule(), scope);
        if (i1 + i2 + i3 + i4 == 0) break;
        iterations += 1;
    }
    return iterations;
}

size_t scoped_clifford_simp(ZXGraph* graph, ZXVertexList const& scope) {
    Simplifier simplifier = Simplifier(graph);

    size_t iteration = 0;
    while (true) {
        size_t i1 = scoped_interior_clifford_simp(graph, scope);
        iteration += i1;
        size_t i2 = simplifier.scoped_simplify(PivotBoundaryRule(), scope);
        if (i2 == 0) break;
    }
    return iteration;
}
