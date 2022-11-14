// /****************************************************************************
//   FileName     [ sfusion.cpp ]
//   PackageName  [ simplifier ]
//   Synopsis     [ Spider Fusion Rule Definition ]
//   Author       [ Cheng-Hua Lu ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/

#include <iostream>
#include <vector>

#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Finds non-interacting matchings of the spider fusion rule.
 *        (Check PyZX/pyzx/rules.py/match_spider_parallel for more details)
 *
 * @param g
 */
void SpiderFusion::match(ZXGraph* g) {
    _matchTypeVec.clear();
    if(verbose >= 8) g->printVertices();

    unordered_map<EdgePair, size_t> Edge2idx;
    size_t i = 0;
    g -> forEachEdge([&Edge2idx, &i](const EdgePair& epair) {
        Edge2idx[epair] = i;
        i++;
    });

    vector<bool> validEdge(g->getNumEdges(), true);

    g -> forEachEdge([&Edge2idx, &validEdge, this](const EdgePair& epair) {
        
        if (!validEdge[Edge2idx[epair]]) return;
        validEdge[Edge2idx[epair]] = false;

        //NOTE - Skip Hadamard Edges
        if (epair.second != EdgeType::SIMPLE) return;
        ZXVertex* v0 = epair.first.first;
        ZXVertex* v1 = epair.first.second;

        //NOTE - Both X or Both Z
        if ((v0->getType() == v1->getType()) && (v0->getType() == VertexType::X || v0->getType() == VertexType::Z)) {
            const Neighbors& v0n = v0->getNeighbors();
            const Neighbors& v1n = v1->getNeighbors();
            //NOTE - Cannot choose the vertex connected to the vertices that will be merged
            for (auto itr = v0n.begin(); itr != v0n.end(); ++itr) {
                validEdge[Edge2idx[makeEdgePair(v0, itr->first, itr->second)]] = false;
            }
            for (auto itr = v1n.begin(); itr != v1n.end(); ++itr) {
                validEdge[Edge2idx[makeEdgePair(v1, itr->first, itr->second)]] = false;
                //NOTE - Cannot choose the vertex connected to the that will be deleted
                for (auto& jtr : (itr->first) -> getNeighbors()) {
                    validEdge[Edge2idx[makeEdgePair((itr->first), jtr.first, jtr.second)]] = false;
                }
            }
            _matchTypeVec.push_back(make_pair(v0, v1));
        }
    });

    if (verbose >= 5) cout << "Found " << _matchTypeVec.size() << " match(es) of spider fusion rule: " << endl;
    setMatchTypeVecNum(_matchTypeVec.size());
}

/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        (Check PyZX/pyzx/rules.py/spider for more details)
 *
 * @param g
 */
void SpiderFusion::rewrite(ZXGraph* g) {
    reset();
    // TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD
    
    for (size_t i = 0; i < _matchTypeVec.size(); i++) {

        //NOTE - No selfloops, directly merge the phase
        _matchTypeVec[i].first->setPhase(_matchTypeVec[i].first->getPhase() + _matchTypeVec[i].second->getPhase());

        ZXVertex* v0 = _matchTypeVec[i].first;
        ZXVertex* v1 = _matchTypeVec[i].second;
        Neighbors v1n = v1->getNeighbors();
        unordered_map<ZXVertex*, bool> done;
        // for(const auto& v: v1n) done[v.first] = false;
        
        for(auto& nbp: v1n){
            // if(done[nbp.first]) continue;
            //NOTE - No more selfloops
            //NOTE - Will become selfloop after merged, only considered hadamard
            if(nbp.first == v0){
                if(nbp.second == EdgeType::HADAMARD)
                    v0->setPhase(v0->getPhase() + Phase(1));
                //NOTE - No need to remove edges since v1 will be removed
            }
            else{
                _edgeTableKeys.push_back(make_pair(v0, nbp.first));
                _edgeTableValues.push_back(nbp.second == EdgeType::SIMPLE ? make_pair(1,0): make_pair(0,1));
            }
        }
        //NOTE - No selfloops, directly delete v1
        _removeVertices.push_back(v1);
    }
}

//? (Check PyZX/pyzx/rules.py/unspider if have done the above functions)
