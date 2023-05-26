/****************************************************************************
  FileName     [ scheduler.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "scheduler.h"

#include <algorithm>
#include <cassert>

using namespace std;

/**
 * @brief Get the Scheduler object
 *
 * @param typ
 * @param topo
 * @param tqdm
 * @return unique_ptr<BaseScheduler>
 */
unique_ptr<BaseScheduler> getScheduler(unique_ptr<CircuitTopo> topo, bool tqdm) {
    // 0:base 1:static 2:random 3:greedy 4:search
    if (DUOSTRA_SCHEDULER == 2) {
        return make_unique<RandomScheduler>(move(topo), tqdm);
    } else if (DUOSTRA_SCHEDULER == 1) {
        return make_unique<StaticScheduler>(move(topo), tqdm);
    } else if (DUOSTRA_SCHEDULER == 3) {
        return make_unique<GreedyScheduler>(move(topo), tqdm);
    } else if (DUOSTRA_SCHEDULER == 4) {
        return make_unique<SearchScheduler>(move(topo), tqdm);
    } else if (DUOSTRA_SCHEDULER == 0) {
        return make_unique<BaseScheduler>(move(topo), tqdm);
    } else {
        cerr << "Error: scheduler type not found" << endl;
        abort();
    }
}

// SECTION - Class BaseScheduler Member Functions

/**
 * @brief Construct a new Base Scheduler:: Base Scheduler object
 *
 * @param topo
 * @param tqdm
 */
BaseScheduler::BaseScheduler(unique_ptr<CircuitTopo> topo, bool tqdm) : _circuitTopology(move(topo)), _operations({}), _assignOrder({}), _tqdm(tqdm) {}

/**
 * @brief Construct a new Base Scheduler:: Base Scheduler object
 *
 * @param other
 */
BaseScheduler::BaseScheduler(const BaseScheduler& other) : _circuitTopology(other._circuitTopology->clone()), _operations(other._operations), _assignOrder(other._assignOrder), _tqdm(other._tqdm) {}

/**
 * @brief Construct a new Base Scheduler:: Base Scheduler object
 *
 * @param other
 */
BaseScheduler::BaseScheduler(BaseScheduler&& other) : _circuitTopology(move(other._circuitTopology)), _operations(move(other._operations)), _assignOrder(move(other._assignOrder)), _tqdm(other._tqdm) {}

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
unique_ptr<BaseScheduler> BaseScheduler::clone() const {
    return make_unique<BaseScheduler>(*this);
}

/**
 * @brief Sort by operation time
 *
 */
void BaseScheduler::sort() {
    std::sort(_operations.begin(), _operations.end(), [](const Operation& a, const Operation& b) -> bool {
        return a.getOperationTime() < b.getOperationTime();
    });
    _sorted = true;
}

/**
 * @brief Get final cost (depth)
 *
 * @return size_t
 */
size_t BaseScheduler::getFinalCost() const {
    assert(_sorted);
    return _operations.at(_operations.size() - 1).getCost();
}

/**
 * @brief Get total time
 *
 * @return size_t
 */
size_t BaseScheduler::getTotalTime() const {
    assert(_sorted);
    size_t ret = 0;
    for (size_t i = 0; i < _operations.size(); ++i) {
        tuple<size_t, size_t> dur = _operations[i].getDuration();
        ret += std::get<1>(dur) - std::get<0>(dur);
    }
    return ret;
}

/**
 * @brief Get number of swaps
 *
 * @return size_t
 */
size_t BaseScheduler::getSwapNum() const {
    size_t ret = 0;
    for (size_t i = 0; i < _operations.size(); ++i) {
        if (_operations.at(i).getType() == GateType::SWAP)
            ++ret;
    }
    return ret;
}

/**
 * @brief Get executable gate
 *
 * @param router
 * @return size_t
 */
size_t BaseScheduler::getExecutable(Router& router) const {
    for (size_t gateIdx : _circuitTopology->getAvailableGates()) {
        if (router.isExecutable(_circuitTopology->getGate(gateIdx))) {
            return gateIdx;
        }
    }
    return ERROR_CODE;
}

/**
 * @brief Get cost of operations
 *
 * @return size_t
 */
size_t BaseScheduler::operationsCost() const {
    return max_element(_operations.begin(), _operations.end(),
                       [](const Operation& a, const Operation& b) {
                           return a.getCost() < b.getCost();
                       })
        ->getCost();
}

/**
 * @brief Assign gates and sort
 *
 * @param router
 * @return Device
 */
Device BaseScheduler::assignGatesAndSort(unique_ptr<Router> router) {
    Device d = assignGates(move(router));
    sort();
    return d;
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
Device BaseScheduler::assignGates(unique_ptr<Router> router) {
    for (TqdmWrapper bar{_circuitTopology->getNumGates()}; !bar.done(); ++bar)
        routeOneGate(*router, bar.idx());
    return router->getDevice();
}

/**
 * @brief Route one gate
 *
 * @param router
 * @param gateIdx
 * @param forget
 * @return size_t
 */
size_t BaseScheduler::routeOneGate(Router& router, size_t gateIdx, bool forget) {
    const auto& gate = _circuitTopology->getGate(gateIdx);
    auto ops{router.assignGate(gate)};
    size_t maxCost = 0;
    for (const auto& op : ops) {
        if (op.getCost() > maxCost)
            maxCost = op.getCost();
    }
    if (!forget)
        _operations.insert(_operations.end(), ops.begin(), ops.end());
    _assignOrder.emplace_back(gateIdx);
    _circuitTopology->updateAvailableGates(gateIdx);
    return maxCost;
}

// SECTION - Class RandomScheduler Member Functions

/**
 * @brief Construct a new Random Scheduler:: Random Scheduler object
 *
 * @param topo
 * @param tqdm
 */
RandomScheduler::RandomScheduler(unique_ptr<CircuitTopo> topo, bool tqdm) : BaseScheduler(move(topo), tqdm) {}

/**
 * @brief Construct a new Random Scheduler:: Random Scheduler object
 *
 * @param other
 */
RandomScheduler::RandomScheduler(const RandomScheduler& other) : BaseScheduler(other) {}

/**
 * @brief Construct a new Random Scheduler:: Random Scheduler object
 *
 * @param other
 */
RandomScheduler::RandomScheduler(RandomScheduler&& other) : BaseScheduler(move(other)) {}

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
unique_ptr<BaseScheduler> RandomScheduler::clone() const {
    return make_unique<RandomScheduler>(*this);
}

/**
 * @brief Assign gate
 *
 * @param router
 * @return Device
 */
Device RandomScheduler::assignGates(unique_ptr<Router> router) {
    [[maybe_unused]] size_t count = 0;

    for (TqdmWrapper bar{_circuitTopology->getNumGates()}; !bar.done(); ++bar) {
        auto& waitlist = _circuitTopology->getAvailableGates();
        assert(waitlist.size() > 0);
        srand(chrono::system_clock::now().time_since_epoch().count());

        size_t choose = rand() % waitlist.size();

        routeOneGate(*router, waitlist[choose]);
#ifdef DEBUG
        cout << waitlist << " " << waitlist[choose] << "\n\n";
#endif
        ++count;
    }
    assert(count == _circuitTopology->getNumGates());
    return router->getDevice();
}

// SECTION - Class StaticScheduler Member Functions

/**
 * @brief Construct a new Static Scheduler:: Static Scheduler object
 *
 * @param topo
 * @param tqdm
 */
StaticScheduler::StaticScheduler(unique_ptr<CircuitTopo> topo, bool tqdm) : BaseScheduler(move(topo), tqdm) {}

/**
 * @brief Construct a new Static Scheduler:: Static Scheduler object
 *
 * @param other
 */
StaticScheduler::StaticScheduler(const StaticScheduler& other) : BaseScheduler(other) {}

/**
 * @brief Construct a new Static Scheduler:: Static Scheduler object
 *
 * @param other
 */
StaticScheduler::StaticScheduler(StaticScheduler&& other) : BaseScheduler(move(other)) {}

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
unique_ptr<BaseScheduler> StaticScheduler::clone() const {
    return make_unique<StaticScheduler>(*this);
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
Device StaticScheduler::assignGates(unique_ptr<Router> router) {
    [[maybe_unused]] size_t count = 0;
    for (TqdmWrapper bar{_circuitTopology->getNumGates()}; !bar.done(); ++bar) {
        auto& waitlist = _circuitTopology->getAvailableGates();
        assert(waitlist.size() > 0);

        size_t gate_idx = getExecutable(*router);
        if (gate_idx == ERROR_CODE)
            gate_idx = waitlist[0];

        routeOneGate(*router, gate_idx);
        ++count;
    }
    assert(count == _circuitTopology->getNumGates());
    return router->getDevice();
}
