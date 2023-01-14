/****************************************************************************
  FileName     [ hopf.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Hopf Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

// #include <iostream>
// #include <vector>

// #include "zxRules.h"
// using namespace std;

// extern size_t verbose;

// /**
//  * @brief Finds same type (Z or X) spiders with even hadamard edges and cancel the edges; Find different types (Z and X) spiders with even simple edges and cancel them.
//  *
//  *
//  * @param g
//  */
// void Hopf::match(ZXGraph* g) {
//     // Should be run in graph-like
//     _matchTypeVec.clear();
//     if(verbose >= 8) g->printVertices_depr();

//     vector<ZXVertex*> Vertices = g->getVertices_depr();
//     unordered_map<ZXVertex*, size_t> Vertex2idx;
//     for (size_t i = 0; i < Vertices.size(); i++) Vertex2idx[Vertices[i]] = i;
//     vector<bool> validVertex(Vertices.size(), true);

//     for (size_t i = 0; i < g->getNumVertices_depr(); i++) {
//         ZXVertex* v = Vertices[i];
//         if (v->getType() == VertexType::BOUNDARY) {
//             validVertex[Vertex2idx[v]] = false;
//             continue;
//         }
//         if (!validVertex[Vertex2idx[v]]) continue;
//         vector<ZXVertex*> neighbors = v->getNeighbors_depr();
//         NeighborMap_depr nbm = Vertices[i]->getNeighborMap();

//         for (size_t j = 0; j < neighbors.size(); j++) {
//             if (neighbors[j]->getType() == VertexType::BOUNDARY) continue;
//             bool sameColor = v->getType() == neighbors[j]->getType();
//             size_t targetEdges = 0;
//             for (auto itr = nbm.begin(); itr != nbm.end(); itr++) {
//                 if (itr->first == neighbors[j]) {
//                     if (targetEdges >= 2) break;
//                     if (sameColor) {
//                         if (*(itr->second) == EdgeType::HADAMARD)
//                             targetEdges++;
//                     } else {
//                         if (*(itr->second) == EdgeType::SIMPLE)
//                             targetEdges++;
//                     }
//                 }
//             }
//             if (targetEdges >= 2) {
//                 _matchTypeVec.push_back(make_pair(v, neighbors[j]));
//                 validVertex[Vertex2idx[v]] = false;
//                 validVertex[Vertex2idx[neighbors[j]]] = false;
//                 break;
//             }
//         }
//     }
//     setMatchTypeVecNum(_matchTypeVec.size());
// }

// /**
//  * @brief Generate Rewrite format from `_matchTypeVec`
//  *
//  *
//  * @param g
//  */
// void Hopf::rewrite(ZXGraph* g) {
//     reset();
//     // Need to update global scalar and phase
//     for (size_t i = 0; i < _matchTypeVec.size(); i++) {
//         bool sameColor = _matchTypeVec[i].first->getType() == _matchTypeVec[i].second->getType();
//         if (sameColor) {
//             NeighborMap_depr nbm = _matchTypeVec[i].first->getNeighborMap();
//             auto results = nbm.equal_range(_matchTypeVec[i].second);
//             EdgeType* tmp = nullptr;
//             size_t n = 0;
//             for (auto itr = results.first; itr != results.second; itr++) {
//                 if (*(itr->second) == EdgeType::HADAMARD) {
//                     n++;
//                     if (n % 2 == 0) {
//                         if(_matchTypeVec[i].first == _matchTypeVec[i].second){
//                             _matchTypeVec[i].first->setPhase(_matchTypeVec[i].first->getPhase() + Phase(1));
//                         }
//                         _removeEdges.push_back(make_pair(make_pair(_matchTypeVec[i].first, _matchTypeVec[i].second), tmp));
//                         _removeEdges.push_back(make_pair(make_pair(_matchTypeVec[i].first, _matchTypeVec[i].second), itr->second));
//                     }
//                     tmp = itr->second;
//                 }
//             }
//         } else {
//             NeighborMap_depr nbm = _matchTypeVec[i].first->getNeighborMap();
//             auto results = nbm.equal_range(_matchTypeVec[i].second);
//             EdgeType* tmp = nullptr;
//             size_t n = 0;
//             for (auto itr = results.first; itr != results.second; itr++) {
//                 if (*(itr->second) == EdgeType::SIMPLE) {
//                     n++;
//                     if (n % 2 == 0) {
//                         _removeEdges.push_back(make_pair(make_pair(_matchTypeVec[i].first, _matchTypeVec[i].second), tmp));
//                         _removeEdges.push_back(make_pair(make_pair(_matchTypeVec[i].first, _matchTypeVec[i].second), itr->second));
//                     }
//                     tmp = itr->second;
//                 }
//             }
//         }
//     }
// }