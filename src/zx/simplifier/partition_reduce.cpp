#include <algorithm>

#include "./simplify.hpp"
#include "./zx_rules_template.hpp"
#include "zx/zx_partition.hpp"

void scoped_full_reduce(ZXGraph* graph, ZXVertexList const& scope);
void scoped_dynamic_reduce(ZXGraph* graph, ZXVertexList const& scope);
int scoped_interior_clifford_simp(ZXGraph* graph, ZXVertexList const& scope);
int scoped_clifford_simp(ZXGraph* graph, ZXVertexList const& scope);

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void Simplifier::partition_reduce(size_t n_partitions, size_t iterations = 1) {
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

    int a1 = scoped_interior_clifford_simp(graph, scope);

    if (a1 == -1) {
        return;
    }

    int a2 = simplifier.scoped_simplify(PivotGadgetRule(), scope);
    if (a2 == -1 && t_optimal == graph->t_count()) {
        return;
    }

    while (!stop_requested()) {
        int a3 = scoped_clifford_simp(graph, scope);
        if (a3 == -1 && t_optimal == graph->t_count()) {
            return;
        }

        int a4 = simplifier.scoped_simplify(PhaseGadgetRule(), scope);
        if (a4 == -1 && t_optimal == graph->t_count()) {
            return;
        }

        int a5 = scoped_interior_clifford_simp(graph, scope);
        if (a5 == -1 && t_optimal == graph->t_count()) {
            return;
        }

        int a6 = simplifier.scoped_simplify(PivotGadgetRule(), scope);
        if (a6 == -1 && t_optimal == graph->t_count()) {
            return;
        }

        if (a4 + a6 == 0) break;
    }
}

void scoped_full_reduce(ZXGraph* graph, ZXVertexList const& scope) {
    Simplifier simplifier = Simplifier(graph);

    scoped_interior_clifford_simp(graph, scope);
    simplifier.scoped_simplify(PivotGadgetRule(), scope);
    while (!stop_requested()) {
        simplifier.clifford_simp();
        int i = simplifier.scoped_simplify(PhaseGadgetRule(), scope);
        if (i == -1) i = 0;
        scoped_interior_clifford_simp(graph, scope);
        int j = simplifier.scoped_simplify(PivotGadgetRule(), scope);
        if (j == -1) j = 0;
        if (i + j == 0) break;
    }
}

int scoped_interior_clifford_simp(ZXGraph* graph, ZXVertexList const& scope) {
    Simplifier simplifier = Simplifier(graph);

    simplifier.scoped_simplify(SpiderFusionRule(), scope);
    simplifier.to_z_graph();
    int i = 0;
    while (true) {
        int i1 = simplifier.scoped_simplify(IdentityRemovalRule(), scope);
        if (i1 == -1) return -1;
        int i2 = simplifier.scoped_simplify(SpiderFusionRule(), scope);
        if (i2 == -1) return -1;
        int i3 = simplifier.scoped_simplify(PivotRule(), scope);
        if (i3 == -1) return -1;
        int i4 = simplifier.scoped_simplify(LocalComplementRule(), scope);
        if (i4 == -1) return -1;
        if (i1 + i2 + i3 + i4 == 0) break;
        i += 1;
    }
    return i;
}

int scoped_clifford_simp(ZXGraph* graph, ZXVertexList const& scope) {
    Simplifier simplifier = Simplifier(graph);

    int i = 0;
    while (true) {
        int i1 = scoped_interior_clifford_simp(graph, scope);
        if (i1 == -1) return -1;
        i += i1;
        int i2 = simplifier.scoped_simplify(PivotBoundaryRule(), scope);
        if (i2 == -1) return -1;
        if (i2 == 0) break;
    }
    return i;
}
