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

#include "optimizer.h"
#include "zxRules.h"  // for ZXRule
#include "zxoptimizer.h"

class ZXGraph;

class Simplifier {
public:
    Simplifier(ZXGraph* g, std::stop_token st): _rule{nullptr}, _simpGraph{g}, _stop_token{st} {
        hruleSimp();
    }
    Simplifier(std::unique_ptr<ZXRule> rule, ZXGraph* g): _rule{std::move(rule)}, _simpGraph{g} {}

    ZXRule* getRule() const { return _rule.get(); }

    void setRule(std::unique_ptr<ZXRule> rule) { _rule = std::move(rule); }

    void rewrite() { _rule->rewrite(_simpGraph); };
    void amend();
    // Simplification strategies
    int simp();
    int hadamardSimp();

    // Basic rules simplification
    int bialgSimp();
    int copySimp();
    int gadgetSimp();
    int hfusionSimp();
    int hruleSimp();
    int idSimp();
    int lcompSimp();
    int pivotSimp();
    int pivotBoundarySimp();
    int pivotGadgetSimp();
    // int degadgetizeSimp(std::stop_token);
    int sfusionSimp();

    // action
    void toGraph();
    void toRGraph();
    int interiorCliffordSimp();
    int piCliffordSimp();
    int cliffordSimp();
    void fullReduce();
    void dynamicReduce(int tOptimal = INT_MAX);
    void hybridReduce();
    void symbolicReduce();

    // print function
    void printRecipe();
    void printOptimizer();
    void getStepInfo(ZXGraph* g);

private:
    std::unique_ptr<ZXRule> _rule;
    ZXGraph* _simpGraph;
    std::vector<std::tuple<std::string, std::vector<int> > > _recipe;
    std::stop_token _stop_token;
};

#endif