/****************************************************************************
  FileName     [ simplify.h ]
  PackageName  [ simplifier ]
  Synopsis     [ Simplification strategies ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include "zxRules.h"  // for ZXRule

class ZXGraph;

class Simplifier {
public:
    Simplifier(ZXGraph* g) {
        _rule = nullptr;
        _simpGraph = g;
        _recipe.clear();
        hruleSimp();
    }
    Simplifier(ZXRule* rule, ZXGraph* g) {
        _rule = rule;
        _simpGraph = g;
        _recipe.clear();
    }
    ~Simplifier() { delete _rule; }

    void setRule(ZXRule* rule) {
        if (_rule != nullptr) delete _rule;
        _rule = rule;
    }

    void amend();
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
    void symbolicReduce();

    // print function
    void printRecipe();

private:
    ZXRule* _rule;
    ZXGraph* _simpGraph;
    vector<tuple<string, vector<int> > > _recipe;
};

#endif