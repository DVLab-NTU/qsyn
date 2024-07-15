#include <algorithm>
#include <cstddef>
#include <util/util.hpp>

#include "./rules/zx_rules_template.hpp"
#include "./simplify.hpp"
#include "zx/zx_partition.hpp"
#include "zx/zxgraph.hpp"

using namespace qsyn::zx;

void scoped_full_reduce(ZXGraph& graph, ZXVertexList const& scope);
void scoped_dynamic_reduce(ZXGraph& graph, ZXVertexList const& scope);
size_t scoped_interior_clifford_simp(ZXGraph& graph, ZXVertexList const& scope);
size_t scoped_clifford_simp(ZXGraph& graph, ZXVertexList const& scope);

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds (experimental)
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void Simplifier::partition_reduce(ZXGraph& g, size_t n_partitions) {
    hadamard_rule_simp(g);
    auto const partitions        = kl_partition(g, n_partitions);
    auto const [subgraphs, cuts] = g.create_subgraphs(partitions);

    for (auto& graph : subgraphs) {
        auto simplifier = Simplifier();
        simplifier.dynamic_reduce(*graph);
    }

    ZXGraph* const temp_graph = ZXGraph::from_subgraphs(subgraphs, cuts);
    g.swap(*temp_graph);
    delete temp_graph;

    spider_fusion_simp(g);
}

void scoped_dynamic_reduce(ZXGraph& graph, ZXVertexList const& scope) {
    ZXGraph copied_graph = graph;
    scoped_full_reduce(copied_graph, scope);
    auto const optimal_t_count = copied_graph.t_count();

    auto simplifier = Simplifier();

    scoped_interior_clifford_simp(graph, scope);
    simplifier.scoped_simplify(graph, PivotGadgetRule(), scope);
    if (graph.t_count() <= optimal_t_count) return;

    while (!stop_requested()) {
        scoped_clifford_simp(graph, scope);
        if (graph.t_count() <= optimal_t_count) return;
        auto const i1 = simplifier.scoped_simplify(graph, PhaseGadgetRule(), scope);
        if (graph.t_count() <= optimal_t_count) return;
        scoped_interior_clifford_simp(graph, scope);
        if (graph.t_count() <= optimal_t_count) return;
        auto const i2 = simplifier.scoped_simplify(graph, PivotGadgetRule(), scope);
        if (graph.t_count() <= optimal_t_count) return;
        if (i1 + i2 == 0) break;
    }
}

void scoped_full_reduce(ZXGraph& graph, ZXVertexList const& scope) {
    auto simplifier = Simplifier();

    scoped_interior_clifford_simp(graph, scope);
    simplifier.scoped_simplify(graph, PivotGadgetRule(), scope);
    while (!stop_requested()) {
        scoped_clifford_simp(graph, scope);
        auto const i1 = simplifier.scoped_simplify(graph, PhaseGadgetRule(), scope);
        scoped_interior_clifford_simp(graph, scope);
        auto const i2 = simplifier.scoped_simplify(graph, PivotGadgetRule(), scope);
        if (i1 + i2 == 0) break;
    }
}

size_t scoped_interior_clifford_simp(ZXGraph& graph, ZXVertexList const& scope) {
    auto simplifier = Simplifier();

    simplifier.scoped_simplify(graph, SpiderFusionRule(), scope);
    simplifier.to_z_graph(graph);
    for (size_t iterations = 0; !stop_requested(); iterations++) {
        auto const i1 = simplifier.scoped_simplify(graph, IdentityRemovalRule(), scope);
        auto const i2 = simplifier.scoped_simplify(graph, SpiderFusionRule(), scope);
        auto const i3 = simplifier.scoped_simplify(graph, PivotRule(), scope);
        auto const i4 = simplifier.scoped_simplify(graph, LocalComplementRule(), scope);
        if (i1 + i2 + i3 + i4 == 0) return iterations;
    }
    DVLAB_UNREACHABLE("This line should not be reached");
    return 0;
}

size_t scoped_clifford_simp(ZXGraph& graph, ZXVertexList const& scope) {
    auto simplifier = Simplifier();

    size_t iteration = 0;
    while (true) {
        auto i1 = scoped_interior_clifford_simp(graph, scope);
        iteration += i1;
        auto i2 = simplifier.scoped_simplify(graph, PivotBoundaryRule(), scope);
        if (i2 == 0) break;
    }
    return iteration;
}
