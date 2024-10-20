/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Scheduler structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "./circuit_topology.hpp"
#include "./duostra_def.hpp"
#include "./router.hpp"
#include "device/device.hpp"

namespace qsyn::duostra {

class BaseScheduler {
public:
    using Device = qsyn::device::Device;
    BaseScheduler(CircuitTopology topo, bool tqdm)
        : _circuit_topology(std::move(topo)), _tqdm(tqdm) {}
    virtual ~BaseScheduler() = default;

    BaseScheduler(BaseScheduler const& other) = default;
    BaseScheduler(BaseScheduler&& other)      = default;

    BaseScheduler& operator=(BaseScheduler const& other) = default;
    BaseScheduler& operator=(BaseScheduler&& other)      = default;

    virtual std::unique_ptr<BaseScheduler> clone() const;

    CircuitTopology& circuit_topology() { return _circuit_topology; }
    CircuitTopology const& circuit_topology() const { return _circuit_topology; }

    size_t get_final_cost() const;
    size_t get_total_time() const;
    size_t get_num_swaps() const;
    std::optional<size_t> get_executable_gate(Router& router) const;
    size_t get_operations_cost() const;
    bool is_sorted() const { return _sorted; }
    std::vector<size_t> const& get_available_gates() const { return _circuit_topology.get_available_gates(); }
    std::vector<GateInfo> const& get_operations() const { return _operations; }

    Device assign_gates_and_sort(std::unique_ptr<Router> router);
    size_t route_one_gate(Router& router, size_t gate_id, bool forget = false);

protected:
    CircuitTopology _circuit_topology;
    std::vector<GateInfo> _operations;
    bool _sorted = false;
    bool _tqdm   = true;
    virtual Device _assign_gates(std::unique_ptr<Router> router);
    void _sort();
};

class RandomScheduler : public BaseScheduler {
public:
    using Device = BaseScheduler::Device;
    RandomScheduler(CircuitTopology const& topo, bool tqdm) : BaseScheduler(topo, tqdm) {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
};

class NaiveScheduler : public BaseScheduler {
public:
    using Device = BaseScheduler::Device;
    NaiveScheduler(CircuitTopology const& topo, bool tqdm) : BaseScheduler(topo, tqdm) {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
};

struct GreedyConf {
    GreedyConf();

    MinMaxOptionType available_time_strategy;
    MinMaxOptionType cost_type;
    size_t num_candidates;
    size_t apsp_coeff;
};

class GreedyScheduler : public BaseScheduler {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    using Device = BaseScheduler::Device;
    GreedyScheduler(CircuitTopology const& topo, bool tqdm) : BaseScheduler(topo, tqdm) {}

    std::unique_ptr<BaseScheduler> clone() const override;
    size_t greedy_fallback(Router& router, std::vector<size_t> const& waitlist) const;

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
class TreeNode {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom

public:
    TreeNode(TreeNodeConf conf,
             size_t gate_id,
             std::unique_ptr<Router> router,
             std::unique_ptr<BaseScheduler> scheduler,
             size_t max_cost);
    TreeNode(TreeNodeConf conf,
             std::vector<size_t> gate_ids,
             std::unique_ptr<Router> router,
             std::unique_ptr<BaseScheduler> scheduler,
             size_t max_cost);

    ~TreeNode() = default;

    TreeNode(TreeNode const& other);
    TreeNode(TreeNode&& other) noexcept = default;

    void swap(TreeNode& other) noexcept {
        std::swap(_conf, other._conf);
        std::swap(_gate_ids, other._gate_ids);
        std::swap(_children, other._children);
        std::swap(_max_cost, other._max_cost);
        std::swap(_router, other._router);
        std::swap(_scheduler, other._scheduler);
    }

    friend void swap(TreeNode& a, TreeNode& b) noexcept {
        a.swap(b);
    }

    TreeNode& operator=(TreeNode copy) {
        copy.swap(*this);
        return *this;
    }

    bool is_leaf() const { return _children.empty(); }
    bool can_grow() const { return !scheduler().get_available_gates().empty(); }

    TreeNode best_child(size_t depth);
    size_t best_cost(size_t depth);
    size_t best_cost() const;

    Router const& router() const { return *_router; }
    BaseScheduler const& scheduler() const { return *_scheduler; }

    std::vector<size_t> const& get_executed_gates() const { return _gate_ids; }

    bool done() const { return scheduler().get_available_gates().empty(); }

private:
    TreeNodeConf _conf = {};

    // The head of the node.
    std::vector<size_t> _gate_ids;

    // Using vector to pointer so that frequent cache misses
    // won't be as bad in parallel code.
    std::vector<TreeNode> _children;

    // The state of duostra.
    size_t _max_cost{0};
    std::unique_ptr<Router> _router;
    std::unique_ptr<BaseScheduler> _scheduler;

    void _grow();
    std::optional<size_t> _immediate_next() const;
    void _route_internal_gates();
};

class SearchScheduler : public GreedyScheduler {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    using Device = GreedyScheduler::Device;
    SearchScheduler(CircuitTopology const& topo, bool tqdm = true);

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    bool _never_cache;
    bool _execute_single;
    size_t _lookahead;

    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
    void _cache_when_necessary();
};

class StarNode {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom

public:
    StarNode(size_t type,
             size_t gate_id,
             std::unique_ptr<Router> router,
             std::unique_ptr<BaseScheduler> scheduler,
             StarNode* parent);

    ~StarNode() = default;

    bool is_root() const { return _type == 0; }
    bool is_leaf() const { return _type == 2; }
    bool can_grow() const { return !scheduler().get_available_gates().empty(); }

    size_t get_gate_id() const { return _gate_id; }
    size_t get_estimated_cost() const { return _cost; }
    void grow(StarNode* self_pointer);
    StarNode* get_parent() const { return _parent; }
    
    Router const& router() const { return *_router; }
    BaseScheduler const& scheduler() const { return *_scheduler; }

    bool done() const { return scheduler().get_available_gates().empty(); }
    void delete_self() { _parent->delete_child(); delete this; }
    void delete_child() { _delete_count++; if(_delete_count==children.size() && !children.empty()) delete_self(); }

    std::vector<StarNode> children;

private:

    // The configuration of the node.
    // type of the node: 0 for root, 1 for internal, 2 for leaf.
    size_t _type = 0;

    // The head of the node.
    size_t _gate_id;

    // parent node
    StarNode* _parent = nullptr;

    // The estimated cost of the node.
    size_t _cost = 0;

    // number to record how many children have been delete
    size_t _delete_count = 0;

    // Using vector to pointer so that frequent cache misses
    // won't be as bad in parallel code.
    
    // The state of duostra.
    std::unique_ptr<Router> _router;
    std::unique_ptr<BaseScheduler> _scheduler;

    
    std::optional<size_t> _immediate_next() const;
    void _route_internal_gates();
    void _route_and_estimate();
};


class AStarScheduler : public GreedyScheduler {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
public:
    using Device = GreedyScheduler::Device;
    AStarScheduler(CircuitTopology const& topo, bool tqdm = true);

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    bool _never_cache;
    bool _execute_single;
    size_t _lookahead;

    Device _assign_gates(std::unique_ptr<Router> /*unused*/) override;
    void _cache_when_necessary();
};

std::unique_ptr<BaseScheduler> get_scheduler(std::unique_ptr<CircuitTopology> topo, bool tqdm = true);

}  // namespace qsyn::duostra
