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
                   size_t gateId,
                   unique_ptr<Router> router,
                   unique_ptr<BaseScheduler> scheduler,
                   size_t maxCost)
    : TreeNode(conf,
               vector<size_t>{gateId},
               move(router),
               move(scheduler),
               maxCost) {}

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
                   vector<size_t>&& gateIds,
                   unique_ptr<Router> router,
                   unique_ptr<BaseScheduler> scheduler,
                   size_t maxCost)
    : _conf(conf),
      _gateIds(move(gateIds)),
      _children({}),
      _maxCost(maxCost),
      _router(move(router)),
      _scheduler(move(scheduler)) {
    routeInternalGates();
}

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param other
 */
TreeNode::TreeNode(const TreeNode& other)
    : _conf(other._conf),
      _gateIds(other._gateIds),
      _children(other._children),
      _maxCost(other._maxCost),
      _router(other._router->clone()),
      _scheduler(other._scheduler->clone()) {}

/**
 * @brief Construct a new Tree Node:: Tree Node object
 *
 * @param other
 */
TreeNode::TreeNode(TreeNode&& other)
    : _conf(other._conf),
      _gateIds(move(other._gateIds)),
      _children(move(other._children)),
      _maxCost(other._maxCost),
      _router(move(other._router)),
      _scheduler(move(other._scheduler)) {}

/**
 * @brief Assignment operator overloading for TreeNode
 *
 * @param other
 * @return TreeNode&
 */
TreeNode& TreeNode::operator=(const TreeNode& other) {
    _conf = other._conf;
    _gateIds = other._gateIds;
    _children = other._children;
    _maxCost = other._maxCost;
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
    _gateIds = move(other._gateIds);
    _children = move(other._children);
    _maxCost = other._maxCost;
    _router = move(other._router);
    _scheduler = move(other._scheduler);
    return *this;
}

/**
 * @brief Get children
 *
 * @return vector<TreeNode>&&
 */
vector<TreeNode>&& TreeNode::children() {
    growIfNeeded();
    return move(_children);
}

/**
 * @brief Grow by adding availalble gates to children.
 *
 */
void TreeNode::grow() {
    const auto& availGates = scheduler().getAvailableGates();
    assert(_children.empty());
    _children.reserve(availGates.size());
    for (size_t gateId : availGates)
        _children.emplace_back(_conf, gateId, router().clone(), scheduler().clone(), _maxCost);
}

/**
 * @brief Grow if needed
 *
 */
inline void TreeNode::growIfNeeded() {
    if (isLeaf()) grow();
}

/**
 * @brief Can grow or not
 *
 * @return true
 * @return false
 */
inline bool TreeNode::canGrow() const {
    return !scheduler().getAvailableGates().empty();
}

/**
 * @brief Get immediate next
 *
 * @return size_t
 */
size_t TreeNode::immediateNext() const {
    size_t gateId = scheduler().getExecutable(*_router);
    const auto& avail_gates = scheduler().getAvailableGates();
    if (gateId != ERROR_CODE)
        return gateId;
    if (avail_gates.size() == 1)
        return avail_gates[0];
    return ERROR_CODE;
}

/**
 * @brief Route internal gates
 *
 */
void TreeNode::routeInternalGates() {
    assert(_children.empty());

    // Execute the initial gates.
    for (size_t gateId : _gateIds) {
        [[maybe_unused]] const auto& availGates = scheduler().getAvailableGates();
        assert(find(availGates.begin(), availGates.end(), gateId) != availGates.end());
        _maxCost = max(_maxCost, _scheduler->routeOneGate(*_router, gateId, true));
        assert(find(availGates.begin(), availGates.end(), gateId) == availGates.end());
    }

    // Execute additional gates if _executeSingle.
    if (_gateIds.empty() || !_conf._executeSingle)
        return;

    size_t gateId;
    while ((gateId = immediateNext()) != ERROR_CODE) {
        _maxCost = max(_maxCost, _scheduler->routeOneGate(*_router, gateId, true));
        _gateIds.push_back(gateId);
    }

    unordered_set<size_t> executed{_gateIds.begin(), _gateIds.end()};
    assert(executed.size() == _gateIds.size());
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
TreeNode TreeNode::bestChild(int depth) {
    auto nextNodes = children();
    size_t bestId = 0, best = (size_t)-1;
    for (size_t idx = 0; idx < nextNodes.size(); ++idx) {
        auto& node = nextNodes[idx];
        assert(depth >= 1);
        size_t cost = node.bestCost(depth);
        if (cost < best) {
            bestId = idx;
            best = cost;
        }
    }
    return nextNodes[bestId];
}

/**
 * @brief Cost recursively calls children's cost, and selects the best one.
 *
 * @param depth
 * @return size_t
 */
size_t TreeNode::bestCost(int depth) {
    // Grow if remaining depth >= 2.
    // Terminates on leaf nodes.
    if (isLeaf()) {
        if (depth <= 0 || !canGrow())
            return _maxCost;

        if (depth > 1)
            grow();
    }

    // Calls the more efficient bestCost() when depth is only 1.
    if (depth == 1)
        return bestCost();

    assert(depth > 1);
    assert(_children.size() != 0);

    auto end = _children.end();
    if (_conf._candidates < _children.size()) {
        end = _children.begin() + _conf._candidates;
        nth_element(_children.begin(), end, _children.end(),
                    [](const TreeNode& a, const TreeNode& b) {
                        return a._maxCost < b._maxCost;
                    });
    }

    // Calcualtes the best cost for each children.
    size_t best = (size_t)-1;
    for (auto child = _children.begin(); child < end; ++child) {
        size_t cost = child->bestCost(depth - 1);

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
size_t TreeNode::bestCost() const {
    size_t best = (size_t)-1;

    const auto& availGates = scheduler().getAvailableGates();

#pragma omp parallel for
    for (size_t idx = 0; idx < availGates.size(); ++idx) {
        TreeNode childNode{_conf, availGates[idx], router().clone(),
                           scheduler().clone(), _maxCost};
        size_t cost = childNode._maxCost;

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
SearchScheduler::SearchScheduler(unique_ptr<CircuitTopo> topo, bool tqdm)
    : GreedyScheduler(move(topo), tqdm),
      _lookAhead(DUOSTRA_DEPTH),
      _neverCache(DUOSTRA_NEVER_CACHE),
      _executeSingle(DUOSTRA_EXECUTE_SINGLE) {
    cacheOnlyWhenNecessary();
}

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param other
 */
SearchScheduler::SearchScheduler(const SearchScheduler& other)
    : GreedyScheduler(other),
      _lookAhead(other._lookAhead),
      _neverCache(other._neverCache),
      _executeSingle(other._executeSingle) {}

/**
 * @brief Construct a new Search Scheduler:: Search Scheduler object
 *
 * @param other
 */
SearchScheduler::SearchScheduler(SearchScheduler&& other)
    : GreedyScheduler(other),
      _lookAhead(other._lookAhead),
      _neverCache(other._neverCache),
      _executeSingle(other._executeSingle) {}

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
void SearchScheduler::cacheOnlyWhenNecessary() {
    if (!_neverCache && _lookAhead == 1) {
        cerr << "When _lookAhead = 1, '_neverCache' is used by default.\n";
        _neverCache = true;
    }
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
Device SearchScheduler::assignGates(unique_ptr<Router> router) {
    auto totalGates = _circuitTopology->getNumGates();

    auto root = make_unique<TreeNode>(
        TreeNodeConf{_neverCache, _executeSingle, _conf._candidates},
        vector<size_t>{}, router->clone(), clone(), 0);

    // For each step. (all nodes + 1 dummy)
    TqdmWrapper bar{totalGates + 1, _tqdm};
    do {
        // Update the _candidates.
        auto selectedNode = make_unique<TreeNode>(root->bestChild(_lookAhead));
        root = move(selectedNode);

        for (size_t gateId : root->executedGates()) {
            routeOneGate(*router, gateId);
            ++bar;
        }
    } while (!root->done());
    return router->getDevice();
}
