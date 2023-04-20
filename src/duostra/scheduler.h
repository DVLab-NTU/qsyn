/****************************************************************************
  FileName     [ scheduler.h ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Scheduler structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "circuitTopology.h"
#include "device.h"
#include "router.h"
#include "util.h"
#include "variables.h"

class BaseScheduler {
public:
    BaseScheduler(std::unique_ptr<CircuitTopo>, bool = true);
    BaseScheduler(const BaseScheduler&);
    BaseScheduler(BaseScheduler&&);
    virtual ~BaseScheduler() {}

    virtual std::unique_ptr<BaseScheduler> clone() const;

    CircuitTopo& circuitTopology() { return *_circuitTopology; }
    const CircuitTopo& circuitTopology() const { return *_circuitTopology; }

    size_t getFinalCost() const;
    size_t getTotalTime() const;
    size_t getSwapNum() const;
    size_t getExecutable(Router&) const;
    bool isSorted() const { return _sorted; }
    const std::vector<size_t>& getAvailableGates() const { return _circuitTopology->getAvailableGates(); }
    const std::vector<Operation>& getOperations() const { return _operations; }
    const std::vector<size_t>& getOrder() const { return _assignOrder; }
    size_t operationsCost() const;

    Device assignGatesAndSort(std::unique_ptr<Router>);
    size_t routeOneGate(Router&, size_t, bool = false);

protected:
    std::unique_ptr<CircuitTopo> _circuitTopology;
    std::vector<Operation> _operations;
    std::vector<size_t> _assignOrder;
    bool _sorted = false;
    bool _tqdm = true;
    virtual Device assignGates(std::unique_ptr<Router>);
    void sort();
};

class RandomScheduler : public BaseScheduler {
public:
    RandomScheduler(std::unique_ptr<CircuitTopo>, bool = true);
    RandomScheduler(const RandomScheduler&);
    RandomScheduler(RandomScheduler&&);
    ~RandomScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    Device assignGates(std::unique_ptr<Router>) override;
};

class StaticScheduler : public BaseScheduler {
public:
    StaticScheduler(std::unique_ptr<CircuitTopo>, bool = true);
    StaticScheduler(const StaticScheduler&);
    StaticScheduler(StaticScheduler&&);
    ~StaticScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    Device assignGates(std::unique_ptr<Router>) override;
};

struct GreedyConf {
    GreedyConf();

    bool _availableType;  // true is max, false is min
    bool _costType;       // true is max, false is min
    size_t _candidates;
    size_t _APSPCoeff;
};

class GreedyScheduler : public BaseScheduler {
public:
    GreedyScheduler(std::unique_ptr<CircuitTopo>, bool = true);
    GreedyScheduler(const GreedyScheduler&);
    GreedyScheduler(GreedyScheduler&&);
    ~GreedyScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;
    size_t greedyFallback(Router&, const std::vector<size_t>&, size_t) const;

protected:
    GreedyConf _conf;

    Device assignGates(std::unique_ptr<Router>) override;
};

struct TreeNodeConf {
    // Never cache any children unless children() is called.
    bool _neverCache;
    // Execute the single gates when they are available.
    bool _executeSingle;
    // The number of childrens to consider,
    // selected with some operationsCost heuristic.
    size_t _candidates;
};

// This is a node of the heuristic search tree.
class TreeNode {
public:
    TreeNode(TreeNodeConf, size_t, std::unique_ptr<Router>, std::unique_ptr<BaseScheduler>, size_t);
    TreeNode(TreeNodeConf, std::vector<size_t>&&, std::unique_ptr<Router>, std::unique_ptr<BaseScheduler>, size_t);
    TreeNode(const TreeNode&);
    TreeNode(TreeNode&&);

    TreeNode& operator=(const TreeNode&);
    TreeNode& operator=(TreeNode&&);

    bool isLeaf() const { return _children.empty(); }
    void growIfNeeded();
    bool canGrow() const;

    TreeNode bestChild(int);
    size_t bestCost(int);
    size_t bestCost() const;

    const Router& router() const { return *_router; }
    const BaseScheduler& scheduler() const { return *_scheduler; }

    const std::vector<size_t>& executedGates() const { return _gateIds; }

    bool done() const { return scheduler().getAvailableGates().empty(); }

private:
    TreeNodeConf _conf;

    // The head of the node.
    std::vector<size_t> _gateIds;

    // Using vector to pointer so that frequent cache misses
    // won't be as bad in parallel code.
    std::vector<TreeNode> _children;

    // The state of duostra.
    size_t _maxCost;
    std::unique_ptr<Router> _router;
    std::unique_ptr<BaseScheduler> _scheduler;

    std::vector<TreeNode>&& children();

    void grow();
    size_t immediateNext() const;
    void routeInternalGates();
};

class SearchScheduler : public GreedyScheduler {
public:
    SearchScheduler(std::unique_ptr<CircuitTopo>, bool = true);
    SearchScheduler(const SearchScheduler&);
    SearchScheduler(SearchScheduler&&);
    ~SearchScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

    const size_t _lookAhead;

protected:
    bool _neverCache;
    bool _executeSingle;

    Device assignGates(std::unique_ptr<Router>) override;
    void cacheOnlyWhenNecessary();
};

std::unique_ptr<BaseScheduler> getScheduler(std::unique_ptr<CircuitTopo>, bool = true);

#endif