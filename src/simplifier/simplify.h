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

#include "cmdParser.h"
#include "stop_token.hpp"
#include "zxRules.h"
#include "zxoptimizer.h"

extern ZXOPTimizer opt;
extern CmdParser cli;

class ZXGraph;

class Simplifier {
public:
    Simplifier(ZXGraph* g) : _rule{nullptr}, _simpGraph{g} {
        hruleSimp();
    }
    Simplifier(std::unique_ptr<ZXRule> rule, ZXGraph* g) : _rule{std::move(rule)}, _simpGraph{g} {}

    ZXRule* getRule() const { return _rule.get(); }

    void setRule(std::unique_ptr<ZXRule> rule) { _rule = std::move(rule); }

    void rewrite() { _rule->rewrite(_simpGraph); };
    void amend();

    template <typename Rule>
    size_t new_simp(const Rule& rule) {
        size_t iterations = 0;
        for (int r2r = opt.getR2R(rule.name); !cli.stop_requested() && r2r > 0; r2r--) {
            std::vector<typename Rule::MatchType> matches = rule.findMatches(*_simpGraph);
            if (matches.empty()) {
                break;
            }
            rule.apply(*_simpGraph, matches);
            iterations++;
        }
        return iterations;
    }

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
};

#endif
