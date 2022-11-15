// /****************************************************************************
//   FileName     [ simplify.cpp ]
//   PackageName  [ simplifier ]
//   Synopsis     [ Define class Stats, Simplify member functions ]
//   Author       [ Cheng-Hua Lu ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include "simplify.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "util.h"
#include <chrono>

using namespace std;
extern size_t verbose;

/******************************************/
/*      class Stats member functions      */
/******************************************/

// void Stats::countRewrites(string rule, int n){
//     if(_rewritesNum.find(rule) != _rewritesNum.end()) _rewritesNum[rule] += n;
//     else _rewritesNum[rule] = n;
// }

/******************************************/
/*     class Simplify member functions    */
/******************************************/

/**
 * @brief Helper method for constructing simplification strategies based on the rules.
 *
 * @param rule_name
 * @return int
 */
int Simplifier::simp() {
    chrono::steady_clock::time_point t_start, timer;
    chrono::microseconds t_match{0}, t_rewrite{0}, t_apply{0};
    t_start = chrono::steady_clock::now();
    size_t addEdgeCount = 0, rmEdgeCount = 0, rmVertexCount = 0;

    if (_rule->getName() == "Hadamard Rule") {
        cerr << "Error: Please use `hadamardSimp` when using HRule." << endl;
        return 0;
    } 
    int i = 0;
    bool new_matches = true;
    if (verbose >= 2) cout << _rule->getName() << ": \n";
    while (new_matches) {
        new_matches = false;
        timer = chrono::steady_clock::now();
        _rule->match(_simpGraph);
        t_match += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();

        if (_rule->getMatchTypeVecNum() <= 0) break;

        i += 1;
        if(verbose >= 5) cout << "Found " << _rule->getMatchTypeVecNum() << " match(es)" << endl;
        
        timer = chrono::steady_clock::now();
        _rule->rewrite(_simpGraph);
        t_rewrite += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();

        // add_edge_table
        // TODO add_edge_table


        timer = chrono::steady_clock::now();
        unordered_set<EdgePair_depr> redundantEdges;
        for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
            ZXVertex* v                = _rule->getEdgeTableKeys()[e].first;
            ZXVertex* v_n              = _rule->getEdgeTableKeys()[e].second;
            int       numSimpleEdges   = _rule->getEdgeTableValues()[e].first;
            int       numHadamardEdges = _rule->getEdgeTableValues()[e].second;
            
            for (int j = 0; j < numSimpleEdges; j++){
                _simpGraph->addEdge(v, v_n, EdgeType(EdgeType::SIMPLE));
                addEdgeCount++;
            }
            // t_simp += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
            // timer = chrono::steady_clock::now();

            for (int j = 0; j < numHadamardEdges; j++){
                _simpGraph->addEdge(v, v_n, EdgeType(EdgeType::HADAMARD));
                addEdgeCount++;
            }
        }     

        _simpGraph->removeEdges(_rule->getRemoveEdges());
        rmEdgeCount += _rule->getRemoveEdges().size();

        // remove vertices
        _simpGraph->removeVertices(_rule->getRemoveVertices());
        rmVertexCount += _rule->getRemoveVertices().size();

        // remove isolated vertices
        _simpGraph->removeIsolatedVertices();
        t_apply += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();
        new_matches = true;
        // TODO check stats
        // cout << "Round " << i << endl;
        // cout << "  - match    : " << ((double)   t_match.count() / 1000) << " ms" << endl;
        // cout << "  - rewrite  : " << ((double) t_rewrite.count() / 1000) << " ms" << endl;
        // cout << "  - apply    : " << ((double)   t_apply.count() / 1000) << " ms" << endl;
    }

    if (verbose == 1 && i != 0) {
        _recipe.emplace_back(_rule->getName(), i);
        cout << setw(30) << left << _rule->getName() << i << " iteration(s)\n";
    }
    if (verbose >= 2) { 
        if (i > 0) {
            cout << i << " iterations" << endl;
        }
        else       cout << "No matches" << endl;
    }
    if(verbose >= 5) cout << "\n";
    if(verbose >= 6) _simpGraph->printVertices();
    // cout << "#Add Edge   : " << addEdgeCount << endl;
    // cout << "#rm  Edge   : " << rmEdgeCount << endl;
    // cout << "#rm  Vertex : " << rmVertexCount << endl;
    // cout << "Time used   : " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t_start).count() << " ms\n" << endl;
    return i;
    
}

/**
 * @brief Converts as many Hadamards represented by H-boxes to Hadamard-edges.
 *        We can't use the regular simp function, because removing H-nodes could lead to an infinite loop,
 *        since sometimes g.add_edge_table() decides that we can't change an H-box into an H-edge.
 *
 * @param rule_name
 * @return int
 */
int Simplifier::hadamardSimp() {
    chrono::steady_clock::time_point t_start, timer;
    chrono::microseconds t_match{0}, t_rewrite{0}, t_apply{0};
    t_start = chrono::steady_clock::now();
    size_t addEdgeCount = 0, rmEdgeCount = 0, rmVertexCount = 0;
    if (_rule->getName() != "Hadamard Rule") {
        cerr << "Error: `hadamardSimp` is only for HRule." << endl;
        return 0;
    }
    int i = 0;
    while (true) {
        size_t vcount = _simpGraph->getNumVertices();

        timer = chrono::steady_clock::now();
        _rule->match(_simpGraph);
        t_match += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();
        
        if (_rule->getMatchTypeVecNum() == 0) break;
        i += 1;
        if (i == 1 && verbose >= 2) cout << _rule->getName() << ": ";
        if (verbose >= 2) cout << _rule->getMatchTypeVecNum() << " ";

        timer = chrono::steady_clock::now();
        _rule->rewrite(_simpGraph);
        t_rewrite += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();
        // add_edge_table
        //! TODO add_edge_table

        timer = chrono::steady_clock::now();
        for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
            ZXVertex* v = _rule->getEdgeTableKeys()[e].first;
            ZXVertex* v_n = _rule->getEdgeTableKeys()[e].second;
            int numSimpleEdges = _rule->getEdgeTableValues()[e].first;
            int numHadamardEdges = _rule->getEdgeTableValues()[e].second;
            for (int j = 0; j < numSimpleEdges; j++) {
                _simpGraph->addEdge(v, v_n, EdgeType(EdgeType::SIMPLE));
                addEdgeCount++;
            }
            for (int j = 0; j < numHadamardEdges; j++) {
                _simpGraph->addEdge(v, v_n, EdgeType(EdgeType::HADAMARD));
                addEdgeCount++;
            }
        }
        // remove edges
        _simpGraph->removeEdges(_rule->getRemoveEdges());
        rmEdgeCount += _rule->getRemoveEdges().size();

        // remove vertices
        _simpGraph->removeVertices(_rule->getRemoveVertices());
        rmVertexCount += _rule->getRemoveVertices().size();
        // remove isolated vertices
        _simpGraph->removeIsolatedVertices();
        t_apply += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();
        if (verbose >= 3) cout << ". ";
        // cout << "Round " << i << endl;
        // cout << "  - match    : " << ((double)   t_match.count() / 1000) << " ms" << endl;
        // cout << "  - rewrite  : " << ((double) t_rewrite.count() / 1000) << " ms" << endl;
        // cout << "  - apply    : " << ((double)   t_apply.count() / 1000) << " ms" << endl;
        if (_simpGraph->getNumVertices() >= vcount) break;
    }
    if (verbose >= 2 && i > 0) cout << i << " iterations" << endl;
    // cout << "#Add Edge   : " << addEdgeCount << endl;
    // cout << "#rm  Edge   : " << rmEdgeCount << endl;
    // cout << "#rm  Vertex : " << rmVertexCount << endl;
    // cout << "Time used   : " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t_start).count() << " ms" << endl;
    return i;
}



// Basic rules simplification

int Simplifier::bialgSimp(){
    this->setRule(new Bialgebra());
    int i = this->simp();
    return i;
}


int Simplifier::copySimp(){
    if(!_simpGraph->isGraphLike()) return 0;
    this->setRule(new StateCopy());
    int i = this->simp();
    return i;
}


int Simplifier::gadgetSimp(){
    this->setRule(new PhaseGadget());
    int i = this->simp();
    return i;
}


int Simplifier::hfusionSimp(){
    this->setRule(new HboxFusion());
    int i = this->simp();
    return i;
}


// int Simplifier::hopfSimp(){
//     this->setRule(new Hopf());
//     int i = this->simp();
//     return i;
// }


int Simplifier::hruleSimp(){
    this->setRule(new HRule());
    int i = this->hadamardSimp();
    return i;
}


int Simplifier::idSimp(){
    this->setRule(new IdRemoval());
    int i = this->simp();
    return i;
}


int Simplifier::lcompSimp(){
    this->setRule(new LComp());
    int i = this->simp();
    return i;
}


int Simplifier::pivotSimp(){
    this->setRule(new Pivot());
    int i = this->simp();
    return i;
}


int Simplifier::pivotBoundarySimp(){
    // TODO: pivot_boundary rule
    return 0;
}


int Simplifier::pivotGadgetSimp(){
    this->setRule(new PivotGadget());
    int i = this->simp();
    return i;
}


int Simplifier::sfusionSimp(){
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
    for(auto& v:  _simpGraph->getVertices()){
        if (v->getType() == VertexType::X){
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
    for(auto& v:  _simpGraph->getVertices()){
        if (v->getType() == VertexType::Z){
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
int Simplifier::interiorCliffordSimp(){
    this->sfusionSimp();
    toGraph();

    int i = 0;
    while(true){
        int i1 = this->idSimp();
        int i2 = this->sfusionSimp();
        int i3 = this->pivotSimp();
        int i4 = this->lcompSimp();
        if(i1+i2+i3+i4 == 0) break;
        i += 1;
    }
    return i;
}


int Simplifier::cliffordSimp(){
    int i = 0;
    while(true){
        i += this->interiorCliffordSimp();
        int i2 = this->pivotBoundarySimp();
        if(i2 == 0) break;
    }
    return i;
}

/**
 * @brief The main simplification routine of PyZX
 * 
 */
void Simplifier::fullReduce(){
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    while(true){
        this->cliffordSimp();
        int i = this->gadgetSimp();
        this->interiorCliffordSimp();
        int j = this->pivotGadgetSimp();
        if(i+j == 0) break;
    }
    // this->printRecipe();
}

/**
 * @brief The main simplification routine of PyZX
 * 
 */
void Simplifier::simulatedReduce(){
    this->interiorCliffordSimp();
    this->pivotGadgetSimp();
    this->copySimp();
    while(true){
        this->cliffordSimp();
        int i = this->gadgetSimp();
        this->interiorCliffordSimp();
        int j = this->pivotGadgetSimp();
        this->copySimp();
        if(i+j == 0) break;
    }
}




// print function
void Simplifier::printRecipe(){
    if(verbose == 1){
        for(auto& [rule_name, num] : _recipe){
            string rule = rule_name+": ";
            cout << setw(30) << left << rule << num << " iteration(s)\n";
        }
    }
}