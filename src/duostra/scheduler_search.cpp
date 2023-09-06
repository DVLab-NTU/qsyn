/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Searcg Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <omp.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <unordered_set>
#include <vector>

#include "./scheduler.hpp"
#include "./variables.hpp"

extern bool stop_requested();

using namespace std;

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
                   unique_ptr<Router> router,
                   unique_ptr<BaseScheduler> scheduler,
                   size_t max_cost)
    : TreeNode(conf,
               vector<size_t>{gate_id},
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
                   vector<size_t>&& gate_ids,
                   unique_ptr<Router> router,
                   unique_ptr<BaseScheduler> scheduler,
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
    : _conf(other._conf),
      _gate_ids(other._gate_ids),
      _children(other._children),
      _max_cost(other._max_cost),
      _router(other._router->clone()),
      _scheduler(other._scheduler->clone()) {}

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param other
 */
TreeNode::TreeNode(TreeNode&& other)
    : _conf(other._conf),
      _gate_ids(std::move(other._gate_ids)),
      _children(std::move(other._children)),
      _max_cost(other._max_cost),
      _router(std::move(other._router)),
      _scheduler(std::move(other._scheduler)) {}

/**
 * @brief Assignment operator overloading for TreeNode
 *
 * @param other
 * @return TreeNode&
 */
TreeNode& TreeNode::operator=(TreeNode const& other) {
    _conf = other._conf;
    _gate_ids = other._gate_ids;
    _children = other._children;
    _max_cost = other._max_cost;
    _router = other._router->clone();
    _scheduler = other._scheduler->clone();
    return *this;
}

/**
 * @brief Assignment operator overloading for TreeNode
 *
 * @param other
 * @return TreeNode&
 */
TreeNode& TreeNode::operator=(TreeNode&& other) {
    _conf = other._conf;
    _gate_ids = std::move(other._gate_ids);
    _children = std::move(other._children);
    _max_cost = other._max_cost;
    _router = std::move(other._router);
    _scheduler = std::move(other._scheduler);
    return *this;
}

/**
 * @brief Get children
 *
 * @return vector<TreeNode>&&
 */
vector<TreeNode>&& TreeNode::_take_children() {
    grow_if_needed();
    return std::move(_children);
}

/**
 * @brief Grow by adding availalble gates to children.
 *
 */
void TreeNode::_grow() {
    auto const& avail_gates = scheduler().get_available_gates();
    assert(_children.empty());
    _children.reserve(avail_gates.size());
    for (size_t gate_id : avail_gates)
        _children.emplace_back(_conf, gate_id, router().clone(), scheduler().clone(), _max_cost);
}

/**
 * @brief Grow if needed
 *
 */
inline void TreeNode::grow_if_needed() {
    if (is_leaf()) _grow();
}

/**
 * @brief Can grow or not
 *
 * @return true
 * @return false
 */
inline bool TreeNode::can_grow() const {
    return !scheduler().get_available_gates().empty();
}

/**
 * @brief Get immediate next
 *
 * @return size_t
 */
size_t TreeNode::_immediate_next() const {
    size_t gate_id = scheduler().get_executable(*_router);
    auto const& avail_gates = scheduler().get_available_gates();
    if (gate_id != SIZE_MAX)
        return gate_id;
    if (avail_gates.size() == 1)
        return avail_gates[0];
    return SIZE_MAX;
}

/**
 * @brief Route internal gates
 *
 */
void TreeNode::_route_internal_gates() {
    assert(_children.empty());

    // Execute the initial gates.
    for (size_t gate_id : _gate_ids) {
        [[maybe_unused]] auto const& avail_gates = scheduler().get_available_gates();
        assert(find(avail_gates.begin(), avail_gates.end(), gate_id) != avail_gates.end());
        _max_cost = max(_max_cost, _scheduler->route_one_gate(*_router, gate_id, true));
        assert(find(avail_gates.begin(), avail_gates.end(), gate_id) == avail_gates.end());
    }

    // Execute additional gates if _executeSingle.
    if (_gate_ids.empty() || !_conf._executeSingle)
        return;

    size_t gate_id;
    while ((gate_id = _immediate_next()) != SIZE_MAX) {
        _max_cost = max(_max_cost, _scheduler->route_one_gate(*_router, gate_id, true));
        _gate_ids.emplace_back(gate_id);
    }

    unordered_set<size_t> executed{_gate_ids.begin(), _gate_ids.end()};
    assert(executed.size() == _gate_ids.size());
}

template <>
inline void std::swap<TreeNode>(TreeNode& a, TreeNode& b) {
    TreeNode c{std::move(a)};
    a = std::move(b);
    b = std::move(c);
}

/**
 * @brief Get best child
 *
 * @param depth
 * @return TreeNode
 */
TreeNode TreeNode::best_child(int depth) {
    auto next_nodes = _take_children();
    size_t best_id = 0, best = (size_t)-1;
    for (size_t idx = 0; idx < next_nodes.size(); ++idx) {
        auto& node = next_nodes[idx];
        assert(depth >= 1);
        size_t cost = node.best_cost(depth);
        if (cost < best) {
            best_id = idx;
            best = cost;
        }
    }
    return next_nodes[best_id];
}

/**
 * @brief Cost recursively calls children's cost, and selects the best one.
 *
 * @param depth
 * @return size_t
 */
size_t TreeNode::best_cost(int depth) {
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
        nth_element(_children.begin(), end, _children.end(),
                    [](TreeNode const& a, TreeNode const& b) {
                        return a._max_cost < b._max_cost;
                    });
    }

    // Calcualtes the best cost for each children.
    size_t best = (size_t)-1;
    for (auto child = _children.begin(); child < end; ++child) {
        size_t cost = child->best_cost(depth - 1);

        if (cost < best)
            best = cost;
    }

    // Clear the cache if specified.
    if (_conf._neverCache)
        _children.clear();

    return best;
}

/**
 * @brief Select the best cost.
 *
 * @return size_t
 */
size_t TreeNode::best_cost() const {
    size_t best = (size_t)-1;

    auto const& avail_gates = scheduler().get_available_gates();

#pragma omp parallel for
    for (size_t idx = 0; idx < avail_gates.size(); ++idx) {
        TreeNode child_node{_conf, avail_gates[idx], router().clone(),
                            scheduler().clone(), _max_cost};
        size_t cost = child_node._max_cost;

#pragma omp critical
        if (cost < best)
            best = cost;
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
SearchScheduler::SearchScheduler(unique_ptr<CircuitTopology> topo, bool tqdm)
    : GreedyScheduler(std::move(topo), tqdm),
      _lookAhead(DUOSTRA_DEPTH),
      _never_cache(DUOSTRA_NEVER_CACHE),
      _execute_single(DUOSTRA_EXECUTE_SINGLE) {
    _cache_when_necessary();
}

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param other
 */
SearchScheduler::SearchScheduler(SearchScheduler const& other)
    : GreedyScheduler(other),
      _lookAhead(other._lookAhead),
      _never_cache(other._never_cache),
      _execute_single(other._execute_single) {}

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param other
 */
SearchScheduler::SearchScheduler(SearchScheduler&& other)
    : GreedyScheduler(other),
      _lookAhead(other._lookAhead),
      _never_cache(other._never_cache),
      _execute_single(other._execute_single) {}

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
unique_ptr<BaseScheduler> SearchScheduler::clone() const {
    return make_unique<SearchScheduler>(*this);
}

/**
 * @brief Cache only when necessary
 *
 */
void SearchScheduler::_cache_when_necessary() {
    if (!_never_cache && _lookAhead == 1) {
        cerr << "When _lookAhead = 1, '_neverCache' is used by default.\n";
        _never_cache = true;
    }
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
Device SearchScheduler::_assign_gates(unique_ptr<Router> router) {
    auto total_gates = _circuit_topology->get_num_gates();

    auto root = make_unique<TreeNode>(
        TreeNodeConf{_never_cache, _execute_single, _conf._candidates},
        vector<size_t>{}, router->clone(), clone(), 0);

    // For each step. (all nodes + 1 dummy)
    dvlab::TqdmWrapper bar{total_gates + 1, _tqdm};
    do {
        // Update the _candidates.
        if (stop_requested()) {
            return router->get_device();
        }
        auto selected_node = make_unique<TreeNode>(root->best_child(static_cast<int>(_lookAhead)));
        root = std::move(selected_node);

        for (size_t gate_id : root->get_executed_gates()) {
            route_one_gate(*router, gate_id);
            ++bar;
        }
    } while (!root->done());
    return router->get_device();
}
