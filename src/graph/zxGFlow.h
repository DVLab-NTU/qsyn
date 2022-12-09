/****************************************************************************
  FileName     [ zxGflow.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGFlow structure ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ZX_GFLOW_H
#define ZX_GFLOW_H

#include "zxGraph.h"
#include <vector>
#include <unordered_map>

class ZXGFlow {

public:
    ZXGFlow(ZXGraph* g): _zxg(g), _levels(), _currLevel(0) {}

    void calculate();

    std::vector<std::vector<ZXVertex*>> dump();

private:
    ZXGraph* _zxg;
    std::unordered_map<ZXVertex*, size_t> _levels;
    size_t _currLevel;
};

#endif // ZX_GFLOW_H