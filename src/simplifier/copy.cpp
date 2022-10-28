/****************************************************************************
  FileName     [ copy.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ State Copy Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <iostream>
#include <vector>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Finds spiders with a 0 or pi phase that have a single neighbor, and copies them through. Assumes that all the spiders are green and maximally fused.
 *        (Check PyZX/pyzx/rules.py/match_copy for more details)
 * 
 * @param g 
 */
void StateCopy::match(ZXGraph* g){
  // Should be run in graph-like
  _matchTypeVec.clear();
  if(verbose >= 8) g->printVertices();
  
  vector<ZXVertex*> Vertices = g->getVertices();
  unordered_map<ZXVertex* , size_t> Vertex2idx;
  for(size_t i = 0; i < Vertices.size(); i++) Vertex2idx[Vertices[i]] = i;
  vector<bool> validVertex(Vertices.size(), true);
  for(size_t i=0; i<g->getNumVertices(); i++){
    if(!validVertex[Vertex2idx[Vertices[i]]]) continue;
    if(Vertices[i]->getType() != VertexType::Z){
      validVertex[Vertex2idx[Vertices[i]]] = false;
      continue;
    }
    if(Vertices[i]->getPhase() != Phase(0) && Vertices[i]->getPhase() != Phase(1)){
      validVertex[Vertex2idx[Vertices[i]]] = false;
      continue;
    }
    if(Vertices[i]->getNumNeighbors()!=1){
      validVertex[Vertex2idx[Vertices[i]]] = false;
      continue;
    }
    ZXVertex* PiNeighbor = Vertices[i]->getNeighbor(0);
    if(PiNeighbor->getType() != VertexType::Z){
      validVertex[Vertex2idx[Vertices[i]]] = false;
      continue;
    }
    vector<ZXVertex*> allNeighbors = PiNeighbor->getNeighbors();
    vector<ZXVertex*> applyNeighbors;
    for(size_t j=0; j<allNeighbors.size(); j++){
      if(allNeighbors[j]!=Vertices[i])
        applyNeighbors.push_back(allNeighbors[j]);
      validVertex[Vertex2idx[allNeighbors[j]]] = false;
    }
    _matchTypeVec.push_back(make_tuple(Vertices[i], PiNeighbor, applyNeighbors));
  }
  setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        (Check PyZX/pyzx/rules.py/apply_copy for more details)
 * 
 * @param g 
 */
void StateCopy::rewrite(ZXGraph* g){
  reset();
  //TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
  //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
  //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
  //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
  //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
  //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD
  
  // Need to update global scalar and phase
  for(size_t i=0; i<_matchTypeVec.size(); i++){
    ZXVertex* npi = get<0>(_matchTypeVec[i]);
    ZXVertex* a = get<1>(_matchTypeVec[i]);
    vector<ZXVertex*> neighbors = get<2>(_matchTypeVec[i]);
    _removeVertices.push_back(npi);
    _removeVertices.push_back(a);
    for(size_t i=0; i<neighbors.size(); i++){
      if(neighbors[i]->getType()==VertexType::BOUNDARY){
        ZXVertex* newV = g->addVertex(g->findNextId(), neighbors[i]->getQubit(), VertexType::Z, npi->getPhase());
        bool simpleEdge = false;
        if(*(neighbors[i]->getNeighborMap().begin()->second) == EdgeType::SIMPLE)
          simpleEdge = true;
        _removeEdges.push_back(make_pair(make_pair(a ,neighbors[i]), neighbors[i]->getNeighborMap().begin()->second));
        
        // new to Boundary
        _edgeTableKeys.push_back(make_pair(newV, neighbors[i]));
        _edgeTableValues.push_back(simpleEdge ? make_pair(0,1): make_pair(1,0));

        // a to new
        _edgeTableKeys.push_back(make_pair(a, newV));
        _edgeTableValues.push_back(make_pair(0,1));
        
      }
      else{
        neighbors[i]->setPhase(npi->getPhase()+neighbors[i]->getPhase());
      }
    }
  }
    
}