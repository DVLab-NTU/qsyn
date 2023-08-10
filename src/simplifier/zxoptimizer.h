/****************************************************************************
  FileName     [ optimizer.h ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_OPTIMIZER_H
#define ZX_OPTIMIZER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ZXGraph;

class ZXOPTimizer {
public:
    ZXOPTimizer() {
        init();
    }

    void setLastTCount(int tCnt) { _lTCnt = tCnt; }
    void setLastDensity(double ldensity) { _lDensity = ldensity; }
    void setLastEdgeCount(int edgeCnt) { _lEdgeCnt = edgeCnt; }
    void setLastVerticeCount(int vCnt) { _lVerticeCnt = vCnt; }
    void setR2R(const std::string& rule, int r2r) { _r2r[rule] = r2r; }
    void setS2S(const std::string& rule, int s2s) { _s2s[rule] = s2s; }

    int getLastTCount() { return _lTCnt; }
    double getLastDensity() { return _lDensity; }
    int getLastEdgeCount() { return _lEdgeCnt; }
    int getLastVerticeCount() { return _lVerticeCnt; }
    int getR2R(const std::string& rule) { return _r2r[rule]; }
    int getS2S(const std::string& rule) { return _s2s[rule]; }
    ZXGraph* getLastZXGraph() { return _lZXGraph; }

    bool updateParameters(ZXGraph* g);

    void init();
    void printSingle(const std::string& rule);
    void print();
    void myOptimize();
    // void storeStatus(ZXGraph* g);
    double calculateDensity(ZXGraph* g);

protected:
    int _lTCnt;
    int _lEdgeCnt;
    int _lVerticeCnt;
    double _lDensity;
    ZXGraph* _lZXGraph;
    std::unordered_set<std::string> _rules;
    std::unordered_map<std::string, int> _r2r;
    std::unordered_map<std::string, int> _s2s;
};

#endif
