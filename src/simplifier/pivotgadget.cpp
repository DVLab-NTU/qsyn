// /****************************************************************************
//   FileName     [ pivotgadget.cpp ]
//   PackageName  [ simplifier ]
//   Synopsis     [ Pivot Gadget Rule Definition ]
//   Author       [ Cheng-Hua Lu ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include <iostream>
#include <vector>
#include <numbers>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Check whether the match is a bad match
 * 
 * @param nebs 
 * @param e
 * @param isVsNeighbor
 * @return true if is a bad match
 * @return false if is a good match
 */
bool PivotGadget::checkBadMatch(const Neighbors& nebs, EdgePair e, bool isVsNeighbor){
    for(const auto& [v, et]: nebs){
        if(v->getType() != VertexType::Z)
            return true;
        NeighborPair nbp = v->getFirstNeighbor();
        if( isVsNeighbor && v->getNumNeighbors()==1 && makeEdgePair(v, nbp.first, nbp.second) != e)
            return true;     
    }
    return false;
}

/**
 * @brief Determines which phase gadgets act on the same vertices, so that they can be fused together.
 *        (Check PyZX/pyzx/rules.py/match_phase_gadgets for more details)
 * 
 * @param g 
 */
void PivotGadget::match(ZXGraph* g){
    _matchTypeVec.clear(); 
    if(verbose >= 8) g->printVertices();
    if(verbose >= 5) cout << "> match...\n";

    unordered_map<size_t, size_t> id2idx;
    size_t cnt = 0;
    for(const auto& v: g->getVertices()){
        id2idx[v->getId()] = cnt;
        cnt++;
    }

    vector<bool> taken(g->getNumVertices(), false);
    vector<Phase> verticesStorage;
    vector<pair<size_t, ZXVertex*>> edgesStorage;
    vector<pair<pair<ZXVertex*, ZXVertex*>, size_t>> matchesStorage;
    vector<EdgePair> newEdges;
    vector<ZXVertex*> newVertices;
    cnt = 0;
    g -> forEachEdge([&cnt, &id2idx, &taken, &verticesStorage, &edgesStorage, &matchesStorage, this](const EdgePair& epair) {
        ZXVertex* vs = epair.first.first; ZXVertex* vt = epair.first.second;
        Phase vsp = vs->getPhase(); Phase vtp = vt->getPhase();

        if(taken[id2idx[vs->getId()]] || taken[id2idx[vt->getId()]]) return;

        if(verbose == 9) cout << "\n-----------\n\n" <<
                                 "Edge " << cnt << ": " << vs->getId() << " " << vt->getId() << "\n";

        if(vs->getType() != VertexType::Z || vt->getType() != VertexType::Z) return;

        if(verbose == 9) cout << "(1) type pass\n";

        if(vsp != Phase(0) && vsp != Phase(1)){
            if(vtp == Phase(0) || vtp == Phase(1)){
                vs = epair.first.second; vt = epair.first.first;
                vsp = vs->getPhase(); vtp = vt->getPhase();
            }
            else return;
        }
        else if(vtp == Phase(0) || vtp == Phase(1)) return;

        if(verbose == 9) cout << "(2) phase pass\n";

        //REVIEW - check  if(is_ground(vs)) continues;
        // Neighbors vsn = vs->getNeighbors();
        // Neighbors vtn = vt->getNeighbors();
        if(vt->getNumNeighbors() == 1) return;   // It is a phase gadget

        //SECTION - Start from line 375
        if(checkBadMatch(vs->getNeighbors(), epair, true))
            return;
        if(checkBadMatch(vt->getNeighbors(), epair, false))
            return;

        if(verbose == 9) cout << "(3) Not a bad match\n";
        
        bool no_interior = false;
        for(const auto& [v, et]: vs->getNeighbors()){
            if(v->getType() != VertexType::Z){
                no_interior = true;
                break;
            }
        }
        if(no_interior) return;
        for(const auto& [v, et]: vt->getNeighbors()){
            if(v->getType() != VertexType::Z){
                no_interior = true;
                break;
            }
        }
        if(no_interior) return;
        
        // Both vs and vt are interior
        if(verbose >= 5) cout << "Edge vs and vt are both interior: " << vs->getId() << " " << vt->getId() << endl;

        for(auto& [v, _] : vs->getNeighbors()) taken[id2idx[v->getId()]] = true;
        for(auto& [v, _] : vt->getNeighbors()) taken[id2idx[v->getId()]] = true;
        
        //NOTE - ZXVertex*:  vtp []
        //NOTE - Edge: newV, vt
        //NOTE - match: vs vt newV
        size_t addId = verticesStorage.size();
        verticesStorage.push_back(vtp);
        edgesStorage.push_back(make_pair(addId, vt));
        matchesStorage.push_back(make_pair(make_pair(vs, vt), addId));
        // ZXVertex* newVertex = g->addVertex(-2, VertexType::Z, vtp);
        vt->setPhase(Phase(0));
        vs->setQubit(-1);
        // newEdges.push_back(make_pair(make_pair(newVertex, vt), EdgeType(EdgeType::SIMPLE)));
        // vector<ZXVertex*> match{vs, vt, newVertex};

        // _matchTypeVec.emplace_back(match);

        //REVIEW - Not sure why additional ++ is needed
        // cnt += 1;

        cnt++;
    });

    unordered_map<size_t, ZXVertex*> table;
    for(size_t i=0; i<verticesStorage.size(); i++) {
        table[i] = g->addVertex(-2, VertexType::Z, verticesStorage[i]);
    }
    for(auto& [idx, vt]: edgesStorage){
        g->addEdge(table[idx], vt, EdgeType(EdgeType::SIMPLE));
    }
    for(auto& [vpairs, idx]: matchesStorage){
        vector<ZXVertex*> match{vpairs.first, vpairs.second, table[idx]};
        _matchTypeVec.emplace_back(match);
    }
    for(auto& e : newEdges) g->addEdge(e.first.first, e.first.second, e.second);
    
    // newEdges.clear();
    
    setMatchTypeVecNum(_matchTypeVec.size());
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        
 * @param g 
 */
void PivotGadget::rewrite(ZXGraph* g){
    reset();

    for (auto& m :  _matchTypeVec){

        if(verbose >= 5){
            cout << "> rewrite...\n";
            cout << "vs: " << m[0]->getId() << "\tvt: " << m[1]->getId() << "\tv_gadget: " << m[2]->getId() << endl;
        } 
        vector<ZXVertex*> n0 = m[0]->getCopiedNeighbors();
        vector<ZXVertex*> n1 = m[1]->getCopiedNeighbors();
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
                if(s->getId()==t->getId()) {
                    s->setPhase(s->getPhase()+Phase(1));
                }
                else{
                    _edgeTableKeys.push_back(make_pair(s, t));
                    _edgeTableValues.push_back(make_pair(0,1));
                }
            }
            for(auto& t : n2){
                if(s->getId()==t->getId()) {
                    s->setPhase(s->getPhase()+Phase(1));
                }
                else{
                    _edgeTableKeys.push_back(make_pair(s, t));
                    _edgeTableValues.push_back(make_pair(0,1));
                }
            }
        }
        for(auto& s : n1){
            for(auto& t : n2){
                if(s->getId()==t->getId()) {
                    s->setPhase(s->getPhase()+Phase(1));
                }
                else{
                    _edgeTableKeys.push_back(make_pair(s, t));
                    _edgeTableValues.push_back(make_pair(0,1));
                }
            }
        }

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
                // EdgePair e = g->getIncidentEdges(m[2])[0];
                NeighborPair ep = m[2]->getFirstNeighbor();
                EdgePair e = makeEdgePair(m[2], ep.first, ep.second);
                // EdgePair_depr e = g->getFirstIncidentEdge_depr(m[2]);
                _edgeTableKeys.push_back(make_pair(m[0], m[2]));
                if(e.second == EdgeType::SIMPLE){
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