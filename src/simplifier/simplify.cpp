/****************************************************************************
  FileName     [ simplify.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "simplify.h"

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>

#include "gFlow.h"
#include "zxGraph.h"  // for ZXGraph

using namespace std;
extern size_t verbose;

/**
 * @brief Helper method for constructing simplification strategies based on the rules.
 *
 * @return int
 */
int Simplifier::simp(std::stop_token st) {
    if (_rule->getName() == "Hadamard Rule") {
        cerr << "Error: Please use `hadamardSimp` when using HRule." << endl;
        return 0;
    }
    int i = 0;
    vector<int> matches;
    if (verbose >= 5) cout << setw(30) << left << _rule->getName();
    if (verbose >= 8) cout << endl;
    while (!st.stop_requested()) {
        _rule->match(_simpGraph);
        if (_rule->getMatchTypeVecNum() <= 0)
            break;
        else
            matches.push_back(_rule->getMatchTypeVecNum());
        i += 1;

        if (verbose >= 8) cout << "\nIteration " << i << ":" << endl
                               << ">>>" << endl;
        rewrite();
        amend();
        if (verbose >= 8) cout << "<<<" << endl;
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
int Simplifier::hadamardSimp(std::stop_token st) {
    if (_rule->getName() != "Hadamard Rule") {
        cerr << "Error: `hadamardSimp` is only for HRule." << endl;
        return 0;
    }
    int i = 0;
    vector<int> matches;
    if (verbose >= 5) cout << setw(30) << left << _rule->getName();
    if (verbose >= 8) cout << endl;
    while (!st.stop_requested()) {
        size_t vcount = _simpGraph->getNumVertices();

        _rule->match(_simpGraph);

        if (_rule->getMatchTypeVecNum() == 0)
            break;
        else
            matches.push_back(_rule->getMatchTypeVecNum());
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
int Simplifier::bialgSimp(std::stop_token st) {
    this->setRule(make_unique<Bialgebra>());
    return this->simp(st);
}

/**
 * @brief Perform State Copy Rule
 *
 * @return int
 */
int Simplifier::copySimp(std::stop_token st) {
    if (!_simpGraph->isGraphLike()) return 0;
    this->setRule(make_unique<StateCopy>());
    return this->simp(st);
}

/**
 * @brief Perform Gadget Rule
 *
 * @return int
 */
int Simplifier::gadgetSimp(std::stop_token st) {
    this->setRule(make_unique<PhaseGadget>());
    return this->simp(st);
}

/**
 * @brief Perform Hadamard Fusion Rule
 *
 * @return int
 */
int Simplifier::hfusionSimp(std::stop_token st) {
    this->setRule(make_unique<HboxFusion>());
    return this->simp(st);
}

/**
 * @brief Perform Hadamard Rule
 *
 * @return int
 */
int Simplifier::hruleSimp(std::stop_token st) {
    this->setRule(make_unique<HRule>());
    return this->hadamardSimp(st);
}

/**
 * @brief Perform Identity Removal Rule
 *
 * @return int
 */
int Simplifier::idSimp(std::stop_token st) {
    this->setRule(make_unique<IdRemoval>());
    return this->simp(st);
}

/**
 * @brief Perform Local Complementation Rule
 *
 * @return int
 */
int Simplifier::lcompSimp(std::stop_token st) {
    this->setRule(make_unique<LComp>());
    return this->simp(st);
}

/**
 * @brief Perform Pivot Rule
 *
 * @return int
 */
int Simplifier::pivotSimp(std::stop_token st) {
    this->setRule(make_unique<Pivot>());
    return this->simp(st);
}

/**
 * @brief Perform Pivot Boundary Rule
 *
 * @return int
 */
int Simplifier::pivotBoundarySimp(std::stop_token st) {
    this->setRule(make_unique<PivotBoundary>());
    return this->simp(st);
}

/**
 * @brief Perform Pivot Gadget Rule
 *
 * @return int
 */
int Simplifier::pivotGadgetSimp(std::stop_token st) {
    this->setRule(make_unique<PivotGadget>());
    return this->simp(st);
}

/**
 * @brief Perform Degadgetize Rule
 *
 * @return int
 */
int Simplifier::degadgetizeSimp(std::stop_token st) {
    this->setRule(make_unique<PivotDegadget>());
    return this->simp(st);
}

/**
 * @brief Perform Spider Fusion Rule
 *
 * @return int
 */
int Simplifier::sfusionSimp(std::stop_token st) {
    this->setRule(make_unique<SpiderFusion>());
    return this->simp(st);
}

// action

/**
 * @brief Turn every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 *
 */
void Simplifier::toGraph() {
    for (auto& v : _simpGraph->getVertices()) {
        if (v->getType() == VertexType::X) {
            _simpGraph->toggleEdges(v);
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
            _simpGraph->toggleEdges(v);
        }
    }
}

/**
 * @brief Keep doing the simplifications `id_removal`, `s_fusion`, `pivot`, `lcomp` until none of them can be applied anymore.
 *
 * @return int
 */
int Simplifier::interiorCliffordSimp(std::stop_token st) {
    this->sfusionSimp(st);
    toGraph();

    int i = 0;
    while (!st.stop_requested()) {
        int i1 = this->idSimp(st);
        int i2 = this->sfusionSimp(st);
        int i3 = this->pivotSimp(st);
        int i4 = this->lcompSimp(st);
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
int Simplifier::cliffordSimp(std::stop_token st) {
    int i = 0;
    while (!st.stop_requested()) {
        i += this->interiorCliffordSimp(st);
        int i2 = this->pivotBoundarySimp(st);
        if (i2 == 0) break;
    }
    return i;
}

/**
 * @brief The main simplification routine
 *
 */
void Simplifier::fullReduce(std::stop_token st) {
    _simpGraph->addProcedure("FR");
    this->interiorCliffordSimp(st);
    this->pivotGadgetSimp(st);
    while (!st.stop_requested()) {
        this->cliffordSimp(st);
        int i = this->gadgetSimp(st);
        this->interiorCliffordSimp(st);
        int j = this->pivotGadgetSimp(st);
        if (i + j == 0) break;
    }
    this->printRecipe();
}

/**
 * @brief The reduce strategy with `state_copy` and `full_reduce`
 *
 */
void Simplifier::symbolicReduce(std::stop_token st) {
    this->interiorCliffordSimp(st);
    this->pivotGadgetSimp(st);
    this->copySimp(st);
    while (!st.stop_requested()) {
        this->cliffordSimp(st);
        int i = this->gadgetSimp(st);
        this->interiorCliffordSimp(st);
        int j = this->pivotGadgetSimp(st);
        this->copySimp(st);
        if (i + j == 0) break;
    }
    this->toRGraph();
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