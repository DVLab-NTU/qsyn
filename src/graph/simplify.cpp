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

void Stats::countRewrites(string rule, int n){
    if(_rewritesNum.find(rule) != _rewritesNum.end()) _rewritesNum[rule] += n;
    else _rewritesNum[rule] = n;
}



/******************************************/
/*     class Simplify member functions    */
/******************************************/


// int Simplify::simp(ZXGraph* g, Stats stats, MatchTypeVec match){}

int Simplify::hadamard_simp(ZXGraph* g, Stats stats){
  Hadamard rule;
  vector<ZXVertex*> matches = rule.match(g);
  rule.hadamard2Edge(g, matches);
  return matches.size();
}



/**
 * @brief Turns every red node(VertexType::X) into green node(VertexType::Z) by regular simple edges <--> hadamard edges.
 * 
 * @param g ZXGraph*
 */
void Simplify::to_graph(ZXGraph* g){
    for(size_t i = 0; i < g->getNumVertices(); i++){
      ZXVertex* v = g->getVertices()[i];
      if(v->getType() == VertexType::X){
        for(auto& itr : v->getNeighborMap()) *itr.second = toggleEdge(*itr.second);
        v->setType(VertexType::Z);
      }
    }
    if(verbose >= 3) g->printVertices();
}

/**
 * @brief Turn green nodes into red nodes by color-changing vertices which greedily reducing the number of Hadamard-edges.
 * 
 * @param g 
 */
void Simplify::to_rgraph(ZXGraph* g){
  for(size_t i = 0; i < g->getNumVertices(); i++){
    ZXVertex* v = g->getVertices()[i];
    if(v->getType() == VertexType::Z){
      for(auto& itr : v->getNeighborMap()) *itr.second = toggleEdge(*itr.second);
      v->setType(VertexType::X);
    }
  }
  if(verbose >= 3) g->printVertices();
}

