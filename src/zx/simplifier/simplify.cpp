/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./simplify.hpp"

#include <cstddef>

#include "util/util.hpp"
#include "zx/zx_def.hpp"
#include "zx/zx_partition.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_mgr.hpp"

namespace qsyn::zx {

void report_simplification_result(std::string_view rule_name, std::span<size_t> match_counts) {
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
size_t Simplifier::bialgebra_simp(ZXGraph& g) {
    return simplify(g, BialgebraRule());
}

size_t Simplifier::state_copy_simp(ZXGraph& g) {
    return simplify(g, StateCopyRule());
}

size_t Simplifier::phase_gadget_simp(ZXGraph& g) {
    return simplify(g, PhaseGadgetRule());
}

size_t Simplifier::hadamard_fusion_simp(ZXGraph& g) {
    return simplify(g, HadamardFusionRule());
}

size_t Simplifier::hadamard_rule_simp(ZXGraph& g) {
    return hadamard_simplify(g, HadamardRule());
}

size_t Simplifier::identity_removal_simp(ZXGraph& g) {
    return simplify(g, IdentityRemovalRule());
}

size_t Simplifier::local_complement_simp(ZXGraph& g) {
    return simplify(g, LocalComplementRule());
}

size_t Simplifier::pivot_simp(ZXGraph& g) {
    return simplify(g, PivotRule());
}

size_t Simplifier::pivot_boundary_simp(ZXGraph& g) {
    return simplify(g, PivotBoundaryRule());
}

size_t Simplifier::pivot_gadget_simp(ZXGraph& g) {
    return simplify(g, PivotGadgetRule());
}

size_t Simplifier::spider_fusion_simp(ZXGraph& g) {
    return simplify(g, SpiderFusionRule());
}

/**
 * @brief Turn every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 *
 */
void Simplifier::to_z_graph(ZXGraph& g) {
    for (auto& v : g.get_vertices()) {
        if (v->get_type() == VertexType::x) {
            g.toggle_vertex(v);
        }
    }
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 *
 */
void Simplifier::to_x_graph(ZXGraph& g) {
    for (auto& v : g.get_vertices()) {
        if (v->get_type() == VertexType::z) {
            g.toggle_vertex(v);
        }
    }
}

/**
 * @brief Keep doing the simplifications `id_removal`, `s_fusion`, `pivot`, `lcomp` until none of them can be applied anymore.
 *
 * @return the number of iterations
 */
size_t Simplifier::interior_clifford_simp(ZXGraph& g) {
    this->spider_fusion_simp(g);
    this->to_z_graph(g);
    for (size_t iterations = 0; !stop_requested(); iterations++) {
        auto const i1 = this->identity_removal_simp(g);
        auto const i2 = this->spider_fusion_simp(g);
        auto const i3 = this->pivot_simp(g);
        auto const i4 = this->local_complement_simp(g);
        if (i1 + i2 + i3 + i4 == 0) return iterations;
    }
    return 0;
}

/**
 * @brief Perform `interior_clifford` and `pivot_boundary` iteratively until no pivot_boundary candidate is found
 *
 * @return int
 */
size_t Simplifier::clifford_simp(ZXGraph& g) {
    size_t iterations = 0;
    while (true) {
        auto const i1 = this->interior_clifford_simp(g);
        iterations += i1;
        auto const i2 = this->pivot_boundary_simp(g);
        if (i2 == 0) break;
    }
    return iterations;
}

/**
 * @brief The main simplification routine
 *
 */
void Simplifier::full_reduce(ZXGraph& g) {
    this->interior_clifford_simp(g);
    this->pivot_gadget_simp(g);
    while (!stop_requested()) {
        this->clifford_simp(g);
        auto i1 = this->phase_gadget_simp(g);
        this->interior_clifford_simp(g);
        auto i2 = this->pivot_gadget_simp(g);
        if (i1 + i2 == 0) break;
    }
}

/**
 * @brief Perform a full reduce on the graph to determine the optimal T-count automatically
 *        and then perform a dynamic reduce
 *
 */
void Simplifier::dynamic_reduce(ZXGraph& g) {
    // copy the graph's structure
    hadamard_rule_simp(g);
    ZXGraph copied_graph = g;
    spdlog::info("Full Reduce:");
    // to obtain the T-optimal
    Simplifier().full_reduce(copied_graph);
    auto t_optimal = copied_graph.t_count();

    spdlog::info("Dynamic Reduce: (T-optimal: {})", t_optimal);
    dynamic_reduce(g, t_optimal);
}

/**
 * @brief Do full reduce until the T-count is equal to the T-optimal while maintaining the lowest possible density
 *
 * @param optimal_t_count the target optimal T-count
 */
void Simplifier::dynamic_reduce(ZXGraph& g, size_t optimal_t_count) {
    this->interior_clifford_simp(g);
    this->pivot_gadget_simp(g);
    if (g.t_count() == optimal_t_count) {
        return;
    }

    while (!stop_requested()) {
        this->clifford_simp(g);
        if (g.t_count() == optimal_t_count) {
            break;
        }
        auto i1 = this->phase_gadget_simp(g);
        if (g.t_count() == optimal_t_count) {
            break;
        }
        this->interior_clifford_simp(g);
        if (g.t_count() == optimal_t_count) {
            break;
        }
        auto i2 = this->pivot_gadget_simp(g);
        if (g.t_count() == optimal_t_count) {
            break;
        }
        if (i1 + i2 == 0) break;
    }
}

/**
 * @brief The reduce strategy with `state_copy` and `full_reduce`
 *
 */
void Simplifier::symbolic_reduce(ZXGraph& g) {
    this->interior_clifford_simp(g);
    this->pivot_gadget_simp(g);
    this->state_copy_simp(g);
    while (!stop_requested()) {
        this->clifford_simp(g);
        auto i1 = this->phase_gadget_simp(g);
        this->interior_clifford_simp(g);
        auto i2 = this->pivot_gadget_simp(g);
        this->state_copy_simp(g);
        if (i1 + i2 == 0) break;
    }
    this->to_x_graph(g);
}

void Simplifier::causal_reduce(ZXGraph& /* */) {
    // TODO: implement causal reduce
}

}  // namespace qsyn::zx
