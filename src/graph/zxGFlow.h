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
    using Levels = std::vector<std::vector<ZXVertex*>>;
    
    ZXGFlow(ZXGraph* g): _zxgraph(g) {}

    void calculate();

    const Levels& getLevels() { return _levels; }


private:
    ZXGraph* _zxgraph;
    Levels _levels;
};

#endif // ZX_GFLOW_H