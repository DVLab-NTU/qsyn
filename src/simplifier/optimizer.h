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
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "zxRules.h"

class ZXGraph;

class OPTimizer {
public:
    OPTimizer() {
        init();
    }
    void setR2R(const std::string& rule, int r2r) { _r2r[rule] = r2r; }
    void setS2S(const std::string& rule, int s2s) { _s2s[rule] = s2s; }

    int getR2R(const std::string& rule) { return _r2r[rule]; }
    int getS2S(const std::string& rule) { return _s2s[rule]; }

    void init();
    void printSingle(const std::string& rule);
    void print();

protected:
    std::unordered_set<std::string> _rules;
    std::unordered_map<std::string, int> _r2r;
    std::unordered_map<std::string, int> _s2s;
};

#endif
