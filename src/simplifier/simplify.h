/****************************************************************************
  FileName     [ simplify.h ]
  PackageName  [ simplifier ]
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
            hruleSimp();
        }
        Simplifier(ZXRule* rule, ZXGraph* g){
            _rule = rule;
            _simpGraph = g;
        }
        ~Simplifier(){}

        void setRule(ZXRule* rule)          { _rule = rule; }

        // Simplification strategies
        int simp();
        int hadamardSimp();
        
        // Basic rules simplification
        int bialgSimp();
        int copySimp();
        int gadgetSimp();
        int hfusionSimp();
        int hopfSimp();
        int hruleSimp();
        int idSimp();
        int lcompSimp();
        int pivotSimp();
        int pivotBoundarySimp();
        int pivotGadgetSimp();
        int sfusionSimp();


        // action
        void toGraph();
        void toRGraph();
        int interiorCliffordSimp();
        int cliffordSimp();
        void fullReduce();
        void simulatedReduce();


        // print function
        void printRecipe();

    private:
        ZXRule*                         _rule;
        ZXGraph*                        _simpGraph;
        vector<tuple<string, int> >     _recipe;
};
    
#endif