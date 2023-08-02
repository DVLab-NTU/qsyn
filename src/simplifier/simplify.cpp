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

#include "extract.h"
#include "gFlow.h"
#include "zxDef.h"
#include "zxGraph.h"  // for ZXGraph
#include "zxGraphMgr.h"
#include "zxPartition.h"
#include "zxoptimizer.h"

using namespace std;
extern size_t verbose;
extern size_t dmode;
extern ZXOPTimizer opt;

int cnt = 0;
bool step = false;
bool stop = false;

/**
 * @brief Helper method for constructing simplification strategies based on the rules.
 *
 * @return int
 */
int Simplifier::simp() {
    if (_rule->getName() == "Hadamard Rule") {
        cerr << "Error: Please use `hadamardSimp` when using HRule." << endl;
        return 0;
    }
    int i = 0;
    vector<int> matches;
    if (verbose >= 5) cout << setw(30) << left << _rule->getName();
    if (verbose >= 8) cout << endl;
    int r2r = opt.getR2R(_rule->getName());
    while (!_stop_token.stop_requested() && r2r > 0) {
        _rule->match(_simpGraph, opt.getS2S(_rule->getName()));
        if (r2r < INT_MAX) r2r--;

        if (_rule->getMatchTypeVecNum() <= 0)
            break;
        else
            matches.emplace_back(_rule->getMatchTypeVecNum());
        i += 1;

        if (verbose >= 8) cout << "\nIteration " << i << ":" << endl
                               << ">>>" << endl;
        rewrite();
        amend();
        if (verbose >= 8) cout << "<<<" << endl;
        // if(r2r == 0) break;
    }
    _recipe.emplace_back(_rule->getName(), matches);
    if (verbose >= 8) cout << "=> ";
    if (verbose >= 5) {
        cout << i << " iterations." << endl;
        for (size_t m = 0; m < matches.size(); m++) {
            cout << "  " << m + 1 << ") " << matches[m] << " matches" << endl;
        }
    }
    if (verbose >= 5) cout << "\n";
    return i;
}

/**
 *
 * @brief Convert as many Hadamards represented by H-boxes to Hadamard-edges.
 *
 * @return int
 */
int Simplifier::hadamardSimp() {
    if (_rule->getName() != "Hadamard Rule") {
        cerr << "Error: `hadamardSimp` is only for HRule." << endl;
        return 0;
    }
    int i = 0;
    vector<int> matches;
    if (verbose >= 5) cout << setw(30) << left << _rule->getName();
    if (verbose >= 8) cout << endl;
    while (!_stop_token.stop_requested()) {
        size_t vcount = _simpGraph->getNumVertices();
        _rule->match(_simpGraph);

        if (_rule->getMatchTypeVecNum() == 0)
            break;
        else
            matches.emplace_back(_rule->getMatchTypeVecNum());
        i += 1;

        if (verbose >= 8) cout << "\nIteration " << i << ":" << endl
                               << ">>>" << endl;
        rewrite();
        amend();
        if (verbose >= 8) cout << "<<<" << endl;
        if (_simpGraph->getNumVertices() >= vcount) break;
    }
    _recipe.emplace_back(_rule->getName(), matches);
    if (verbose >= 8) cout << "=> ";
    if (verbose >= 5) {
        cout << i << " iterations." << endl;
        for (size_t m = 0; m < matches.size(); m++) {
            cout << "  " << m + 1 << ") " << matches[m] << " matches" << endl;
        }
    }
    if (verbose >= 5) cout << "\n";
    return i;
}

/**
 * @brief Apply rule
 */
void Simplifier::amend() {
    // cout << _rule->getEdgeTableKeys().size() << " " << _rule->getRemoveEdges().size() << " " << _rule->getRemoveVertices().size() << endl;
    for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
        ZXVertex* v = _rule->getEdgeTableKeys()[e].first;
        ZXVertex* v_n = _rule->getEdgeTableKeys()[e].second;
        int numSimpleEdges = _rule->getEdgeTableValues()[e].first;
        int numHadamardEdges = _rule->getEdgeTableValues()[e].second;

        if (numSimpleEdges) _simpGraph->addEdge(v, v_n, EdgeType::SIMPLE);
        if (numHadamardEdges) _simpGraph->addEdge(v, v_n, EdgeType::HADAMARD);
    }
    _simpGraph->removeEdges(_rule->getRemoveEdges());
    _simpGraph->removeVertices(_rule->getRemoveVertices());

    _simpGraph->removeIsolatedVertices();
}

// Basic rules simplification

/**
 * @brief Perform Bialgebra Rule
 *
 * @return int
 */
int Simplifier::bialgSimp() {
    this->setRule(make_unique<Bialgebra>());
    return this->simp();
}

/**
 * @brief Perform State Copy Rule
 *
 * @return int
 */
int Simplifier::copySimp() {
    if (!_simpGraph->isGraphLike()) return 0;
    this->setRule(make_unique<StateCopy>());
    return this->simp();
}

/**
 * @brief Perform Gadget Rule
 *
 * @return int
 */
int Simplifier::gadgetSimp() {
    this->setRule(make_unique<PhaseGadget>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }

    if (stop) return -1;
    return i;
}

/**
 * @brief Perform Hadamard Fusion Rule
 *
 * @return int
 */
int Simplifier::hfusionSimp() {
    this->setRule(make_unique<HboxFusion>());
    return this->simp();
}

/**
 * @brief Perform Hadamard Rule
 *
 * @return int
 */
int Simplifier::hruleSimp() {
    this->setRule(make_unique<HRule>());
    return this->hadamardSimp();
}

/**
 * @brief Perform Identity Removal Rule
 *
 * @return int
 */
int Simplifier::idSimp() {
    this->setRule(make_unique<IdRemoval>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }
    if (stop) return -1;
    return i;
}

/**
 * @brief Perform Local Complementation Rule
 *
 * @return int
 */
int Simplifier::lcompSimp() {
    this->setRule(make_unique<LComp>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }
    if (stop) return -1;
    return i;
}

/**
 * @brief Perform Pivot Rule
 *
 * @return int
 */
int Simplifier::pivotSimp() {
    this->setRule(make_unique<Pivot>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }
    if (stop) return -1;
    return i;
}

/**
 * @brief Perform Pivot Boundary Rule
 *
 * @return int
 */
int Simplifier::pivotBoundarySimp() {
    this->setRule(make_unique<PivotBoundary>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }
    if (stop) return -1;
    return i;
}

/**
 * @brief Perform Pivot Gadget Rule
 *
 * @return int
 */
int Simplifier::pivotGadgetSimp() {
    this->setRule(make_unique<PivotGadget>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }
    if (stop) return -1;
    return i;
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
    this->setRule(make_unique<SpiderFusion>());
    int i = this->simp();
    if (i > 0) {
        if (verbose >= 8) cout << this->getRule()->getName() << endl;
        stop = opt.updateParameters(_simpGraph);
    }
    if (stop) return -1;
    return i;
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
    int r2r = opt.getR2R("Interior Clifford Simp");
    while (true && r2r > 0) {
        int i1 = this->idSimp();
        if (i1 == -1) return -1;
        int i2 = this->sfusionSimp();
        if (i2 == -1) return -1;
        int i3 = this->pivotSimp();
        if (i3 == -1) return -1;
        int i4 = this->lcompSimp();
        if (i4 == -1) return -1;
        // if(step && i4) getStepInfo(_simpGraph);
        if (i1 + i2 + i3 + i4 == 0) break;
        i += 1;
        r2r--;
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
    int r2r = opt.getR2R("Clifford Simp");
    while (true && r2r > 0) {
        int i1 = this->interiorCliffordSimp();
        if (i1 == -1) return -1;
        i += i1;
        int i2 = this->pivotBoundarySimp();
        if (i2 == -1) return -1;
        if (r2r != INT_MAX) r2r--;
        if (i2 == 0 || r2r == 0) break;
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
    while (!_stop_token.stop_requested()) {
        this->cliffordSimp();
        int i = this->gadgetSimp();
        if (i == -1) i = 0;
        this->interiorCliffordSimp();
        int j = this->pivotGadgetSimp();
        if (j == -1) j = 0;
        if (i + j == 0) break;
    }
    this->printRecipe();
    if (_stop_token.stop_requested()) {
        _simpGraph->addProcedure("FR[INT]");
    } else {
        _simpGraph->addProcedure("FR");
    }
}

void Simplifier::dynamicReduce() {
    // copy the graph's structure
    ZXGraph _copiedGraph = *_simpGraph;
    cout << endl
         << "Full Reduce:";
    // to obtain the T-optimal
    Simplifier simp = Simplifier(&_copiedGraph);
    simp.fullReduce();
    int tOptimal = _copiedGraph.TCount();

    cout << endl
         << "Dynamic Reduce:";
    _recipe.clear();

    cout << " (T-optimal: " << tOptimal << ")";
    opt.init();
    opt.updateParameters(_simpGraph);

    int a1 = this->interiorCliffordSimp();

    if (a1 == -1) {
        _simpGraph = opt.getLastZXGraph();
        this->printRecipe();
        return;
    }

    int a2 = this->pivotGadgetSimp();
    if (a2 == -1 && tOptimal == _simpGraph->TCount()) {
        _simpGraph = opt.getLastZXGraph();
        this->printRecipe();
        return;
    }

    while (!_stop_token.stop_requested()) {
        int a3 = this->cliffordSimp();
        if (a3 == -1 && tOptimal == _simpGraph->TCount()) {
            _simpGraph = opt.getLastZXGraph();
            this->printRecipe();
            return;
        }

        int a4 = this->gadgetSimp();
        if (a4 == -1 && tOptimal == _simpGraph->TCount()) {
            _simpGraph = opt.getLastZXGraph();
            this->printRecipe();
            return;
        }

        int a5 = this->interiorCliffordSimp();
        if (a5 == -1 && tOptimal == _simpGraph->TCount()) {
            _simpGraph = opt.getLastZXGraph();
            this->printRecipe();
            return;
        }

        int a6 = this->pivotGadgetSimp();
        if (a6 == -1 && tOptimal == _simpGraph->TCount()) {
            _simpGraph = opt.getLastZXGraph();
            this->printRecipe();
            return;
        }

        if (a4 + a6 == 0) break;
    }
    this->printRecipe();

    if (_stop_token.stop_requested()) {
        _simpGraph->addProcedure("DR[INT]");
    } else {
        _simpGraph->addProcedure("DR");
    }
}

/**
 * @brief The reduce strategy with `state_copy` and `full_reduce`
 *
 */
void Simplifier::symbolicReduce() {
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    this->copySimp();
    while (!_stop_token.stop_requested()) {
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
    for (size_t n = numPartitions; n >= 2; n /= 2) {
        auto [subgraphs, cuts] = _simpGraph->createSubgraphs(klPartition, n);
        for (auto& graph : subgraphs) {
            Simplifier simplifier(graph);
            simplifier.dynamicReduce();
        }
        ZXGraph* newGraph = ZXGraph::fromSubgraphs(subgraphs, cuts);
        _simpGraph->swap(*newGraph);
        delete newGraph;
    }

    if (_stop_token.stop_requested()) {
        _simpGraph->addProcedure("PR[INT]");
    } else {
        _simpGraph->addProcedure("PR");
    }
}

/**
 * @brief Print recipe of Simplifier
 *
 */
void Simplifier::printRecipe() {
    if (verbose <= 3) {
        if (verbose == 0)
            return;
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
        } else {
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
}
