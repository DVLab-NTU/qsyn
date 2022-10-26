/****************************************************************************
  FileName     [ pivotgadget.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include <numbers>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Determines which phase gadgets act on the same vertices, so that they can be fused together.
 *        (Check PyZX/pyzx/rules.py/match_phase_gadgets for more details)
 * 
 * @param g 
 */
void PivotGadget::match(ZXGraph* g){
    _matchTypeVec.clear(); 

    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    vector<bool> taken(g->getNumVertices(), false);
    vector<bool> discardEdges(g->getNumEdges(), false);
    vector<EdgePair> newEdges;

    // traverse edge
    for(size_t i = 0; i < g->getNumEdges(); i++){
        if(discardEdges[i]) continue;
        EdgePair e = g->getEdges()[i];
        ZXVertex* vs = e.first.first; ZXVertex* vt = e.first.second;
        Phase vsp = vs->getPhase(); Phase vtp = vt->getPhase();
        
        if(verbose == 9) cout << "\n-----------\n\n" <<
                                 "Edge " << i << ": " << vs->getId() << " " << vt->getId() << "\n";

        if(vs->getType() != VertexType::Z || vt->getType() != VertexType::Z) continue;

        if(verbose == 9) cout << "(1) type pass\n";

        if(vsp != Phase(0) && vsp != Phase(1)){
            if(vtp == Phase(0) || vtp == Phase(1)){
                vs = e.first.second; vt = e.first.first;
                vsp = vs->getPhase(); vtp = vt->getPhase();
            }
            else continue;
        }
        else if(vtp == Phase(0) || vtp == Phase(1)) continue;

        if(verbose == 9) cout << "(2) phase pass\n";

        //REVIEW - check  if(is_ground(vs)) continues;

        vector<ZXVertex*> vsn = vs->getNeighbors();
        vector<ZXVertex*> vtn = vt->getNeighbors();
        if(vtn.size() == 1) continue;   // It is phase gadget

        //SECTION - Start from line 375
        
        vector<vector<ZXVertex* > > vnList(1, vsn);
        vnList.push_back(vtn);

        bool bad_match = false;
        for(size_t vn = 0; vn < vnList.size(); vn++){
            for(size_t n = 0; n < vnList[vn].size(); n++){
                if(vnList[vn][n]->getType() != VertexType::Z){
                    bad_match = true;
                    break;
                }
                vector<EdgePair> ne = g->getIncidentEdges(vnList[vn][n]);
                if(vn == 0 && ne.size() == 1 && ne[0] != e){
                    bad_match = true;
                    break;
                }
                for(size_t nei = 0; nei < ne.size(); nei++){
                    for(size_t t = 0; t < g->getNumEdges(); t++){
                        if(g->getEdges()[t] == ne[nei]){
                            if(verbose == 9){
                                cout << "> discard edges:\n";
                                g->printEdge(t);
                            } 
                            discardEdges[t] = true;
                            break;
                        }
                    }
                }
            
            }
            if(bad_match) break;
        }
        if(bad_match) continue;
        
        if(verbose == 9) cout << "(3) Not a bad match\n";
        

        bool no_interior = false;
        for(size_t s = 0; s < vsn.size(); s++){
            if(vsn[s]->getType() != VertexType::Z){
                no_interior = true;
                break;
            }
        }
        if(no_interior) continue;
        for(size_t t = 0; t < vtn.size(); t++){
            if(vtn[t]->getType() != VertexType::Z){
                no_interior = true;
                break;
            }
        }
        if(no_interior) continue;

        
        // Both vs and vt are interior
        if(verbose >= 5) cout << "Edge vs and vt are both interior: " << vs->getId() << " " << vt->getId() << endl;

        ZXVertex* newVertex = g->addVertex(g->findNextId(), -2, VertexType::Z, vtp);
        vt->setPhase(Phase(0));
        vs->setQubit(-1);
        newEdges.push_back(make_pair(make_pair(newVertex, vt),new EdgeType(EdgeType::SIMPLE)));
        _matchTypeVec.push_back(make_pair(e, newVertex));

        i += 1;
    }
    g->addEdges(newEdges);
    newEdges.clear();
    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        
 * @param g 
 */
void PivotGadget::rewrite(ZXGraph* g){
    reset();
    //TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD


    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;
    vector<bool> isBoundary(g->getNumVertices(), false);

    for (auto& m :  _matchTypeVec){
        cout << m.first.first.first->getId() << " " << m.first.first.second->getId() << " " << m.second->getId() << endl;
        // 1 : get m0 m1
        // vector<ZXVertex*> neighbors;
        // neighbors.push_back(m.first.first.first);
        // neighbors.push_back(m.first.first.second);

        // 2 : boundary find
        // for(size_t j=0; j<2; j++){
        //     bool remove = true;

        //     for(auto& itr : neighbors[j]->getNeighborMap()){
        //         if(itr.first->getType() == VertexType::BOUNDARY){
        //             _edgeTableKeys.push_back(make_pair(neighbors[1-j], itr.first));
        //             if( * itr.second == EdgeType::SIMPLE)_edgeTableValues.push_back(make_pair(0,1));
        //             else _edgeTableValues.push_back(make_pair(1,0));
        //             remove = false;
        //             isBoundary[id2idx[itr.first->getId()]] = true;
        //             break;
        //         }
        //     }

        //     if(remove) _removeVertices.push_back(neighbors[1-j]);
        // }

        // 3 table of c
        // vector<int> c(g->getNumVertices() ,0);
        // vector<ZXVertex*> n0;
        // vector<ZXVertex*> n1;
        // vector<ZXVertex*> n2;
        // for(auto& x : neighbors[0]->getNeighbors()) {
        //     if (isBoundary[id2idx[x->getId()]]) continue;
        //     if (x == neighbors[1]) continue;
        //     c[id2idx[x->getId()]]++;
        // }
        // for(auto& x : neighbors[1]->getNeighbors()) {
        //     if (isBoundary[id2idx[x->getId()]]) continue;
        //     if (x == neighbors[0]) continue;
        //     c[id2idx[x->getId()]] += 2;
        // }

        // // 4  Find n0 n1 n2
        // for (size_t a=0; a<g->getNumVertices(); a++){
        //     if(c[a] == 1) n0.push_back(g->getVertices()[a]);
        //     else if (c[a] == 2) n1.push_back(g->getVertices()[a]);
        //     else if (c[a] == 3) n2.push_back(g->getVertices()[a]);
        //     else continue;
        // }
        
        // //// 5: scalar (skip)
        

        // //// 6:add phase
        // for(auto& x: n2){
        //     x->setPhase(x->getPhase() + Phase(1) + neighbors[0]->getPhase() + neighbors[1]->getPhase());
        // }

        // for(auto& x: n1){
        //     x->setPhase(x->getPhase() + neighbors[0]->getPhase());
        // }

        // for(auto& x: n0){
        //     x->setPhase(x->getPhase() + neighbors[1]->getPhase());
        // }

        // //// 7: connect n0 n1 n2
        // for(auto& itr : n0){
        //     if(isBoundary[itr->getId()]) continue;
        //     for(auto& a: n1){
        //         if(isBoundary[id2idx[a->getId()]]) continue;
        //         _edgeTableKeys.push_back(make_pair(itr, a));
        //         _edgeTableValues.push_back(make_pair(0,1));
        //     }
        //     for(auto& b: n2){
        //         if(isBoundary[id2idx[b->getId()]]) continue;
        //         _edgeTableKeys.push_back(make_pair(itr, b));
        //         _edgeTableValues.push_back(make_pair(0,1));
        //     }
        // }

        // for(auto& itr : n1){
        //     if(isBoundary[id2idx[itr->getId()]]) continue;
        //     for(auto& a: n2){
        //         if(isBoundary[id2idx[a->getId()]]) continue;
        //         _edgeTableKeys.push_back(make_pair(itr, a));
        //         _edgeTableValues.push_back(make_pair(0,1));
        //     }
        // }

        // //// 8: clear vector
        // neighbors.clear();
        // c.clear();
        // n0.clear();
        // n1.clear();
        // n2.clear();

    }

    // isBoundary.clear();
    
}