/****************************************************************************
  FileName     [ simplify.h ]
  PackageName  [ graph ]
  Synopsis     [ Simplification strategies ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef SIMPLIFY_H
#define SIMPLIFY_H


#include <vector>
#include <iostream>
#include <unordered_map>

#include "zxDef.h"
#include "zxGraph.h"
#include "zxRules.h"

class Stats;
class Simplify;
enum class SIMP_STRATEGY;

enum class SIMP_STRATEGY{
    SPIDER_SIMP,        // spider fusion
    ID_SIMP,            // identity removal
    COPY_SIMP,          // pi copy
    BIALG_SIMP,         // bialgebra
    PHASE_FREE_SIMP,
    PIVOT_SIMP,
    PIVOT_GADGET_SIMP,
    PIVOT_BOUNDARY_SIMP,
    GADGET_SIMP,
    LCOMP_SIMP

};

class Stats{
    public:
        Stats(){
            _rewritesNum.clear();
        }
        ~Stats(){}
        void countRewrites(string rule, int n);

    private:
        unordered_map<string, int>          _rewritesNum;
};

class Simplify{
    public:
        Simplify(ZXGraph* g){
            _masterGraph = g->copy();
            _simplifyGraph = g->copy();
        }
        ~Simplify(){}

        void setMasterGraph(ZXGraph* g)     { _masterGraph = g; }
        void setSimplifyGraph(ZXGraph* g)   { _simplifyGraph = g; }
        ZXGraph* getMasterGraph()           { return _masterGraph; }
        ZXGraph* getSimplifyGraph()         { return _simplifyGraph; }


        // Simplification strategies
        int simp(ZXGraph* g, Stats stats, );

        // action
        void to_graph(ZXGraph* g);
        void to_rgraph(ZXGraph* g);


    private:
        ZXGraph*    _masterGraph;
        ZXGraph*    _simplifyGraph;
};
    
#endif