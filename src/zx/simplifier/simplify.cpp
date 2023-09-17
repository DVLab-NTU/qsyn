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

extern size_t VERBOSE;

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
        size_t i1 = this->phase_gadget_simp();
        this->interior_clifford_simp();
        size_t i2 = this->pivot_gadget_simp();
        if (i1 + i2 == 0) break;
    }
    this->print_recipe();
}

/**
 * @brief Perform a full reduce on the graph to determine the optimal T-count automatically
 *        and then perform a dynamic reduce
 *
 */
void Simplifier::dynamic_reduce() {
    // copy the graph's structure
    ZXGraph copied_graph = *_simp_graph;
    std::cout << "\nFull Reduce:";
    // to obtain the T-optimal
    Simplifier simp = Simplifier(&copied_graph);
    simp.full_reduce();
    size_t t_optimal = copied_graph.t_count();

    std::cout << "\nDynamic Reduce:";
    _recipe.clear();
    dynamic_reduce(t_optimal);
}

/**
 * @brief Do full reduce until the T-count is equal to the T-optimal while maintaining the lowest possible density
 *
 * @param tOptimal the target optimal T-count
 */
void Simplifier::dynamic_reduce(size_t optimal_t_count) {
    std::cout << " (T-optimal: " << optimal_t_count << ")";

    this->interior_clifford_simp();
    this->pivot_gadget_simp();
    if (_simp_graph->t_count() == optimal_t_count) {
        this->print_recipe();
        return;
    }

    while (!stop_requested()) {
        this->clifford_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        size_t i1 = this->phase_gadget_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        this->interior_clifford_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        size_t i2 = this->pivot_gadget_simp();
        if (_simp_graph->t_count() == optimal_t_count) {
            break;
        }
        if (i1 + i2 == 0) break;
    }
    this->print_recipe();
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
        size_t i1 = this->phase_gadget_simp();
        this->interior_clifford_simp();
        size_t i2 = this->pivot_gadget_simp();
        this->state_copy_simp();
        if (i1 + i2 == 0) break;
    }
    this->to_x_graph();
}

/**
 * @brief Print recipe of Simplifier
 *
 */
void Simplifier::print_recipe() {
    if (VERBOSE == 0) return;
    if (VERBOSE == 1) {
        std::cout << "\nAll rules applied:\n";
        std::unordered_set<std::string> rules;
        for (size_t i = 0; i < _recipe.size(); i++) {
            if (get<1>(_recipe[i]).size() != 0) {
                if (rules.find(get<0>(_recipe[i])) == rules.end()) {
                    std::cout << "(" << rules.size() + 1 << ") " << get<0>(_recipe[i]) << std::endl;
                    rules.insert(get<0>(_recipe[i]));
                }
            }
        }
    } else if (VERBOSE <= 3) {
        std::cout << "\nAll rules applied in order:\n";
        for (size_t i = 0; i < _recipe.size(); i++) {
            if (get<1>(_recipe[i]).size() != 0) {
                std::cout << std::setw(30) << std::left << get<0>(_recipe[i]) << get<1>(_recipe[i]).size() << " iterations." << std::endl;
                if (VERBOSE == 3) {
                    for (size_t j = 0; j < get<1>(_recipe[i]).size(); j++) {
                        std::cout << "  " << j + 1 << ") " << get<1>(_recipe[i])[j] << " matches" << std::endl;
                    }
                }
            }
        }
    }
}
