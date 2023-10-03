/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./simplify.hpp"

#include <cstddef>

#include "./zx_rules_template.hpp"
#include "zx/zx_def.hpp"
#include "zx/zx_partition.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_mgr.hpp"

using namespace qsyn::zx;

// Basic rules simplification
size_t Simplifier::bialgebra_simp() {
    return simplify(BialgebraRule());
}

size_t Simplifier::state_copy_simp() {
    return simplify(StateCopyRule());
}

size_t Simplifier::phase_gadget_simp() {
    return simplify(PhaseGadgetRule());
}

size_t Simplifier::hadamard_fusion_simp() {
    return simplify(HadamardFusionRule());
}

size_t Simplifier::hadamard_rule_simp() {
    return hadamard_simplify(HadamardRule());
}

size_t Simplifier::identity_removal_simp() {
    return simplify(IdentityRemovalRule());
}

size_t Simplifier::local_complement_simp() {
    return simplify(LocalComplementRule());
}

size_t Simplifier::pivot_simp() {
    return simplify(PivotRule());
}

size_t Simplifier::pivot_boundary_simp() {
    return simplify(PivotBoundaryRule());
}

size_t Simplifier::pivot_gadget_simp() {
    return simplify(PivotGadgetRule());
}

size_t Simplifier::spider_fusion_simp() {
    return simplify(SpiderFusionRule());
}

/**
 * @brief Turn every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 *
 */
void Simplifier::to_z_graph() {
    for (auto& v : _simp_graph->get_vertices()) {
        if (v->get_type() == VertexType::x) {
            _simp_graph->toggle_vertex(v);
        }
    }
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 *
 */
void Simplifier::to_x_graph() {
    for (auto& v : _simp_graph->get_vertices()) {
        if (v->get_type() == VertexType::z) {
            _simp_graph->toggle_vertex(v);
        }
    }
}

/**
 * @brief Keep doing the simplifications `id_removal`, `s_fusion`, `pivot`, `lcomp` until none of them can be applied anymore.
 *
 * @return the number of iterations
 */
size_t Simplifier::interior_clifford_simp() {
    this->spider_fusion_simp();
    this->to_z_graph();
    size_t iterations = 0;
    while (true) {
        size_t i1 = this->identity_removal_simp();
        size_t i2 = this->spider_fusion_simp();
        size_t i3 = this->pivot_simp();
        size_t i4 = this->local_complement_simp();
        if (i1 + i2 + i3 + i4 == 0) break;
        iterations++;
    }
    return iterations;
}

/**
 * @brief Perform `interior_clifford` and `pivot_boundary` iteratively until no pivot_boundary candidate is found
 *
 * @return int
 */
size_t Simplifier::clifford_simp() {
    size_t iterations = 0;
    while (true) {
        size_t i1 = this->interior_clifford_simp();
        iterations += i1;
        size_t i2 = this->pivot_boundary_simp();
        if (i2 == 0) break;
    }
    return iterations;
}

/**
 * @brief The main simplification routine
 *
 */
void Simplifier::full_reduce() {
    this->interior_clifford_simp();
    this->pivot_gadget_simp();
    while (!stop_requested()) {
        this->clifford_simp();
        auto i1 = this->phase_gadget_simp();
        this->interior_clifford_simp();
        auto i2 = this->pivot_gadget_simp();
        if (i1 + i2 == 0) break;
    }
}

/**
 * @brief Perform a full reduce on the graph to determine the optimal T-count automatically
 *        and then perform a dynamic reduce
 *
 */
void Simplifier::dynamic_reduce() {
    // copy the graph's structure
    ZXGraph copied_graph = *_simp_graph;
    spdlog::info("Full Reduce:");
    // to obtain the T-optimal
    Simplifier(&copied_graph).full_reduce();
    auto t_optimal = copied_graph.t_count();

    spdlog::info("Dynamic Reduce: (T-optimal: {})", t_optimal);
    dynamic_reduce(t_optimal);
}

/**
 * @brief Do full reduce until the T-count is equal to the T-optimal while maintaining the lowest possible density
 *
 * @param optimal_t_count the target optimal T-count
 */
void Simplifier::dynamic_reduce(size_t optimal_t_count) {
    this->interior_clifford_simp();
    this->pivot_gadget_simp();
    if (_simp_graph->t_count() == optimal_t_count) {
        return;
    }

    while (!stop_requested()) {
        this->clifford_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        auto i1 = this->phase_gadget_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        this->interior_clifford_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        auto i2 = this->pivot_gadget_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        if (i1 + i2 == 0) break;
    }
}

/**
 * @brief The reduce strategy with `state_copy` and `full_reduce`
 *
 */
void Simplifier::symbolic_reduce() {
    this->interior_clifford_simp();
    this->pivot_gadget_simp();
    this->state_copy_simp();
    while (!stop_requested()) {
        this->clifford_simp();
        auto i1 = this->phase_gadget_simp();
        this->interior_clifford_simp();
        auto i2 = this->pivot_gadget_simp();
        this->state_copy_simp();
        if (i1 + i2 == 0) break;
    }
    this->to_x_graph();
}

void Simplifier::_report_simp_result(std::string_view rule_name, std::span<size_t> match_counts) const {
    spdlog::log(
        match_counts.size() > 0 ? spdlog::level::info : spdlog::level::trace,
        "{:<28} {:>2} iterations, total {:>4} matches",
        rule_name,
        match_counts.size(),
        std::accumulate(match_counts.begin(), match_counts.end(), 0));

    for (auto i : std::views::iota(0ul, match_counts.size())) {
        spdlog::log(
            spdlog::level::debug,
            "{:>4}) {} matches",
            i + 1, match_counts[i]);
    }
}