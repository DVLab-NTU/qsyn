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
    if(verbose >= 5) cout << "> match...\n";

    unordered_map<size_t, size_t> id2idx;
    for(size_t i = 0; i < g->getNumVertices(); i++) id2idx[g->getVertices()[i]->getId()] = i;

    vector<bool> taken(g->getNumVertices(), false);
    vector<bool> discardEdges(g->getNumEdges(), false);
    vector<EdgePair> newEdges;

    // traverse edge
    for(size_t i = 0; i < g->getNumEdges(); i++){
        // if(discardEdges[i]) continue;
        EdgePair e = g->getEdges()[i];
        ZXVertex* vs = e.first.first; ZXVertex* vt = e.first.second;
        Phase vsp = vs->getPhase(); Phase vtp = vt->getPhase();

        if(taken[id2idx[vs->getId()]] || taken[id2idx[vt->getId()]]) continue;
        
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
        if(vtn.size() == 1) continue;   // It is a phase gadget

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
                // for(size_t nei = 0; nei < ne.size(); nei++){
                //     for(size_t t = 0; t < g->getNumEdges(); t++){
                //         if(g->getEdges()[t] == ne[nei]){
                //             if(verbose == 9){
                //                 cout << "> discard edges:\n";
                //                 g->printEdge(t);
                //             } 
                //             discardEdges[t] = true;
                //             break;
                //         }
                //     }
                // }
            
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

        for(auto& v : vsn) taken[id2idx[v->getId()]] = true;
        for(auto& v : vtn) taken[id2idx[v->getId()]] = true;
        

        ZXVertex* newVertex = g->addVertex(g->findNextId(), -2, VertexType::Z, vtp);
        vt->setPhase(Phase(0));
        vs->setQubit(-1);
        newEdges.push_back(make_pair(make_pair(newVertex, vt),new EdgeType(EdgeType::SIMPLE)));
        vector<ZXVertex*> match{vs, vt, newVertex};

        _matchTypeVec.emplace_back(match);

        i += 1;
    }

    for(auto& e : newEdges) g->addEdge(e.first.first, e.first.second, e.second);
    
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

    for (auto& m :  _matchTypeVec){

        if(verbose >= 5){
            cout << "> rewrite...\n";
            cout << "vs: " << m[0]->getId() << "\tvt: " << m[1]->getId() << "\tv_gadget: " << m[2]->getId() << endl;
        } 
        vector<ZXVertex*> n0 = m[0]->getNeighbors();
        vector<ZXVertex*> n1 = m[1]->getNeighbors();
        // cout << n0.size() << ' ' << n1.size() << endl;

        for(auto itr = n0.begin(); itr != n0.end();){
            if(n0[itr-n0.begin()] == m[1]){
                n0.erase(itr);
            }
            else itr++;
        }

        for(auto itr = n1.begin(); itr != n1.end();){
            if(n1[itr-n1.begin()] == m[0] || n1[itr-n1.begin()] == m[2]){
                n1.erase(itr);
            }
            else itr++;
        }

        // cout << n0.size() << ' ' << n1.size() << endl;

        vector<ZXVertex*> n2;
        set_intersection(n0.begin(), n0.end(), n1.begin(), n1.end(), back_inserter(n2));

        vector<ZXVertex*> n3, n4;
        set_difference(n0.begin(), n0.end(), n2.begin(), n2.end(), back_inserter(n3));
        set_difference(n1.begin(), n1.end(), n2.begin(), n2.end(), back_inserter(n4));
        n0 = n3; n1 = n4;
        n3.clear();n4.clear();

        // Add edge table
        for(auto& s : n0){
            for(auto& t : n1){
                _edgeTableKeys.push_back(make_pair(s, t));
                _edgeTableValues.push_back(make_pair(0,1));
            }
            for(auto& t : n2){
                _edgeTableKeys.push_back(make_pair(s, t));
                _edgeTableValues.push_back(make_pair(0,1));
            }
        }
        for(auto& s : n1){
            for(auto& t : n2){
                _edgeTableKeys.push_back(make_pair(s, t));
                _edgeTableValues.push_back(make_pair(0,1));
            }
        }

        int k0 = n0.size(), k1 = n1.size(), k2 = n2.size();

        for(auto& v : n2){
            // REVIEW - check if not ground
            v->setPhase(v->getPhase()+1);
        }

        // REVIEW - scalar add power


        for(int i = 0; i < 2; i++){
            Phase a = m[i]->getPhase();
            vector<ZXVertex*> target;
            target = (i == 0) ? n1 : n0;
            if(a != Phase(0)){
                for(auto& t : target){
                    // REVIEW - check if not ground
                    t->setPhase(t->getPhase()+a);
                }
                for(auto& t : n2){
                    // REVIEW - check if not ground
                    t->setPhase(t->getPhase()+a);
                }
            }

            if(i == 0) _removeVertices.push_back(m[1]);
            if(i == 1) {
                EdgePair e = g->getIncidentEdges(m[2])[0];
                _edgeTableKeys.push_back(make_pair(m[0], m[2]));
                if(*e.second == EdgeType::SIMPLE){
                    _edgeTableValues.push_back(make_pair(0, 1));
                }
                else{
                    _edgeTableValues.push_back(make_pair(1, 0));
                }
                _removeEdges.push_back(e);
            }
        }
    }
}