/****************************************************************************
  FileName     [ simplify.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Stats, Simplify member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "simplify.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "util.h"

using namespace std;
extern size_t verbose;

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

    bool new_matches = true;  // FIXME - useless flag
    vector<int> matches;

    if (verbose >= 5) cout << setw(30) << left << _rule->getName();
    if (verbose >= 8) cout << endl;
    while (new_matches) {
        new_matches = false;
        _rule->match(_simpGraph);
        if (_rule->getMatchTypeVecNum() <= 0)
            break;
        else
            matches.push_back(_rule->getMatchTypeVecNum());
        i += 1;

        if (verbose >= 8) cout << "\nIteration " << i << ":" << endl
                               << ">>>" << endl;
        _rule->rewrite(_simpGraph);
        amend();
        if (verbose >= 8) cout << "<<<" << endl;
        new_matches = true;
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
 * @brief Converts as many Hadamards represented by H-boxes to Hadamard-edges.
 *        We can't use the regular simp function, because removing H-nodes could lead to an infinite loop,
 *        since sometimes g.add_edge_table() decides that we can't change an H-box into an H-edge.
 *
 * //FIXME - weird function brief
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
    while (true) {
        size_t vcount = _simpGraph->getNumVertices();

        _rule->match(_simpGraph);

        if (_rule->getMatchTypeVecNum() == 0)
            break;
        else
            matches.push_back(_rule->getMatchTypeVecNum());
        i += 1;

        if (verbose >= 8) cout << "\nIteration " << i << ":" << endl
                               << ">>>" << endl;
        _rule->rewrite(_simpGraph);
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
 * @brief apply rule
 */
void Simplifier::amend() {
    for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
        ZXVertex* v = _rule->getEdgeTableKeys()[e].first;
        ZXVertex* v_n = _rule->getEdgeTableKeys()[e].second;
        int numSimpleEdges = _rule->getEdgeTableValues()[e].first;
        int numHadamardEdges = _rule->getEdgeTableValues()[e].second;

        if (v->getId() > v_n->getId()) swap(v, v_n);
        for (int j = 0; j < numSimpleEdges; j++)
            _simpGraph->addEdge(v, v_n, EdgeType(EdgeType::SIMPLE));

        for (int j = 0; j < numHadamardEdges; j++)
            _simpGraph->addEdge(v, v_n, EdgeType(EdgeType::HADAMARD));
    }
    _simpGraph->removeEdges(_rule->getRemoveEdges());
    _simpGraph->removeVertices(_rule->getRemoveVertices());

    _simpGraph->removeIsolatedVertices();
}

// Basic rules simplification

int Simplifier::bialgSimp() {
    this->setRule(new Bialgebra());
    int i = this->simp();
    return i;
}

int Simplifier::copySimp() {
    if (!_simpGraph->isGraphLike()) return 0;
    this->setRule(new StateCopy());
    int i = this->simp();
    return i;
}

int Simplifier::gadgetSimp() {
    this->setRule(new PhaseGadget());
    int i = this->simp();
    return i;
}

int Simplifier::hfusionSimp() {
    this->setRule(new HboxFusion());
    int i = this->simp();
    return i;
}

// int Simplifier::hopfSimp(){
//     this->setRule(new Hopf());
//     int i = this->simp();
//     return i;
// }

int Simplifier::hruleSimp() {
    this->setRule(new HRule());
    int i = this->hadamardSimp();
    return i;
}

int Simplifier::idSimp() {
    this->setRule(new IdRemoval());
    int i = this->simp();
    return i;
}

int Simplifier::lcompSimp() {
    this->setRule(new LComp());
    int i = this->simp();
    return i;
}

int Simplifier::pivotSimp() {
    this->setRule(new Pivot());
    int i = this->simp();
    return i;
}

int Simplifier::pivotBoundarySimp() {
    this->setRule(new PivotBoundary());
    int i = this->simp();
    return i;
}

int Simplifier::pivotGadgetSimp() {
    this->setRule(new PivotGadget());
    int i = this->simp();
    return i;
}

int Simplifier::sfusionSimp() {
    this->setRule(new SpiderFusion());
    int i = this->simp();
    return i;
}

// action

/**
 * @brief Turns every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 *
 * @param
 */
void Simplifier::toGraph() {
    for (auto& v : _simpGraph->getVertices()) {
        if (v->getType() == VertexType::X) {
            _simpGraph->toggleEdges(v);
            v->setType(VertexType::Z);
        }
    }
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 *
 * @param
 */
void Simplifier::toRGraph() {
    for (auto& v : _simpGraph->getVertices()) {
        if (v->getType() == VertexType::Z) {
            _simpGraph->toggleEdges(v);
            v->setType(VertexType::X);
        }
    }
}

/**
 * @brief Keeps doing the simplifications `id_removal`, `s_fusion`, `pivot`, `lcomp` until none of them can be applied anymore.
 *
 * @return int
 */
int Simplifier::interiorCliffordSimp() {
    this->sfusionSimp();
    toGraph();

    int i = 0;
    while (true) {
        int i1 = this->idSimp();
        int i2 = this->sfusionSimp();
        int i3 = this->pivotSimp();
        int i4 = this->lcompSimp();
        if (i1 + i2 + i3 + i4 == 0) break;
        i += 1;
    }
    return i;
}

int Simplifier::cliffordSimp() {
    int i = 0;
    while (true) {
        i += this->interiorCliffordSimp();
        int i2 = this->pivotBoundarySimp();
        if (i2 == 0) break;
    }
    return i;
}

/**
 * @brief The main simplification routine of PyZX
 *
 */
void Simplifier::fullReduce() {
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    while (true) {
        this->cliffordSimp();
        int i = this->gadgetSimp();
        this->interiorCliffordSimp();
        int j = this->pivotGadgetSimp();
        if (i + j == 0) break;
    }
    this->printRecipe();
}

/**
 * @brief
 *
 */
void Simplifier::symbolicReduce() {
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    this->copySimp();
    while (true) {
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
 * @brief print recipe of Simplifier
 *
 */
void Simplifier::printRecipe() {
    if (verbose <= 3) {
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