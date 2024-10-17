/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./simplify.hpp"

#include <cstddef>

#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

namespace qsyn::zx::simplify {

void report_simplification_result(
    std::string_view rule_name, std::span<size_t> match_counts) {
    spdlog::log(
        !match_counts.empty() ? spdlog::level::info : spdlog::level::trace,
        "{:<28} {:>2} iterations, total {:>4} matches",
        rule_name,
        match_counts.size(),
        std::reduce(std::begin(match_counts), std::end(match_counts), 0));

    for (auto i : std::views::iota(0ul, match_counts.size())) {
        spdlog::log(
            spdlog::level::debug,
            "{:>4}) {} matches",
            i + 1, match_counts[i]);
    }
}

// Basic rules simplification
size_t bialgebra_simp(ZXGraph& g) {
    return simplify(g, BialgebraRule());
}

size_t state_copy_simp(ZXGraph& g) {
    return simplify(g, StateCopyRule());
}

size_t phase_gadget_simp(ZXGraph& g) {
    return simplify(g, PhaseGadgetRule());
}

size_t hadamard_fusion_simp(ZXGraph& g) {
    return simplify(g, HadamardFusionRule());
}

size_t hadamard_rule_simp(ZXGraph& g) {
    return hadamard_simplify(g, HadamardRule());
}

size_t identity_removal_simp(ZXGraph& g) {
    return simplify(g, IdentityRemovalRule());
}

size_t local_complement_simp(ZXGraph& g) {
    return simplify(g, LocalComplementRule());
}

size_t pivot_simp(ZXGraph& g) {
    return simplify(g, PivotRule());
}

size_t pivot_boundary_simp(ZXGraph& g) {
    return simplify(g, PivotBoundaryRule());
}

size_t pivot_gadget_simp(ZXGraph& g) {
    return simplify(g, PivotGadgetRule());
}

size_t spider_fusion_simp(ZXGraph& g) {
    return simplify(g, SpiderFusionRule());
}

/**
 * @brief Turn X-spiders into Z-spiders and toggle edges accordingly
 *
 */
void to_z_graph(ZXGraph& g) {
    for (auto& v : g.get_vertices()) {
        if (v->is_x()) {
            toggle_vertex(g, v->get_id());
        }
    }
}

/**
 * @brief Turn Z-spiders into X-spiders and toggle edges accordingly
 *
 */
void to_x_graph(ZXGraph& g) {
    for (auto& v : g.get_vertices()) {
        if (v->is_z()) {
            toggle_vertex(g, v->get_id());
        }
    }
}

/**
 * @brief Make the ZXGraph graph-like, i.e., all vertices are Z-spiders or
 *        boundary vertices and all Z-Z edges are Hadamard edges
 *
 * @param g
 */
void to_graph_like(ZXGraph& g) {
    spider_fusion_simp(g);
    to_z_graph(g);
}

/**
 * @brief Remove Clifford vertices in the interior of the graph iteratively
 *        until no more can be removed
 *
 * @return the number of iterations
 */
size_t interior_clifford_simp(ZXGraph& g) {
    to_graph_like(g);
    for (size_t iterations = 0; !stop_requested(); iterations++) {
        auto const i1 = identity_removal_simp(g);
        auto const i2 = spider_fusion_simp(g);
        auto const i3 = pivot_simp(g);
        auto const i4 = local_complement_simp(g);
        if (i1 + i2 + i3 + i4 == 0) return iterations;
    }
    return 0;
}

/**
 * @brief Perform `interior_clifford` and `pivot_boundary` iteratively
 *        until no pivot_boundary candidate is found
 *
 * @return the number of iterations
 */
size_t clifford_simp(ZXGraph& g) {
    size_t iterations = 0;
    while (true) {
        auto const i1 = interior_clifford_simp(g);
        iterations += i1;
        auto const i2 = pivot_boundary_simp(g);
        if (i2 == 0) break;
    }
    return iterations;
}

/**
 * @brief Perform full reduction on the graph
 *
 */
void full_reduce(ZXGraph& g) {
    interior_clifford_simp(g);
    pivot_gadget_simp(g);
    while (!stop_requested()) {
        clifford_simp(g);
        auto i1 = phase_gadget_simp(g);
        interior_clifford_simp(g);
        auto i2 = pivot_gadget_simp(g);
        if (i1 + i2 == 0) break;
    }
}

/**
 * @brief Perform a full reduce on the graph to determine the optimal T-count
 *        and then perform a dynamic reduce
 *
 */
void dynamic_reduce(ZXGraph& g) {
    // copy the graph's structure
    hadamard_rule_simp(g);
    ZXGraph copied_graph = g;
    spdlog::info("Full Reduce:");
    // to obtain the T-optimal
    full_reduce(copied_graph);
    auto t_optimal = t_count(copied_graph);

    spdlog::info("Dynamic Reduce: (T-optimal: {})", t_optimal);
    dynamic_reduce(g, t_optimal);
}

/**
 * @brief Do full reduce until the T-count is equal to the T-optimal
 *        while maintaining the lowest possible density
 *
 * @param optimal_t_count the target optimal T-count
 */
void dynamic_reduce(ZXGraph& g, size_t optimal_t_count) {
    interior_clifford_simp(g);
    pivot_gadget_simp(g);
    if (t_count(g) == optimal_t_count) {
        return;
    }

    while (!stop_requested()) {
        clifford_simp(g);
        if (t_count(g) == optimal_t_count) {
            break;
        }
        auto i1 = phase_gadget_simp(g);
        if (t_count(g) == optimal_t_count) {
            break;
        }
        interior_clifford_simp(g);
        if (t_count(g) == optimal_t_count) {
            break;
        }
        auto i2 = pivot_gadget_simp(g);
        if (t_count(g) == optimal_t_count) {
            break;
        }
        if (i1 + i2 == 0) break;
    }
}

/**
 * @brief The reduce strategy with `state_copy` and `full_reduce`
 *
 */
void symbolic_reduce(ZXGraph& g) {
    interior_clifford_simp(g);
    pivot_gadget_simp(g);
    state_copy_simp(g);
    while (!stop_requested()) {
        clifford_simp(g);
        auto i1 = phase_gadget_simp(g);
        interior_clifford_simp(g);
        auto i2 = pivot_gadget_simp(g);
        state_copy_simp(g);
        if (i1 + i2 == 0) break;
    }
    to_x_graph(g);
}

}  // namespace qsyn::zx::simplify
