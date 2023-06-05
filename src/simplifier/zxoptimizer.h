/****************************************************************************
  FileName     [ optimizer.h ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "zxRules.h"

class ZXGraph;

class OPTimizer {
public:
    OPTimizer(){
        init();
    }
    void setLastTCount(int tCnt) { _lTCnt = tCnt; }
    void setLastEdgeCount(int edgeCnt) { _lEdgeCnt = edgeCnt; }
    void setLastVerticeCount(int vCnt) { _lVerticeCnt = vCnt; }
    void setR2R(const std::string& rule, int r2r) { _r2r[rule] = r2r; }
    void setS2S(const std::string& rule, int s2s) { _s2s[rule] = s2s; }

    int getLastTCount() { return _lTCnt; }
    int getLastEdgeCount() { return _lEdgeCnt; }
    int getLastVerticeCount() { return _lVerticeCnt; }
    int getR2R(const std::string& rule) { return _r2r[rule]; }
    int getS2S(const std::string& rule) { return _s2s[rule]; }


    void init();
    void printSingle(const std::string& rule);
    void print();
    void myOptimize();
    void storeStatus(ZXGraph* g);

protected:
    int _lTCnt;
    int _lEdgeCnt;
    int _lVerticeCnt;
    std::vector<ZXGraph* > _candGraphs;
    std::unordered_set<std::string> _rules;
    std::unordered_map<std::string, int> _r2r;
    std::unordered_map<std::string, int> _s2s;
};

#endif
