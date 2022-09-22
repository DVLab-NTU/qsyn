/****************************************************************************
  FileName     [ simplify.h ]
  PackageName  [ graph ]
  Synopsis     [ Simplification strategies ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include <iostream>
#include <vector>
#include "zxGraph.h"
#include "zxDef.h"

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


    private:
        ZXGraph*    _masterGraph;
        ZXGraph*    _simplifyGraph;
};
    
#endif