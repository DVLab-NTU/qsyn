/****************************************************************************
  FileName     [ simplify.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "simplify.h"

#include <cstddef>
#include <iomanip>
#include <iostream>

#include "cmdParser.h"
#include "extract.h"
#include "gFlow.h"
#include "zxDef.h"
#include "zxGraph.h"  // for ZXGraph
#include "zxGraphMgr.h"
#include "zxPartition.h"
#include "zxRulesTemplate.hpp"

using namespace std;
extern size_t verbose;
extern size_t dmode;
extern CmdParser cli;

/**
 * @brief Perform Bialgebra Rule
 *
 * @return int
 */
int Simplifier::bialgSimp() {
    return new_simp(BialgebraRule());
}

/**
 * @brief Perform State Copy Rule
 *
 * @return int
 */
int Simplifier::copySimp() {
    return new_simp(StateCopyRule());
}

/**
 * @brief Perform Gadget Rule
 *
 * @return int
 */
int Simplifier::gadgetSimp() {
    return new_simp(PhaseGadgetRule());
}

/**
 * @brief Perform Hadamard Fusion Rule
 *
 * @return int
 */
int Simplifier::hfusionSimp() {
    return new_simp(HBoxFusionRule());
}

/**
 * @brief Perform Hadamard Rule
 *
 * @return int
 */
int Simplifier::hruleSimp() {
    return new_hadamard_simp(HadamardRule());
}

/**
 * @brief Perform Identity Removal Rule
 *
 * @return int
 */
int Simplifier::idSimp() {
    return new_simp(IdRemovalRule());
}

/**
 * @brief Perform Local Complementation Rule
 *
 * @return int
 */
int Simplifier::lcompSimp() {
    return new_simp(LocalComplementRule());
}

/**
 * @brief Perform Pivot Rule
 *
 * @return int
 */
int Simplifier::pivotSimp() {
    return new_simp(PivotRule());
}

/**
 * @brief Perform Pivot Boundary Rule
 *
 * @return int
 */
int Simplifier::pivotBoundarySimp() {
    return new_simp(PivotBoundaryRule());
}

/**
 * @brief Perform Pivot Gadget Rule
 *
 * @return int
 */
int Simplifier::pivotGadgetSimp() {
    return new_simp(PivotGadgetRule());
}

// /**
//  * @brief Perform Degadgetize Rule
//  *
//  * @return int
//  */
// int Simplifier::degadgetizeSimp() {
//     this->setRule(make_unique<PivotDegadget>());
//     return this->simp();
// }

/**
 * @brief Perform Spider Fusion Rule
 *
 * @return int
 */
int Simplifier::sfusionSimp() {
    return new_simp(SpiderFusionRule());
}

// action

/**
 * @brief Turn every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 *
 */
void Simplifier::toGraph() {
    for (auto& v : _simpGraph->getVertices()) {
        if (v->getType() == VertexType::X) {
            _simpGraph->toggleVertex(v);
        }
    }
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 *
 */
void Simplifier::toRGraph() {
    for (auto& v : _simpGraph->getVertices()) {
        if (v->getType() == VertexType::Z) {
            _simpGraph->toggleVertex(v);
        }
    }
}

/**
 * @brief Keep doing the simplifications `id_removal`, `s_fusion`, `pivot`, `lcomp` until none of them can be applied anymore.
 *
 * @return int
 */
int Simplifier::interiorCliffordSimp() {
    this->sfusionSimp();
    toGraph();
    int i = 0;
    while (true) {
        int i1 = this->idSimp();
        if (i1 == -1) return -1;
        int i2 = this->sfusionSimp();
        if (i2 == -1) return -1;
        int i3 = this->pivotSimp();
        if (i3 == -1) return -1;
        int i4 = this->lcompSimp();
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
int Simplifier::cliffordSimp() {
    int i = 0;
    while (true) {
        int i1 = this->interiorCliffordSimp();
        if (i1 == -1) return -1;
        i += i1;
        int i2 = this->pivotBoundarySimp();
        if (i2 == -1) return -1;
        if (i2 == 0) break;
    }
    return i;
}

/**
 * @brief The main simplification routine
 *
 */
void Simplifier::fullReduce() {
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    while (!cli.stop_requested()) {
        this->cliffordSimp();
        int i = this->gadgetSimp();
        if (i == -1) i = 0;
        this->interiorCliffordSimp();
        int j = this->pivotGadgetSimp();
        if (j == -1) j = 0;
        if (i + j == 0) break;
    }
    this->printRecipe();
}

/**
 * @brief Perform a full reduce on the graph to determine the optimal T-count automatically
 *        and then perform a dynamic reduce
 *
 */
void Simplifier::dynamicReduce() {
    // copy the graph's structure
    ZXGraph _copiedGraph = *_simpGraph;
    cout << endl
         << "Full Reduce:";
    // to obtain the T-optimal
    Simplifier simp = Simplifier(&_copiedGraph);
    simp.fullReduce();
    size_t tOptimal = _copiedGraph.TCount();

    cout << endl
         << "Dynamic Reduce:";
    _recipe.clear();
    dynamicReduce(tOptimal);
}

/**
 * @brief Do full reduce until the T-count is equal to the T-optimal while maintaining the lowest possible density
 *
 * @param tOptimal the target optimal T-count
 */
void Simplifier::dynamicReduce(size_t tOptimal) {
    cout << " (T-optimal: " << tOptimal << ")";

    int a1 = this->interiorCliffordSimp();

    if (a1 == -1) {
        this->printRecipe();
        return;
    }

    int a2 = this->pivotGadgetSimp();
    if (a2 == -1 && tOptimal == _simpGraph->TCount()) {
        this->printRecipe();
        return;
    }

    while (!cli.stop_requested()) {
        int a3 = this->cliffordSimp();
        if (a3 == -1 && tOptimal == _simpGraph->TCount()) {
            this->printRecipe();
            return;
        }

        int a4 = this->gadgetSimp();
        if (a4 == -1 && tOptimal == _simpGraph->TCount()) {
            this->printRecipe();
            return;
        }

        int a5 = this->interiorCliffordSimp();
        if (a5 == -1 && tOptimal == _simpGraph->TCount()) {
            this->printRecipe();
            return;
        }

        int a6 = this->pivotGadgetSimp();
        if (a6 == -1 && tOptimal == _simpGraph->TCount()) {
            this->printRecipe();
            return;
        }

        if (a4 + a6 == 0) break;
    }
    this->printRecipe();
}

/**
 * @brief The reduce strategy with `state_copy` and `full_reduce`
 *
 */
void Simplifier::symbolicReduce() {
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    this->copySimp();
    while (!cli.stop_requested()) {
        this->cliffordSimp();
        int i = this->gadgetSimp();
        this->interiorCliffordSimp();
        int j = this->pivotGadgetSimp();
        this->copySimp();
        if (i + j == 0) break;
    }
    this->toRGraph();
}

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void Simplifier::partitionReduce(size_t numPartitions, size_t iterations = 1) {
    for (size_t n = numPartitions; n >= 1; n /= 2) {
        auto [subgraphs, cuts] = _simpGraph->createSubgraphs(klPartition, n);
        for (auto& graph : subgraphs) {
            Simplifier simplifier(graph);
            simplifier.dynamicReduce();
        }
        ZXGraph* newGraph = ZXGraph::fromSubgraphs(subgraphs, cuts);
        _simpGraph->swap(*newGraph);
        delete newGraph;
    }
}
/**
 * @brief Print recipe of Simplifier
 *
 */
void Simplifier::printRecipe() {
    if (verbose == 0) return;
    if (verbose == 1) {
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
    } else if (verbose <= 3) {
        cout << "\nAll rules applied in order:\n";
        for (size_t i = 0; i < _recipe.size(); i++) {
            if (get<1>(_recipe[i]).size() != 0) {
                cout << setw(30) << left << get<0>(_recipe[i]) << get<1>(_recipe[i]).size() << " iterations." << endl;
                if (verbose == 3) {
                    for (size_t j = 0; j < get<1>(_recipe[i]).size(); j++) {
                        cout << "  " << j + 1 << ") " << get<1>(_recipe[i])[j] << " matches" << endl;
                    }
                }
            }
        }
    }
}
