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
    bool checked = true; // for efficiency; you may want to change this to `false` if debugging a new rule
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
        size_t adde_actual = 0;
        size_t  rme_actual = 0;
        size_t  rmv_actual = 0;
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
        unordered_set<EdgePair> redundantEdges;
        for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
            ZXVertex* v                = _rule->getEdgeTableKeys()[e].first;
            ZXVertex* v_n              = _rule->getEdgeTableKeys()[e].second;
            int       numSimpleEdges   = _rule->getEdgeTableValues()[e].first;
            int       numHadamardEdges = _rule->getEdgeTableValues()[e].second;
            
            for (int j = 0; j < numSimpleEdges; j++){
                bool found = false;
                if((v->getType()==VertexType::Z && v_n->getType()==VertexType::X) ||
                    (v->getType()==VertexType::X && v_n->getType()==VertexType::Z)
                ){
                    auto candidates = v->getNeighborMap().equal_range(v_n);
                    for(auto itr = candidates.first; itr!=candidates.second; itr++){

                        EdgePair tmp = (v->getId() < v_n->getId()) ? make_pair(make_pair(v,v_n), itr->second) : make_pair(make_pair(v_n,v), itr->second);
                        if ((*itr->second)==EdgeType::SIMPLE && !redundantEdges.contains(tmp)) {
                            redundantEdges.insert(tmp);
                            _rule->pushRemoveEdge(tmp);
                            found = true;
                            break;
                        }
                    }
                }
                if(!found) {
                    _simpGraph->addEdge(v, v_n, new EdgeType(EdgeType::SIMPLE));
                    addEdgeCount++;
                    adde_actual++;
                }
            }
            // t_simp += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
            // timer = chrono::steady_clock::now();

            for (int j = 0; j < numHadamardEdges; j++){
                bool found = false;
                if((v->getType()==VertexType::Z && v_n->getType()==VertexType::Z) ||
                    (v->getType()==VertexType::X && v_n->getType()==VertexType::X)
                ){
                    auto candidates = v->getNeighborMap().equal_range(v_n);
                    for(auto itr = candidates.first; itr!=candidates.second; itr++){
                        EdgePair tmp = (v->getId() < v_n->getId()) ? make_pair(make_pair(v,v_n), itr->second) : make_pair(make_pair(v_n,v), itr->second);
                        if ((*itr->second)==EdgeType::HADAMARD && !redundantEdges.contains(tmp)) {
                            redundantEdges.insert(tmp);
                            _rule->pushRemoveEdge(tmp);
                            found = true;
                            break;
                        }
                    }
                }
                if(!found) {
                    _simpGraph->addEdge(v, v_n, new EdgeType(EdgeType::HADAMARD));
                    adde_actual++;
                }
            }
        }     

        _simpGraph->removeEdgesByEdgePairs(_rule->getRemoveEdges());
        rmEdgeCount += _rule->getRemoveEdges().size();
        rme_actual += _rule->getRemoveEdges().size();

        // remove vertices
        _simpGraph->removeVertices(_rule->getRemoveVertices(), checked);
        rmVertexCount += _rule->getRemoveVertices().size();
        rmv_actual += _rule->getRemoveEdges().size();

        // remove isolated vertices
        _simpGraph->removeIsolatedVertices();
        t_apply += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();
        new_matches = true;
        // TODO check stats
        // cout << "Round " << i << endl;
        // cout << "  - Add edges    : " << ((double)   t_match.count() / 1000) << " ms / " << adde_actual << endl;
        // cout << "  - rm  edges    : " << ((double) t_rewrite.count() / 1000) << " ms / " <<  rme_actual << endl;
        // cout << "  - rm  vertices : " << ((double)   t_apply.count() / 1000) << " ms / " <<  rmv_actual << endl;
    }

    if (verbose == 1 && i != 0) {
        _recipe.emplace_back(_rule->getName(), i);
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
    bool checked = true; // for efficiency; you may want to change this to `false` if debugging a new rule
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
        size_t adde_actual = 0;
        size_t  rme_actual = 0;
        size_t  rmv_actual = 0;
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
                _simpGraph->addEdge(v, v_n, new EdgeType(EdgeType::SIMPLE));
                addEdgeCount++;
                adde_actual++;
            }
            for (int j = 0; j < numHadamardEdges; j++) {
                _simpGraph->addEdge(v, v_n, new EdgeType(EdgeType::HADAMARD));
                addEdgeCount++;
                adde_actual++;
            }
        }
        // remove edges
        _simpGraph->removeEdgesByEdgePairs(_rule->getRemoveEdges());
        rmEdgeCount += _rule->getRemoveEdges().size();
        rme_actual += _rule->getRemoveEdges().size();

        // remove vertices
        _simpGraph->removeVertices(_rule->getRemoveVertices(), checked);
        rmVertexCount += _rule->getRemoveVertices().size();
        rmv_actual += _rule->getRemoveVertices().size();
        // remove isolated vertices
        _simpGraph->removeIsolatedVertices();
        t_apply += chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - timer);
        timer = chrono::steady_clock::now();
        if (verbose >= 3) cout << ". ";
        // cout << "Round " << i << endl;
        // cout << "  - Add edges    : " << ((double)   t_match.count() / 1000) << " ms / " << adde_actual << endl;
        // cout << "  - rm  edges    : " << ((double) t_rewrite.count() / 1000) << " ms / " <<  rme_actual << endl;
        // cout << "  - rm  vertices : " << ((double)   t_apply.count() / 1000) << " ms / " <<  rmv_actual << endl;
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
    // TODO: phase gadget rule
    this->setRule(new PhaseGadget());
    int i = this->simp();
    return i;
}


int Simplifier::hfusionSimp(){
    this->setRule(new HboxFusion());
    int i = this->simp();
    return i;
}


int Simplifier::hopfSimp(){
    this->setRule(new Hopf());
    int i = this->simp();
    return i;
}


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
    hopfSimp();
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
 * @param g ZXGraph*
 */
void Simplifier::toGraph() {
    for (size_t i = 0; i < _simpGraph->getNumVertices(); i++) {
        ZXVertex* v = _simpGraph->getVertices()[i];
        if (v->getType() == VertexType::X) {
            for (auto& itr : v->getNeighborMap()) *itr.second = toggleEdge(*itr.second);
            v->setType(VertexType::Z);
        }
    }
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 *
 * @param g
 */
void Simplifier::toRGraph() {
    for (size_t i = 0; i < _simpGraph->getNumVertices(); i++) {
        ZXVertex* v = _simpGraph->getVertices()[i];
        if (v->getType() == VertexType::Z) {
            for (auto& itr : v->getNeighborMap()) *itr.second = toggleEdge(*itr.second);
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
    this->printRecipe();
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
            cout << setw(30) << left << rule << num << " iterator(s)\n";
        }
    }
}