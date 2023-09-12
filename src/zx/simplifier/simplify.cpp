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

using namespace std;
extern size_t VERBOSE;

// Basic rules simplification

/**
 * @brief Perform Bialgebra Rule
 *
 * @return int
 */
int Simplifier::bialgebra_simp() {
    return simplify(BialgebraRule());
}

/**
 * @brief Perform State Copy Rule
 *
 * @return int
 */
int Simplifier::state_copy_simp() {
    return simplify(StateCopyRule());
}

/**
 * @brief Perform Gadget Rule
 *
 * @return int
 */
int Simplifier::phase_gadget_simp() {
    return simplify(PhaseGadgetRule());
}

/**
 * @brief Perform Hadamard Fusion Rule
 *
 * @return int
 */
int Simplifier::hadamard_fusion_simp() {
    return simplify(HadamardFusionRule());
}

/**
 * @brief Perform Hadamard Rule
 *
 * @return int
 */
int Simplifier::hadamard_rule_simp() {
    return hadamard_simplify(HadamardRule());
}

/**
 * @brief Perform Identity Removal Rule
 *
 * @return int
 */
int Simplifier::identity_removal_simp() {
    return simplify(IdentityRemovalRule());
}

/**
 * @brief Perform Local Complementation Rule
 *
 * @return int
 */
int Simplifier::local_complement_simp() {
    return simplify(LocalComplementRule());
}

/**
 * @brief Perform Pivot Rule
 *
 * @return int
 */
int Simplifier::pivot_simp() {
    return simplify(PivotRule());
}

/**
 * @brief Perform Pivot Boundary Rule
 *
 * @return int
 */
int Simplifier::pivot_boundary_simp() {
    return simplify(PivotBoundaryRule());
}

/**
 * @brief Perform Pivot Gadget Rule
 *
 * @return int
 */
int Simplifier::pivot_gadget_simp() {
    return simplify(PivotGadgetRule());
}

/**
 * @brief Perform Spider Fusion Rule
 *
 * @return int
 */
int Simplifier::spider_fusion_simp() {
    return simplify(SpiderFusionRule());
}

// action

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
 * @return int
 */
int Simplifier::interior_clifford_simp() {
    this->spider_fusion_simp();
    to_z_graph();
    int i = 0;
    while (true) {
        int i1 = this->identity_removal_simp();
        if (i1 == -1) return -1;
        int i2 = this->spider_fusion_simp();
        if (i2 == -1) return -1;
        int i3 = this->pivot_simp();
        if (i3 == -1) return -1;
        int i4 = this->local_complement_simp();
        if (i4 == -1) return -1;
        if (i1 + i2 + i3 + i4 == 0) break;
        i += 1;
    }
    return i;
}

/**
 * @brief Perform `interior_clifford` and `pivot_boundary` iteratively until no pivot_boundary candidate is found
 *
 * @return int
 */
int Simplifier::clifford_simp() {
    int i = 0;
    while (true) {
        int i1 = this->interior_clifford_simp();
        if (i1 == -1) return -1;
        i += i1;
        int i2 = this->pivot_boundary_simp();
        if (i2 == -1) return -1;
        if (i2 == 0) break;
    }
    return i;
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
        int i = this->phase_gadget_simp();
        if (i == -1) i = 0;
        this->interior_clifford_simp();
        int j = this->pivot_gadget_simp();
        if (j == -1) j = 0;
        if (i + j == 0) break;
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
    cout << endl
         << "Full Reduce:";
    // to obtain the T-optimal
    Simplifier simp = Simplifier(&copied_graph);
    simp.full_reduce();
    size_t t_optimal = copied_graph.t_count();

    cout << endl
         << "Dynamic Reduce:";
    _recipe.clear();
    dynamic_reduce(t_optimal);
}

/**
 * @brief Do full reduce until the T-count is equal to the T-optimal while maintaining the lowest possible density
 *
 * @param tOptimal the target optimal T-count
 */
void Simplifier::dynamic_reduce(size_t optimal_t_count) {
    cout << " (T-optimal: " << optimal_t_count << ")";

    int a1 = this->interior_clifford_simp();

    if (a1 == -1) {
        this->print_recipe();
        return;
    }

    int a2 = this->pivot_gadget_simp();
    if (a2 == -1 && optimal_t_count == _simp_graph->t_count()) {
        this->print_recipe();
        return;
    }

    while (!stop_requested()) {
        int a3 = this->clifford_simp();
        if (a3 == -1 && optimal_t_count == _simp_graph->t_count()) {
            this->print_recipe();
            return;
        }

        int a4 = this->phase_gadget_simp();
        if (a4 == -1 && optimal_t_count == _simp_graph->t_count()) {
            this->print_recipe();
            return;
        }

        int a5 = this->interior_clifford_simp();
        if (a5 == -1 && optimal_t_count == _simp_graph->t_count()) {
            this->print_recipe();
            return;
        }

        int a6 = this->pivot_gadget_simp();
        if (a6 == -1 && optimal_t_count == _simp_graph->t_count()) {
            this->print_recipe();
            return;
        }

        if (a4 + a6 == 0) break;
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
        int i = this->phase_gadget_simp();
        this->interior_clifford_simp();
        int j = this->pivot_gadget_simp();
        this->state_copy_simp();
        if (i + j == 0) break;
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
        cout << "\nAll rules applied:\n";
        unordered_set<string> rules;
        for (size_t i = 0; i < _recipe.size(); i++) {
            if (get<1>(_recipe[i]).size() != 0) {
                if (rules.find(get<0>(_recipe[i])) == rules.end()) {
                    cout << "(" << rules.size() + 1 << ") " << get<0>(_recipe[i]) << endl;
                    rules.insert(get<0>(_recipe[i]));
                }
            }
        }
    } else if (VERBOSE <= 3) {
        cout << "\nAll rules applied in order:\n";
        for (size_t i = 0; i < _recipe.size(); i++) {
            if (get<1>(_recipe[i]).size() != 0) {
                cout << setw(30) << left << get<0>(_recipe[i]) << get<1>(_recipe[i]).size() << " iterations." << endl;
                if (VERBOSE == 3) {
                    for (size_t j = 0; j < get<1>(_recipe[i]).size(); j++) {
                        cout << "  " << j + 1 << ") " << get<1>(_recipe[i])[j] << " matches" << endl;
                    }
                }
            }
        }
    }
}
