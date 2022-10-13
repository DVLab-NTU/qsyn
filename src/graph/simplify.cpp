/****************************************************************************
  FileName     [ simplify.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class Stats, Simplify member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/





#include <vector>
#include <iostream>
#include <unordered_map>

#include "util.h"
#include "simplify.h"

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
int Simplifier::simp(){
  if(_rule->getName() == "Hadamard Rule"){
    cerr << "Error: Please use `hadamard_simp` when using HRule." << endl;
    return 0;
  }
  else{
    int i = 0; bool new_matches = true;
    while(new_matches){
      new_matches = false;
      _rule->match(_simpGraph);

      if(_rule->getMatchTypeVecNum() > 0){
        i += 1;
        if(i == 1 && verbose >= 2) cout << _rule->getName() << ": ";
        if(verbose >= 2) cout << _rule->getMatchTypeVecNum() << " ";
        _rule->rewrite(_simpGraph);
        // add_edge_table
        //! TODO add_edge_table
        for(size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++){
          for(int j = 0; j < _rule->getEdgeTableValues()[e].first; j++)
            _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::SIMPLE));
          for(int j = 0; j < _rule->getEdgeTableValues()[e].second; j++)
            _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::HADAMARD));
        }
        // remove edges
        for(size_t e = 0; e < _rule->getRemoveEdges().size(); e++){
          _simpGraph->removeEdgeByEdgePair(_rule->getRemoveEdges()[e]);
        }
        // remove vertices
        _simpGraph->removeVertices(_rule->getRemoveVertices());
        // remove isolated vertices
        _simpGraph->removeIsolatedVertices();
        if(verbose >= 3) cout << ". ";
        new_matches = true;
        //! TODO check stats
      } 
    }
    if(verbose >= 2 && i > 0) cout << i << " iterations" << endl;
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
int Simplifier::hadamard_simp(){
  if(_rule->getName() != "Hadamard Rule"){
    cerr << "Error: `hadamard_simp` is only for HRule." << endl;
    return 0;
  } 
  else{
    int i = 0; 
    while(true){
      size_t vcount = _simpGraph->getNumVertices();
      _rule->match(_simpGraph);
      
      if(_rule->getMatchTypeVecNum() == 0) break;
      i += 1;
      if(i == 1 && verbose >= 2) cout << _rule->getName() << ": ";
      if(verbose >= 2) cout << _rule->getMatchTypeVecNum() << " ";

      _rule->rewrite(_simpGraph);
      // add_edge_table
      //! TODO add_edge_table
      for(size_t e = 0; e < _rule->getEdgeTableKeys().size(); e++){
        for(int j = 0; j < _rule->getEdgeTableValues()[e].first; j++)
          _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::SIMPLE));
        for(int j = 0; j < _rule->getEdgeTableValues()[e].second; j++)
          _simpGraph->addEdge(_rule->getEdgeTableKeys()[e].first, _rule->getEdgeTableKeys()[e].second, new EdgeType(EdgeType::HADAMARD));
      }
      // remove edges
      for(size_t e = 0; e < _rule->getRemoveEdges().size(); e++){
        _simpGraph->removeEdgeByEdgePair(_rule->getRemoveEdges()[e]);
      }
      // remove vertices
      _simpGraph->removeVertices(_rule->getRemoveVertices());
      // remove isolated vertices
      _simpGraph->removeIsolatedVertices();
      if(verbose >= 3) cout << ". ";
      if(_simpGraph->getNumVertices() >= vcount) break;
    } 

    if(verbose >= 2 && i > 0) cout << i << " iterations" << endl;
    return i;
  }
}



/**
 * @brief Turns every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 * 
 * @param g ZXGraph*
 */
void Simplifier::to_graph(){
    for(size_t i = 0; i < _simpGraph->getNumVertices(); i++){
      ZXVertex* v = _simpGraph->getVertices()[i];
      if(v->getType() == VertexType::X){
        for(auto& itr : v->getNeighborMap()) *itr.second = toggleEdge(*itr.second);
        v->setType(VertexType::Z);
      }
    }
    if(verbose >= 3) _simpGraph->printVertices();
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 * 
 * @param g 
 */
void Simplifier::to_rgraph(){
  for(size_t i = 0; i < _simpGraph->getNumVertices(); i++){
    ZXVertex* v = _simpGraph->getVertices()[i];
    if(v->getType() == VertexType::Z){
      for(auto& itr : v->getNeighborMap()) *itr.second = toggleEdge(*itr.second);
      v->setType(VertexType::X);
    }
  }
  if(verbose >= 3) _simpGraph->printVertices();
}

