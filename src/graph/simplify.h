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
class Simplifier;
// enum class SIMP_STRATEGY;

// enum class SIMP_STRATEGY{
//     SPIDER_SIMP,        // spider fusion
//     ID_SIMP,            // identity removal
//     COPY_SIMP,          // pi copy
//     BIALG_SIMP,         // bialgebra
//     PHASE_FREE_SIMP,
//     PIVOT_SIMP,
//     PIVOT_GADGET_SIMP,
//     PIVOT_BOUNDARY_SIMP,
//     GADGET_SIMP,
//     LCOMP_SIMP

// };

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

class Simplifier{
    public:
        Simplifier(ZXGraph* g){
            _rule = nullptr;
            _simpGraph = g;
        }
        Simplifier(ZXRule* rule, ZXGraph* g){
            _rule = rule;
            _simpGraph = g;
        }
        ~Simplifier(){}

        void setRule(ZXRule* rule)          { _rule = rule; }

        // Simplification strategies
        int simp();
        int hadamard_simp();
        // int hadamard_simp(ZXGraph* g, Stats stats);

        // action
        void to_graph();
        void to_rgraph();


    private:
        ZXRule*             _rule;
        ZXGraph*            _simpGraph;
};
    
#endif