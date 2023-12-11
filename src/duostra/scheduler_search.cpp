/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Search Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <omp.h>

#include <algorithm>
#include <functional>
#include <tl/enumerate.hpp>
#include <unordered_set>
#include <vector>

#include "./duostra.hpp"
#include "./scheduler.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::duostra {

// SECTION - Class TreeNode Member Functions

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param conf
 * @param gateId
 * @param router
 * @param scheduler
 * @param maxCost
 */
TreeNode::TreeNode(TreeNodeConf conf,
                   size_t gate_id,
                   std::unique_ptr<Router> router,
                   std::unique_ptr<BaseScheduler> scheduler,
                   size_t max_cost)
    : TreeNode(conf,
               std::vector<size_t>{gate_id},
               std::move(router),
               std::move(scheduler),
               max_cost) {}

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param conf
 * @param gateIds
 * @param router
 * @param scheduler
 * @param maxCost
 */
TreeNode::TreeNode(TreeNodeConf conf,
                   std::vector<size_t> gate_ids,
                   std::unique_ptr<Router> router,
                   std::unique_ptr<BaseScheduler> scheduler,
                   size_t max_cost)
    : _conf(conf),
      _gate_ids(std::move(gate_ids)),
      _children({}),
      _max_cost(max_cost),
      _router(std::move(router)),
      _scheduler(std::move(scheduler)) {
    _route_internal_gates();
}

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param other
 */
TreeNode::TreeNode(TreeNode const& other)
    : _conf{other._conf},
      _gate_ids{other._gate_ids},
      _children{other._children},
      _max_cost{other._max_cost},
      _router{other._router->clone()},
      _scheduler{other._scheduler->clone()} {}

/**
 * @brief Grow by adding availalble gates to children.
 *
 */
void TreeNode::_grow() {
    auto const& avail_gates = scheduler().get_available_gates();
    assert(_children.empty());
    _children.reserve(avail_gates.size());
    for (auto gate_id : avail_gates)
        _children.emplace_back(_conf, gate_id, router().clone(), scheduler().clone(), _max_cost);
}

/**
 * @brief Get immediate next
 *
 * @return size_t
 */
std::optional<size_t> TreeNode::_immediate_next() const {
    auto gate_id            = scheduler().get_executable_gate(*_router);
    auto const& avail_gates = scheduler().get_available_gates();
    if (gate_id.has_value())
        return gate_id;
    if (avail_gates.size() == 1)
        return avail_gates[0];
    return std::nullopt;
}

/**
 * @brief Route internal gates
 *
 */
void TreeNode::_route_internal_gates() {
    assert(_children.empty());

    // Execute the initial gates.
    for (auto const& gate_id : _gate_ids) {
        [[maybe_unused]] auto const& avail_gates = scheduler().get_available_gates();
        assert(find(avail_gates.begin(), avail_gates.end(), gate_id) != avail_gates.end());
        _max_cost = std::max(_max_cost, _scheduler->route_one_gate(*_router, gate_id, true));
        assert(find(avail_gates.begin(), avail_gates.end(), gate_id) == avail_gates.end());
    }

    // Execute additional gates if _executeSingle.
    if (_gate_ids.empty() || !_conf._executeSingle)
        return;

    std::optional<size_t> gate_id;
    while ((gate_id = _immediate_next()) != std::nullopt) {
        _max_cost = std::max(_max_cost, _scheduler->route_one_gate(*_router, gate_id.value(), true));
        _gate_ids.emplace_back(gate_id.value());
    }
}

/**
 * @brief Get best child
 *
 * @param depth
 * @return TreeNode
 */
TreeNode TreeNode::best_child(size_t depth) {
    if (is_leaf()) _grow();
    // NOTE - best_cost(depth) is computationally expensive, so we don't use std::min_element here to avoid calling it twice.
    size_t best_idx  = 0;
    size_t best_cost = SIZE_MAX;
    for (auto&& [idx, node] : tl::views::enumerate(_children)) {
        assert(depth >= 1);
        auto cost = node.best_cost(depth);
        if (cost < best_cost) {
            best_idx  = idx;
            best_cost = cost;
        }
    }
    return std::move(_children[best_idx]);
}

/**
 * @brief Cost recursively calls children's cost, and selects the best one.
 *
 * @param depth
 * @return size_t
 */
size_t TreeNode::best_cost(size_t depth) {
    // Grow if remaining depth >= 2.
    // Terminates on leaf nodes.
    if (is_leaf()) {
        if (depth <= 0 || !can_grow())
            return _max_cost;

        if (depth > 1)
            _grow();
    }

    // Calls the more efficient best_cost() when depth is only 1.
    if (depth == 1)
        return best_cost();

    assert(depth > 1);
    assert(_children.size() != 0);

    auto end = _children.end();
    if (_conf._candidates < _children.size()) {
        end = _children.begin() + static_cast<std::ptrdiff_t>(_conf._candidates);
        std::nth_element(_children.begin(), end, _children.end(),
                         [](TreeNode const& a, TreeNode const& b) {
                             return a._max_cost < b._max_cost;
                         });
    }

    // Calculates the best cost for each children.
    size_t best_cost = SIZE_MAX;
    for (auto& child : _children) {
        best_cost = std::min(best_cost, child.best_cost(depth - 1));
    }

    // Clear the cache if specified.
    if (_conf._neverCache)
        _children.clear();

    return best_cost;
}

/**
 * @brief Select the best cost.
 *
 * @return size_t
 */
size_t TreeNode::best_cost() const {
    size_t best = SIZE_MAX;

#pragma omp parallel for reduction(min : best)
    for (auto& gate : scheduler().get_available_gates()) {
        TreeNode const child_node{_conf, gate, router().clone(),
                                  scheduler().clone(), _max_cost};
        best = std::min(best, child_node._max_cost);
    }

    return best;
}

// SECTION - Class Search Scheduler Member Functions

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param topo
 * @param tqdm
 */
SearchScheduler::SearchScheduler(CircuitTopology const& topo, bool tqdm)
    : GreedyScheduler(topo, tqdm),
      _never_cache(DuostraConfig::NEVER_CACHE),
      _execute_single(DuostraConfig::EXECUTE_SINGLE_QUBIT_GATES_ASAP),
      _lookahead(DuostraConfig::SEARCH_DEPTH) {
    _cache_when_necessary();
}

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param other
 */
SearchScheduler::SearchScheduler(SearchScheduler const& other)
    : GreedyScheduler(other),
      _never_cache(other._never_cache),
      _execute_single(other._execute_single),
      _lookahead(other._lookahead) {}

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param other
 */
SearchScheduler::SearchScheduler(SearchScheduler&& other) noexcept
    : GreedyScheduler(std::move(other)),
      _never_cache(other._never_cache),
      _execute_single(other._execute_single),
      _lookahead(other._lookahead) {}

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> SearchScheduler::clone() const {
    return std::make_unique<SearchScheduler>(*this);
}

/**
 * @brief Cache only when necessary
 *
 */
void SearchScheduler::_cache_when_necessary() {
    if (!_never_cache && _lookahead == 1) {
        spdlog::error("When _lookAhead = 1, '_neverCache' is used by default.");
        _never_cache = true;
    }
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
SearchScheduler::Device SearchScheduler::_assign_gates(std::unique_ptr<Router> router) {
    auto total_gates = _circuit_topology.get_num_gates();

    auto root = make_unique<TreeNode>(
        TreeNodeConf{_never_cache, _execute_single, _conf.num_candidates},
        std::vector<size_t>{}, router->clone(), clone(), 0);

    // For each step. (all nodes + 1 dummy)
    dvlab::TqdmWrapper bar{total_gates + 1, _tqdm};
    assert(!root->done());
    while (!root->done()) {
        // Update the _candidates.
        if (stop_requested()) {
            return router->get_device();
        }
        auto selected_node = std::make_unique<TreeNode>(root->best_child(static_cast<int>(_lookahead)));
        root               = std::move(selected_node);

        for (auto const& gate_id : root->get_executed_gates()) {
            route_one_gate(*router, gate_id);
            ++bar;
        }
    }
    return router->get_device();
}

}  // namespace qsyn::duostra
