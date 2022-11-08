// /****************************************************************************
//   FileName     [ id.cpp ]
//   PackageName  [ simplifier ]
//   Synopsis     [ Identity Removal Rule Definition ]
//   Author       [ Cheng-Hua Lu ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

// #include <iostream>
// #include <vector>

// #include "zxRules.h"
// using namespace std;

// extern size_t verbose;

// /**
//  * @brief Finds non-interacting identity vertices.
//  *        (Check PyZX/pyzx/rules.py/match_ids_parallel for more details)
//  *
//  * @param g
//  */
// void IdRemoval::match(ZXGraph* g) {
//     _matchTypeVec.clear();
//     if(verbose >= 8) g->printVertices_depr();

//     unordered_map<size_t, size_t> id2idx;
//     for (size_t i = 0; i < g->getVertices_depr().size(); ++i) {
//         id2idx[g->getVertices_depr()[i]->getId()] = i;
//     }
//     vector<bool> valid(g->getVertices_depr().size(), true);
//     for (size_t i = 0; i < g->getVertices_depr().size(); ++i) {
//         if (!valid[i]) continue;
//         ZXVertex* v = g->getVertices_depr()[i];
//         NeighborMap_depr nmap = v->getNeighborMap();
//         if (v->getPhase() != Phase(0)) continue;
//         if (v->getType() != VertexType::Z && v->getType() != VertexType::X) continue;
//         if (nmap.size() != 2) continue;
    
//         ZXVertex* neigh0 = v->getNeighbor_depr(0);
//         ZXVertex* neigh1 = v->getNeighbor_depr(1);
//         EdgeType* etype0;
//         EdgeType* etype1;
//         if (neigh0 == neigh1) {
//             auto result = nmap.equal_range(neigh0);
//             auto itr0 = result.first;
//             auto itr1 = itr0; itr1++;
//             etype0 = itr0->second;
//             etype1 = itr1->second;
//         } else {
//             etype0 = nmap.find(neigh0)->second;
//             etype1 = nmap.find(neigh1)->second;
//         }
//         EdgeType  etype  = (*etype0 == *etype1) ? EdgeType::SIMPLE : EdgeType::HADAMARD;

//         _matchTypeVec.emplace_back(v, neigh0, neigh1, etype);

//         valid[id2idx[     v->getId()]] = false;
//         valid[id2idx[neigh0->getId()]] = false;
//         valid[id2idx[neigh1->getId()]] = false;
//     }
//     setMatchTypeVecNum(_matchTypeVec.size());
// }

// /**
//  * @brief Generate Rewrite format from `_matchTypeVec`
//  *        (Check PyZX/pyzx/rules.py/remove_ids for more details)
//  *
//  * @param g
//  */
// void IdRemoval::rewrite(ZXGraph* g) {
//     reset();
//     // TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
//     //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
//     //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
//     //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
//     //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
//     //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD
//     for (const auto& [v, n0, n1, et] : _matchTypeVec) {
//         _removeVertices.push_back(v);
//         _edgeTableKeys.emplace_back(n0, n1);
//         if (et == EdgeType::SIMPLE) {
//             _edgeTableValues.emplace_back(1, 0); 
//         } else {
//             _edgeTableValues.emplace_back(0, 1); 
//         }
//     } 

// }