/****************************************************************************
  FileName     [ optimizer.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>

#include <unordered_map>
#include <unordered_set>

#include "simplify.h"
#include "zxoptimizer.h"
#include "zxGraph.h"  // for ZXGraph
#include "qcirMgr.h"    // for QCirMgr
#include "zxGraphMgr.h"  // for ZXGraphMgr
#include "extract.h"
#include "optimizer.h"

using namespace std;

extern size_t verbose;
extern size_t dmode;
extern QCirMgr *qcirMgr;
extern ZXGraphMgr *zxGraphMgr;


void ZXOPTimizer::init() {
    vector<string> vec{"Identity Removal Rule","Local Complementation Rule","Phase Gadget Rule","Pivot Rule","Pivot Gadget Rule","Pivot Boundary Rule","Spider Fusion Rule","Interior Clifford Simp","Clifford Simp"};
    _rules.insert(vec.begin(), vec.end());

    for(auto& rule: _rules){
        if(!(rule == "Interior Clifford Simp" && rule == "Clifford Simp")) setS2S(rule, INT_MAX);
        setR2R(rule, INT_MAX);
    }
    _lDensity = -1;
    _lTCnt = -1;
    _lEdgeCnt = -1;
    _lVerticeCnt = -1;
}

void ZXOPTimizer::printSingle(const string& rule) {
    if(_rules.find(rule) != _rules.end()){
        cout << rule << "(r2r, s2s): (";
        if(getR2R(rule) == INT_MAX) cout << "INF";
        else cout << getR2R(rule);
        cout << ", ";
        if(rule == "Interior Clifford Simp" || rule == "Clifford Simp") cout << "-";
        else if(getS2S(rule) == INT_MAX) cout << "INF";
        else cout << getS2S(rule);
        cout << ")" << endl;
    }
}

void ZXOPTimizer::print(){
    for(const string& rule : _rules) printSingle(rule);
}

void ZXOPTimizer::myOptimize(){
    
    // verbose = 3;

    // for(int i = 1; i <= 3; i++){
    //     init();
    //     dmode = i;
    //     qcirMgr->getQCircuit()->ZXMapping();
    //     _candGraphs.push_back(zxGraphMgr->getGraph());
    //     ZXGraph* g = _candGraphs.back();
    //     g->printGraph();
    //     Simplifier s(g);
    //     if(dmode != 1) s.dynamicReduce();
    //     else s.fullReduce();
    // }
    

    // for(auto& g : _candGraphs){
    //     g->printGraph();
    //     Extractor ext(g->copy());
    //     QCir* newQC = ext.extract();
    //     Optimizer op(newQC);
    //     newQC = op.parseCircuit(false, false, 1000);
    //     newQC->printSummary();
    //     cout << endl;
    // } 

}

// void ZXOPTimizer::storeStatus(ZXGraph* g){
//     setLastTCount(g->TCount());
//     setLastEdgeCount(g->getNumEdges());
//     setLastVerticeCount(g->getNumVertices());
// }

double ZXOPTimizer::calculateDensity(ZXGraph* g){
    unordered_map<int, int> mp;
    for(auto& v : g->getVertices()) mp[v->getNumNeighbors()]++;
    double ans = 0;
    for(auto& i : mp) ans += (i.first*i.first*i.second);
    ans /= g->getNumVertices();
    return ans;
}

bool ZXOPTimizer::updateParameters(ZXGraph* g) {
    bool stop = false;
    if(_lTCnt != -1){
        if(_lTCnt == g->TCount()){
            double newDensity = calculateDensity(g);
            // cout << _lDensity << " " << newDensity << "= " << (newDensity - _lDensity)/_lDensity << " -> ";
            if((newDensity - _lDensity)/_lDensity > 0.2) stop = true;
            // cout << stop << endl;
            _lDensity = newDensity;
        }
        else _lDensity = calculateDensity(g);
        _lTCnt = g->TCount();
        _lEdgeCnt = g->getNumEdges();
        _lVerticeCnt = g->getNumVertices();
    }
    else{
        _lTCnt = g->TCount();
        _lEdgeCnt = g->getNumEdges();
        _lVerticeCnt = g->getNumVertices();
        _lDensity = calculateDensity(g);
        _lZXGraph = g;
    }
    cout << "# " << _lVerticeCnt << " vertices, " << _lEdgeCnt << " edges, " << _lTCnt << " T-gates, Density = " << _lDensity << " -> " << stop << endl << endl;
    return stop;
}