/****************************************************************************
  FileName     [ simplify.h ]
  PackageName  [ simplifier ]
  Synopsis     [ Define class Simplifier structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include <memory>
#include <thread>

#include "zxRules.h"  // for ZXRule

class ZXGraph;

class Simplifier {
public:
    Simplifier(ZXGraph* g, std::stop_token st) {
        _rule = nullptr;
        _simpGraph = g;
        _recipe.clear();
        hruleSimp(st);
    }
    Simplifier(std::unique_ptr<ZXRule> rule, ZXGraph* g) {
        _rule = std::move(rule);
        _simpGraph = g;
        _recipe.clear();
    }

    ZXRule* getRule() const { return _rule.get(); }

    void setRule(std::unique_ptr<ZXRule> rule) { _rule = std::move(rule); }

    void rewrite() { _rule->rewrite(_simpGraph); };
    void amend();
    // Simplification strategies
    int simp(std::stop_token);
    int hadamardSimp(std::stop_token);

    // Basic rules simplification
    int bialgSimp(std::stop_token);
    int copySimp(std::stop_token);
    int gadgetSimp(std::stop_token);
    int hfusionSimp(std::stop_token);
    int hruleSimp(std::stop_token);
    int idSimp(std::stop_token);
    int lcompSimp(std::stop_token);
    int pivotSimp(std::stop_token);
    int pivotBoundarySimp(std::stop_token);
    int pivotGadgetSimp(std::stop_token);
    int degadgetizeSimp(std::stop_token);
    int sfusionSimp(std::stop_token);

    // action
    void toGraph();
    void toRGraph();
    int interiorCliffordSimp(std::stop_token);
    int cliffordSimp(std::stop_token);
    void fullReduce(std::stop_token);
    void symbolicReduce(std::stop_token);

    // print function
    void printRecipe();

private:
    std::unique_ptr<ZXRule> _rule;
    ZXGraph* _simpGraph;
    std::vector<std::tuple<std::string, std::vector<int> > > _recipe;
};

#endif