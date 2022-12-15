/****************************************************************************
  FileName     [ gFlow.h ]
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow structure ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef GFLOW_H
#define GFLOW_H

#include "zxGraph.h"
#include <vector>
#include <unordered_map>

class GFlow {
public:
    using Levels = std::vector<std::vector<ZXVertex*>>;
    using CorrectionSet = std::unordered_map<ZXVertex*, std::vector<ZXVertex*>>;
    
    GFlow(ZXGraph* g): _zxgraph(g) {}

    void calculate();

    const Levels& getLevels() { return _levels; }

    void print() const;

    void printLevels() const;

private:
    ZXGraph* _zxgraph;
    Levels _levels;
    CorrectionSet _correctionSet;
};

#endif // GFLOW_H