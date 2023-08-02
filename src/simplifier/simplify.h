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

#include "stop_token.hpp"
#include "zxRules.h"
#include "zxoptimizer.h"

class ZXGraph;

class Simplifier {
public:
    Simplifier(ZXGraph* g, mythread::stop_token st = mythread::stop_token{}) : _rule{nullptr}, _simpGraph{g}, _stop_token{st} {
        hruleSimp();
    }
    Simplifier(std::unique_ptr<ZXRule> rule, ZXGraph* g) : _rule{std::move(rule)}, _simpGraph{g} {}

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
    // int degadgetizeSimp(mythread::stop_token);
    int sfusionSimp();

    // action
    void toGraph();
    void toRGraph();
    int interiorCliffordSimp();
    int piCliffordSimp();
    int cliffordSimp();
    void fullReduce();
    void dynamicReduce();
    void dynamicReduce(size_t tOptimal);
    void symbolicReduce();
    void partitionReduce(size_t numPartitions, size_t iterations);

    // print function
    void printRecipe();
    void printOptimizer();
    void getStepInfo(ZXGraph* g);

private:
    std::unique_ptr<ZXRule> _rule;
    ZXGraph* _simpGraph;
    std::vector<std::tuple<std::string, std::vector<int> > > _recipe;
    mythread::stop_token _stop_token;
};

#endif
