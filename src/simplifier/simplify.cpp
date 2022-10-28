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
#include <vector>

#include "util.h"

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
    if (_rule->getName() == "Hadamard Rule") {
        cerr << "Error: Please use `hadamard_simp` when using HRule." << endl;
        return 0;
    } else {
        int i = 0;
        bool new_matches = true;
        if (verbose >= 2) cout << _rule->getName() << ": \n";
        while (new_matches) {
            new_matches = false;
            _rule->match(_simpGraph);

            if (_rule->getMatchTypeVecNum() > 0) {
                i += 1;
                if(verbose >= 5) cout << "Found " << _rule->getMatchTypeVecNum() << " match(es)" << endl;
                _rule->rewrite(_simpGraph);
                // add_edge_table
                // TODO add_edge_table
                for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
                    for (int j = 0; j < _rule->getEdgeTableValues()[e].first; j++){
                        bool found=false;
                        if((_rule->getEdgeTableKeys()[e].first->getType()==VertexType::Z && _rule->getEdgeTableKeys()[e].second->getType()==VertexType::X) ||
                            (_rule->getEdgeTableKeys()[e].first->getType()==VertexType::X && _rule->getEdgeTableKeys()[e].second->getType()==VertexType::Z)
                        ){
                            ZXVertex* v = _rule->getEdgeTableKeys()[e].first;
                            ZXVertex* v_n = _rule->getEdgeTableKeys()[e].second;
                            auto candidates = v->getNeighborMap().equal_range(_rule->getEdgeTableKeys()[e].second);
                            for(auto itr = candidates.first; itr!=candidates.second; itr++){
                                if ((*itr->second)==EdgeType::SIMPLE){
                                    _simpGraph->removeEdgeByEdgePair(make_pair(make_pair(v,v_n), itr->second));
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if(!found)
                            _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::SIMPLE));
                    }
                       
                    for (int j = 0; j < _rule->getEdgeTableValues()[e].second; j++){
                        bool found=false;
                        if((_rule->getEdgeTableKeys()[e].first->getType()==VertexType::Z && _rule->getEdgeTableKeys()[e].second->getType()==VertexType::Z) ||
                            (_rule->getEdgeTableKeys()[e].first->getType()==VertexType::X && _rule->getEdgeTableKeys()[e].second->getType()==VertexType::X)
                        ){
                            ZXVertex* v = _rule->getEdgeTableKeys()[e].first;
                            ZXVertex* v_n = _rule->getEdgeTableKeys()[e].second;
                            auto candidates = v->getNeighborMap().equal_range(_rule->getEdgeTableKeys()[e].second);
                            for(auto itr = candidates.first; itr!=candidates.second; itr++){
                                if ((*itr->second)==EdgeType::HADAMARD){
                                    _simpGraph->removeEdgeByEdgePair(make_pair(make_pair(v,v_n), itr->second));
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if(!found)
                            _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::HADAMARD));
                    }
                       
                }
                // remove edges
                for (size_t e = 0; e < _rule->getRemoveEdges().size(); e++) {
                    _simpGraph->removeEdgeByEdgePair(_rule->getRemoveEdges()[e]);
                }
                // remove vertices
                _simpGraph->removeVertices(_rule->getRemoveVertices());
                // remove isolated vertices
                _simpGraph->removeIsolatedVertices();
                new_matches = true;
                // TODO check stats
            }
        }
        if (verbose >= 2) { 
            if (i > 0) {
                cout << i << " iterations" << endl;
            }
            else       cout << "No matches" << endl;
            
        }
        return i;
    }
}

/**
 * @brief Converts as many Hadamards represented by H-boxes to Hadamard-edges.
 *        We can't use the regular simp function, because removing H-nodes could lead to an infinite loop,
 *        since sometimes g.add_edge_table() decides that we can't change an H-box into an H-edge.
 *
 * @param rule_name
 * @return int
 */
int Simplifier::hadamard_simp() {
    if (_rule->getName() != "Hadamard Rule") {
        cerr << "Error: `hadamard_simp` is only for HRule." << endl;
        return 0;
    } else {
        int i = 0;
        while (true) {
            size_t vcount = _simpGraph->getNumVertices();
            _rule->match(_simpGraph);

            if (_rule->getMatchTypeVecNum() == 0) break;
            i += 1;
            if (i == 1 && verbose >= 2) cout << _rule->getName() << ": ";
            if (verbose >= 2) cout << _rule->getMatchTypeVecNum() << " ";

            _rule->rewrite(_simpGraph);
            // add_edge_table
            //! TODO add_edge_table
            for (size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++) {
                for (int j = 0; j < _rule->getEdgeTableValues()[e].first; j++)
                    _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::SIMPLE));
                for (int j = 0; j < _rule->getEdgeTableValues()[e].second; j++)
                    _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::HADAMARD));
            }
            // remove edges
            for (size_t e = 0; e < _rule->getRemoveEdges().size(); e++) {
                _simpGraph->removeEdgeByEdgePair(_rule->getRemoveEdges()[e]);
            }
            // remove vertices
            _simpGraph->removeVertices(_rule->getRemoveVertices());
            // remove isolated vertices
            _simpGraph->removeIsolatedVertices();
            if (verbose >= 3) cout << ". ";
            if (_simpGraph->getNumVertices() >= vcount) break;
        }

        if (verbose >= 2 && i > 0) cout << i << " iterations" << endl;
        return i;
    }
}



// Basic rules simplification

int Simplifier::bialg_simp(){
    this->setRule(new Bialgebra());
    int i = this->simp();
    return i;
}


int Simplifier::copy_simp(){
    this->setRule(new StateCopy());
    int i = this->simp();
    return i;
}


int Simplifier::gadget_simp(){
    // TODO: phase gadget rule
    this->setRule(new PhaseGadget());
    int i = this->simp();
    return i;
}


int Simplifier::hfusion_simp(){
    this->setRule(new HboxFusion());
    int i = this->simp();
    return i;
}


int Simplifier::hopf_simp(){
    this->setRule(new Hopf());
    int i = this->simp();
    return i;
}


int Simplifier::hrule_simp(){
    this->setRule(new HRule());
    int i = this->hadamard_simp();
    return i;
}


int Simplifier::id_simp(){
    this->setRule(new IdRemoval());
    int i = this->simp();
    return i;
}


int Simplifier::lcomp_simp(){
    this->setRule(new LComp());
    int i = this->simp();
    return i;
}


int Simplifier::pivot_simp(){
    this->setRule(new Pivot());
    int i = this->simp();
    return i;
}


int Simplifier::pivot_boundary_simp(){
    // TODO: pivot_boundary rule
    return 0;
}


int Simplifier::pivot_gadget_simp(){
    this->setRule(new PivotGadget());
    int i = this->simp();
    hopf_simp();
    return i;
}


int Simplifier::sfusion_simp(){
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
void Simplifier::to_graph() {
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
void Simplifier::to_rgraph() {
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
int Simplifier::interior_clifford_simp(){
    this->sfusion_simp();
    to_graph();

    int i = 0;
    while(true){
        int i1 = this->id_simp();
        int i2 = this->sfusion_simp();
        int i3 = this->pivot_simp();
        int i4 = this->lcomp_simp();
        if(i1+i2+i3+i4 == 0) break;
        i += 1;
    }
    return i;
}


int Simplifier::clifford_simp(){
    int i = 0;
    while(true){
        i += this->interior_clifford_simp();
        int i2 = this->pivot_boundary_simp();
        if(i2 == 0) break;
    }
    return i;
}

/**
 * @brief The main simplification routine of PyZX
 * 
 */
void Simplifier::full_reduce(){
    this->interior_clifford_simp();
    this->pivot_gadget_simp();
    while(true){
        this->clifford_simp();
        int i = this->gadget_simp();
        this->interior_clifford_simp();
        int j = this->pivot_gadget_simp();
        if(i+j == 0) break;
    }
}

/**
 * @brief The main simplification routine of PyZX
 * 
 */
void Simplifier::simulated_reduce(){
    this->interior_clifford_simp();
    this->pivot_gadget_simp();
    // this->copy_simp();
    // while(true){
    //     this->clifford_simp();
    //     int i = this->gadget_simp();
    //     this->interior_clifford_simp();
    //     int j = this->pivot_gadget_simp();
    //     this->copy_simp();
    //     if(i+j == 0) break;
    // }
}
