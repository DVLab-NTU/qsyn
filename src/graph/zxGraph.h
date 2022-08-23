/****************************************************************************
  FileName     [ zxGraph.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph structures ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ZX_GRAPH_H
#define ZX_GRAPH_H

#include <iostream>
#include <vector>
#include <string>
using namespace std;

struct VerType;
struct EdgeType;
class ZXGraph;


//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------
struct VerType{
    size_t _idx;
};
struct EdgeType{
    size_t _idx;
    pair<VerType, VecType> _edge;
};

class ZXGraph{
    public:
        ZXGraph(size_t id, vector<>){

        }

    private:

}
