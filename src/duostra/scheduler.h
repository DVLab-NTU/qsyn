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
#include "router.h"
#include "topology.h"
#include "util.h"

class BaseScheduler {
public:
    BaseScheduler(const BaseScheduler& other);
    BaseScheduler(std::unique_ptr<CircuitTopo> topo);
    BaseScheduler(BaseScheduler&& other);
    virtual ~BaseScheduler() {}

    CircuitTopo& topo() { return *topo_; }
    const CircuitTopo& topo() const { return *topo_; }

    virtual std::unique_ptr<BaseScheduler> clone() const;

    void assign_gates_and_sort(std::unique_ptr<Router> router) {
        assign_gates(std::move(router));
        sort();
    }

    size_t get_final_cost() const;
    size_t get_total_time() const;
    size_t get_swap_num() const;
    const std::vector<size_t>& get_avail_gates() const {
        return topo_->get_avail_gates();
    }

    const std::vector<Operation>& get_operations() const { return ops_; }
    size_t ops_cost() const;

    size_t route_one_gate(Router& router,
                          size_t gate_idx,
                          bool forget = false);

    size_t get_executable(Router& router) const;

protected:
    std::unique_ptr<CircuitTopo> topo_;
    std::vector<Operation> ops_;
    bool sorted_ = false;

    virtual void assign_gates(std::unique_ptr<Router> router);
    void sort();
};

class RandomScheduler : public BaseScheduler {
public:
    RandomScheduler(std::unique_ptr<CircuitTopo> topo);
    RandomScheduler(const RandomScheduler& other);
    RandomScheduler(RandomScheduler&& other);
    ~RandomScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    void assign_gates(std::unique_ptr<Router> router) override;
};

class StaticScheduler : public BaseScheduler {
public:
    StaticScheduler(std::unique_ptr<CircuitTopo> topo);
    StaticScheduler(const StaticScheduler& other);
    StaticScheduler(StaticScheduler&& other);
    ~StaticScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    void assign_gates(std::unique_ptr<Router> router) override;
};

struct GreedyConf {
    GreedyConf()
        : avail_typ(true),
          cost_typ(false),
          candidates(ERROR_CODE),
          apsp_coef(1) {}

    // GreedyConf(const json& conf);

    bool avail_typ;  // true is max, false is min
    bool cost_typ;   // true is max, false is min
    size_t candidates;
    size_t apsp_coef;
};

class GreedyScheduler : public BaseScheduler {
public:
    GreedyScheduler(std::unique_ptr<CircuitTopo> topo);
    GreedyScheduler(const GreedyScheduler& other);
    GreedyScheduler(GreedyScheduler&& other);
    ~GreedyScheduler() override {}

    std::unique_ptr<BaseScheduler> clone() const override;
    size_t greedy_fallback(const Router& router,
                           const std::vector<size_t>& wait_list,
                           size_t gate_idx) const;

protected:
    GreedyConf conf_;

    void assign_gates(std::unique_ptr<Router> router) override;
};

struct TreeNodeConf {
    // Never cache any children unless children() is called.
    bool never_cache;
    // Execute the single gates when they are available.
    bool exec_single;
    // The number of childrens to consider,
    // selected with some ops_cost heuristic.
    size_t candidates;
};

// This is a node of the heuristic search tree.
class TreeNode {
public:
    TreeNode(TreeNodeConf conf,
             size_t gate_idx,
             std::unique_ptr<Router> router,
             std::unique_ptr<BaseScheduler> scheduler,
             size_t max_cost);
    TreeNode(TreeNodeConf conf,
             std::vector<size_t>&& gate_indices,
             std::unique_ptr<Router> router,
             std::unique_ptr<BaseScheduler> scheduler,
             size_t max_cost);
    TreeNode(const TreeNode& other);
    TreeNode(TreeNode&& other);

    TreeNode& operator=(const TreeNode& other);
    TreeNode& operator=(TreeNode&& other);

    TreeNode best_child(int depth);

    size_t best_cost(int depth);
    size_t best_cost() const;

    const Router& router() const { return *router_; }
    const BaseScheduler& scheduler() const { return *scheduler_; }

    const std::vector<size_t>& executed_gates() const { return gate_indices_; }

    bool done() const { return scheduler().get_avail_gates().empty(); }
    bool is_leaf() const { return children_.empty(); }
    void grow_if_needed();

private:
    TreeNodeConf conf_;

    // The head of the node.
    std::vector<size_t> gate_indices_;

    // Using vector to pointer so that frequent cache misses
    // won't be as bad in parallel code.
    std::vector<TreeNode> children_;

    // The state of duostra.
    size_t max_cost_;
    std::unique_ptr<Router> router_;
    std::unique_ptr<BaseScheduler> scheduler_;

    std::vector<TreeNode>&& children();

    void grow();
    void route_internal_gates();
    size_t immediate_next() const;
};

class SearchScheduler : public GreedyScheduler {
public:
    SearchScheduler(std::unique_ptr<CircuitTopo> topo);
    SearchScheduler(const SearchScheduler& other);
    SearchScheduler(SearchScheduler&& other);
    ~SearchScheduler() override {}

    const size_t look_ahead;

    std::unique_ptr<BaseScheduler> clone() const override;

protected:
    bool never_cache_;
    bool exec_single_;

    void assign_gates(std::unique_ptr<Router> router) override;
    void cache_only_when_necessary();
};

std::unique_ptr<BaseScheduler> getScheduler(const std::string& typ, std::unique_ptr<CircuitTopo> topo);

#endif