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

#include "optimizer.h"
#include "zxGraph.h"  // for ZXGraph

using namespace std;
extern size_t verbose;


void OPTimizer::init() {
    vector<string> vec{"Identity Removal Rule","Local Complementation Rule","Phase Gadget Rule","Pivot Rule","Pivot Gadget Rule","Pivot Boundary Rule","Spider Fusion Rule"};
    _rules.insert(vec.begin(), vec.end());

    for(auto& rule: _rules){
        setR2R(rule, INT_MAX);
        setS2S(rule, INT_MAX);
    }
}

void OPTimizer::printSingle(const string& rule) {
    if(_rules.find(rule) != _rules.end()){
        cout << rule << "(r2r, s2s): (";
        if(getR2R(rule) == INT_MAX) cout << "INF";
        else cout << getR2R(rule);
        cout << ", ";
        if(getS2S(rule) == INT_MAX) cout << "INF";
        else cout << getS2S(rule);
        cout << ")" << endl;
    }
}

void OPTimizer::print(){
    for(const string& rule : _rules) printSingle(rule);
}
