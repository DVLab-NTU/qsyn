/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Scheduler structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "./circuit_topology.hpp"
#include "./router.hpp"
#include "device/device.hpp"

class BaseScheduler {
public:
    BaseScheduler(std::unique_ptr<CircuitTopology>, bool = true);
    BaseScheduler(BaseScheduler const&);
    BaseScheduler(BaseScheduler&&) = default;
    virtual ~BaseScheduler() {}

    virtual std::unique_ptr<BaseScheduler> clone() const;

    CircuitTopology& circuit_topology() { return *_circuit_topology; }
    CircuitTopology const& circuit_topology() const { return *_circuit_topology; }

    size_t get_final_cost() const;
    size_t get_total_time() const;
    size_t get_num_swaps() const;
    size_t get_executable(Router&) const;
    size_t get_operations_cost() const;
    bool is_sorted() const { return _sorted; }
    std::vector<size_t> const& get_available_gates() const { return _circuit_topology->get_available_gates(); }
    std::vector<Operation> const& get_operations() const { return _operations; }
    std::vector<size_t> const& get_order() const { return _assign_order; }

    Device assign_gates_and_sort(std::unique_ptr<Router>);
    size_t route_one_gate(Router&, size_t, bool = false);

protected:
    std::unique_ptr<CircuitTopology> _circuit_topology;
    std::vector<Operation> _operations;
    std::vector<size_t> _assign_order;
    bool _sorted = false;
    bool _tqdm = true;
    virtual Device _assign_gates(std::unique_ptr<Router>);
    void _sort();
};

class RandomScheduler : public BaseScheduler {
public:
    RandomScheduler(std::unique_ptr<CircuitTopology>, bool = true);
    RandomScheduler(RandomScheduler const&);
    RandomScheduler(RandomScheduler&&);
    ~RandomScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
};

class StaticScheduler : public BaseScheduler {
public:
    StaticScheduler(std::unique_ptr<CircuitTopology>, bool = true);
    StaticScheduler(StaticScheduler const&);
    StaticScheduler(StaticScheduler&&);
    ~StaticScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
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
    GreedyScheduler(std::unique_ptr<CircuitTopology>, bool = true);
    GreedyScheduler(GreedyScheduler const&);
    GreedyScheduler(GreedyScheduler&&);
    ~GreedyScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;
    size_t greedy_fallback(Router&, std::vector<size_t> const&, size_t) const;

protected:
    GreedyConf _conf;

    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
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
    TreeNode(TreeNode const&);
    TreeNode(TreeNode&&);

    TreeNode& operator=(TreeNode const&);
    TreeNode& operator=(TreeNode&&);

    bool is_leaf() const { return _children.empty(); }
    void grow_if_needed();
    bool can_grow() const;

    TreeNode best_child(int);
    size_t best_cost(int);
    size_t best_cost() const;

    Router const& router() const { return *_router; }
    BaseScheduler const& scheduler() const { return *_scheduler; }

    std::vector<size_t> const& get_executed_gates() const { return _gate_ids; }

    bool done() const { return scheduler().get_available_gates().empty(); }

private:
    TreeNodeConf _conf;

    // The head of the node.
    std::vector<size_t> _gate_ids;

    // Using vector to pointer so that frequent cache misses
    // won't be as bad in parallel code.
    std::vector<TreeNode> _children;

    // The state of duostra.
    size_t _max_cost;
    std::unique_ptr<Router> _router;
    std::unique_ptr<BaseScheduler> _scheduler;

    std::vector<TreeNode>&& _take_children();

    void _grow();
    size_t _immediate_next() const;
    void _route_internal_gates();
};

class SearchScheduler : public GreedyScheduler {
public:
    SearchScheduler(std::unique_ptr<CircuitTopology>, bool = true);
    SearchScheduler(SearchScheduler const&);
    SearchScheduler(SearchScheduler&&);
    ~SearchScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

    const size_t _lookAhead;

protected:
    bool _never_cache;
    bool _execute_single;

    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
    void _cache_when_necessary();
};

std::unique_ptr<BaseScheduler> get_scheduler(std::unique_ptr<CircuitTopology>, bool = true);
