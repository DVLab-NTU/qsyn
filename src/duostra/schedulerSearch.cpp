/****************************************************************************
  FileName     [ schedulerSearch.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Searcg Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <omp.h>

#include <algorithm>
#include <functional>
#include <unordered_set>
#include <vector>

#include "scheduler.h"

using namespace std;

TreeNode::TreeNode(TreeNodeConf conf, size_t gate_idx, unique_ptr<Router> router, unique_ptr<BaseScheduler> scheduler, size_t max_cost)
    : TreeNode(conf, vector<size_t>{gate_idx}, move(router), move(scheduler), max_cost) {}

TreeNode::TreeNode(TreeNodeConf conf, vector<size_t>&& gate_indices, unique_ptr<Router> router, unique_ptr<BaseScheduler> scheduler, size_t max_cost)
    : conf_(conf),
      gate_indices_(move(gate_indices)),
      children_({}),
      max_cost_(max_cost),
      router_(move(router)),
      scheduler_(move(scheduler)) {
    route_internal_gates();
}

TreeNode::TreeNode(const TreeNode& other)
    : conf_(other.conf_),
      gate_indices_(other.gate_indices_),
      children_(other.children_),
      max_cost_(other.max_cost_),
      router_(other.router_->clone()),
      scheduler_(other.scheduler_->clone()) {}

TreeNode::TreeNode(TreeNode&& other)
    : conf_(other.conf_),
      gate_indices_(move(other.gate_indices_)),
      children_(move(other.children_)),
      max_cost_(other.max_cost_),
      router_(move(other.router_)),
      scheduler_(move(other.scheduler_)) {}

TreeNode& TreeNode::operator=(const TreeNode& other) {
    conf_ = other.conf_;
    gate_indices_ = other.gate_indices_;
    children_ = other.children_;
    max_cost_ = other.max_cost_;
    router_ = other.router_->clone();
    scheduler_ = other.scheduler_->clone();
    return *this;
}

TreeNode& TreeNode::operator=(TreeNode&& other) {
    conf_ = other.conf_;
    gate_indices_ = move(other.gate_indices_);
    children_ = move(other.children_);
    max_cost_ = other.max_cost_;
    router_ = move(other.router_);
    scheduler_ = move(other.scheduler_);
    return *this;
}

vector<TreeNode>&& TreeNode::children() {
    grow_if_needed();
    return move(children_);
}

size_t TreeNode::immediate_next() const {
    size_t gate_idx = scheduler().get_executable(*router_);
    const auto& avail_gates = scheduler().get_avail_gates();

    if (gate_idx != ERROR_CODE) {
        return gate_idx;
    }

    if (avail_gates.size() == 1) {
        return avail_gates[0];
    }

    return ERROR_CODE;
}

void TreeNode::route_internal_gates() {
    assert(children_.empty());

    // Execute the initial gates.
    for (size_t gate_idx : gate_indices_) {
        [[maybe_unused]] const auto& avail_gates =
            scheduler().get_avail_gates();

        assert(std::find(avail_gates.begin(), avail_gates.end(), gate_idx) !=
               avail_gates.end());

        max_cost_ = max(max_cost_,
                        scheduler_->route_one_gate(*router_, gate_idx, true));

        assert(std::find(avail_gates.begin(), avail_gates.end(), gate_idx) ==
               avail_gates.end());
    }

    // Execute additional gates if exec_single.
    if (gate_indices_.empty() || !conf_.exec_single) {
        return;
    }

    size_t gate_idx;
    while ((gate_idx = immediate_next()) != ERROR_CODE) {
        max_cost_ = max(max_cost_,
                        scheduler_->route_one_gate(*router_, gate_idx, true));
        gate_indices_.push_back(gate_idx);
    }

    unordered_set<size_t> executed{gate_indices_.begin(), gate_indices_.end()};
    assert(executed.size() == gate_indices_.size());
}

template <>
inline void std::swap<TreeNode>(TreeNode& a, TreeNode& b) {
    TreeNode c{std::move(a)};
    a = std::move(b);
    b = std::move(c);
}

// Cost recursively calls children's cost, and selects the best one.
size_t TreeNode::best_cost(int depth) {
    // Grow if remaining depth >= 2.
    // Terminates on leaf nodes.
    if (is_leaf()) {
        if (depth > 1) {
            grow();
        }

        if (depth == 0) {
            return max_cost_;
        }
    }

    // Calls the more efficient best_cost() when depth is only 1.
    if (depth == 1) {
        return best_cost();
    }

    assert(children_.size() != 0);

    size_t best = (size_t)-1;

    auto end = children_.end();
    if (conf_.candidates < children_.size()) {
        end = children_.begin() + conf_.candidates;
        nth_element(children_.begin(), end, children_.end(),
                    [](const TreeNode& a, const TreeNode& b) {
                        return a.max_cost_ < b.max_cost_;
                    });
    }

    // Calcualtes the best cost for each children.
    for (auto child = children_.begin(); child < end; ++child) {
        size_t cost = child->best_cost(depth - 1);

        if (cost < best) {
            best = cost;
        }
    }

    // Clear the cache if specified.
    if (conf_.never_cache) {
        children_.clear();
    }

    return best;
}

size_t TreeNode::best_cost() const {
    size_t best = (size_t)-1;

    const auto& avail_gates = scheduler().get_avail_gates();

#pragma omp parallel for
    for (size_t idx = 0; idx < avail_gates.size(); ++idx) {
        TreeNode child_node{conf_, avail_gates[idx], router().clone(),
                            scheduler().clone(), max_cost_};
        size_t cost = child_node.max_cost_;

#pragma omp critical
        if (cost < best) {
            best = cost;
        }
    }

    return best;
}

// Grow by adding availalble gates to children.
void TreeNode::grow() {
    const auto& avail_gates = scheduler().get_avail_gates();

    assert(children_.empty());
    children_.reserve(avail_gates.size());

    for (size_t gate_idx : avail_gates) {
        children_.emplace_back(conf_, gate_idx, router().clone(),
                               scheduler().clone(), max_cost_);
    }
}

inline void TreeNode::grow_if_needed() {
    if (is_leaf()) {
        grow();
    }
}

TreeNode TreeNode::best_child(int depth) {
    auto next_nodes = children();
    size_t best_idx = 0, best = (size_t)-1;

    for (size_t idx = 0; idx < next_nodes.size(); ++idx) {
        auto& node = next_nodes[idx];

        assert(depth >= 1);
        size_t cost = node.best_cost(depth);

        if (cost < best) {
            best_idx = idx;
            best = cost;
        }
    }

    return next_nodes[best_idx];
}

// FIXME - settings depth never cache exec single
SearchScheduler::SearchScheduler(unique_ptr<CircuitTopo> topo)
    : GreedyScheduler(move(topo)),
      look_ahead(5),
      never_cache_(true),
      exec_single_(true) {
    cache_only_when_necessary();
}

SearchScheduler::SearchScheduler(const SearchScheduler& other)
    : GreedyScheduler(other),
      look_ahead(other.look_ahead),
      never_cache_(other.never_cache_),
      exec_single_(other.exec_single_) {}

SearchScheduler::SearchScheduler(SearchScheduler&& other)
    : GreedyScheduler(other),
      look_ahead(other.look_ahead),
      never_cache_(other.never_cache_),
      exec_single_(other.exec_single_) {}

unique_ptr<BaseScheduler> SearchScheduler::clone() const {
    return make_unique<SearchScheduler>(*this);
}

void SearchScheduler::cache_only_when_necessary() {
    if (!never_cache_ && look_ahead == 1) {
        cerr << "When look_ahead = 1, 'never_cache' is used by default.\n";
        never_cache_ = true;
    }
}

void SearchScheduler::assign_gates(unique_ptr<Router> router) {
    auto total_gates = topo_->get_num_gates();

    auto root = make_unique<TreeNode>(
        TreeNodeConf{never_cache_, exec_single_, conf_.candidates},
        vector<size_t>{}, router->clone(), clone(), 0);

    // For each step. (all nodes + 1 dummy)
    TqdmWrapper bar{total_gates + 1};
    do {
        // Update the candidates.
        auto selected_node =
            make_unique<TreeNode>(root->best_child(look_ahead));

        root = move(selected_node);

        for (size_t gate_idx : root->executed_gates()) {
            route_one_gate(*router, gate_idx);
            ++bar;
        }
    } while (!root->done());
}
