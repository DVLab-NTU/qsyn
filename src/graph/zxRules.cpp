/****************************************************************************
  FileName     [ zxRules.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Definition for each rules in zxRules.h ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <iostream>
#include <vector>
#include <unordered_map>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Matches all the H-boxes with arity 2 and phase 1, i.e. all the Hadamard gates.
 * 
 * @param g 
 * @return vector<ZXVertex*> 
 */
vector<ZXVertex*> Hadamard::match(ZXGraph* g){
    // Data-preprocessing
    g->sortVerticeById();
    
    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    // Find all H-boxes
    vector<ZXVertex*> matches;
    vector<bool> taken(g->getNumVertices(), false);
    vector<bool> inMatches(g->getNumVertices(), false);
    for(size_t i = 0; i < g->getNumVertices(); i++){
        if(g->getVertices()[i]->getType() == VertexType::H_BOX && g->getVertices()[i]->getNeighborMap().size() == 2){
            vector<ZXVertex*> neighbors = g->getVertices()[i]->getNeighbors();
            size_t n0 = id2idx[neighbors[0]->getId()], n1 = id2idx[neighbors[1]->getId()];
            if(taken[n0] || taken[n1]) continue;
            if(!inMatches[n0] && !inMatches[n1]){
                matches.push_back(g->getVertices()[i]);
                inMatches[id2idx[g->getVertices()[i]->getId()]] = true;
                taken[n0] = true;
                taken[n1] = true;
            }
        }
    }
    if(verbose >= 3) cout << "Find match of hadamard-rule: " << matches.size() << endl;
    return matches;
}

/**
 * @brief Convert v in matches to Hadamard edge.
 * 
 * @param g 
 * @param matches 
 */
void Hadamard::hadamard2Edge(ZXGraph* g, vector<ZXVertex*> matches){
    for(size_t v = 0; v < matches.size(); v++){
        ZXVertex* n0 = matches[v]->getNeighbors()[0]; ZXVertex* n1 = matches[v]->getNeighbors()[1];
        g->addEdge(n0, n1, new EdgeType(EdgeType::HADAMARD));
        g->removeVertex(matches[v]);
    }
    if(verbose >= 3) cout << "Successfully change all H-boxes to HADAMARD EDGE:\t" << matches.size() << endl;
    if(verbose >= 3) g->printVertices();
}