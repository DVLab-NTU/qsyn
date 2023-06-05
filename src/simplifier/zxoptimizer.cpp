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


void OPTimizer::init() {
    vector<string> vec{"Identity Removal Rule","Local Complementation Rule","Phase Gadget Rule","Pivot Rule","Pivot Gadget Rule","Pivot Boundary Rule","Spider Fusion Rule","Interior Clifford Simp","Clifford Simp"};
    _rules.insert(vec.begin(), vec.end());

    for(auto& rule: _rules){
        if(!(rule == "Interior Clifford Simp" && rule == "Clifford Simp")) setS2S(rule, INT_MAX);
        setR2R(rule, INT_MAX);
    }
}

void OPTimizer::printSingle(const string& rule) {
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

void OPTimizer::print(){
    for(const string& rule : _rules) printSingle(rule);
}

void OPTimizer::myOptimize(){
    _candGraphs.clear();
    verbose = 3;

    for(int i = 1; i <= 3; i++){
        init();
        dmode = i;
        qcirMgr->getQCircuit()->ZXMapping();
        _candGraphs.push_back(zxGraphMgr->getGraph());
        ZXGraph* g = _candGraphs.back();
        g->printGraph();
        Simplifier s(g);
        if(dmode != 1) s.dynamicReduce();
        else s.fullReduce();
    }
    

    for(auto& g : _candGraphs){
        g->printGraph();
        Extractor ext(g->copy());
        QCir* newQC = ext.extract();
        Optimizer op(newQC);
        newQC = op.parseCircuit(false, false, 1000);
        newQC->printSummary();
        cout << endl;
    } 

}

void OPTimizer::storeStatus(ZXGraph* g){
    setLastTCount(g->TCount());
    setLastEdgeCount(g->getNumEdges());
    setLastVerticeCount(g->getNumVertices());
}