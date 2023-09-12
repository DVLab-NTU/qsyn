/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./scheduler.hpp"

#include <algorithm>
#include <cassert>

#include "./variables.hpp"

extern bool stop_requested();

namespace qsyn::duostra {

/**
 * @brief Get the Scheduler object
 *
 * @param typ
 * @param topo
 * @param tqdm
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> get_scheduler(std::unique_ptr<CircuitTopology> topo, bool tqdm) {
    // 0:base 1:static 2:random 3:greedy 4:search
    if (DUOSTRA_SCHEDULER == 2) {
        return std::make_unique<RandomScheduler>(*topo.get(), tqdm);
    } else if (DUOSTRA_SCHEDULER == 1) {
        return std::make_unique<StaticScheduler>(*topo.get(), tqdm);
    } else if (DUOSTRA_SCHEDULER == 3) {
        return std::make_unique<GreedyScheduler>(*topo.get(), tqdm);
    } else if (DUOSTRA_SCHEDULER == 4) {
        return std::make_unique<SearchScheduler>(*topo.get(), tqdm);
    } else if (DUOSTRA_SCHEDULER == 0) {
        return std::make_unique<BaseScheduler>(*topo.get(), tqdm);
    } else {
        std::cerr << "Error: scheduler type not found" << std::endl;
        abort();
    }
}

// SECTION - Class BaseScheduler Member Functions

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> BaseScheduler::clone() const {
    return std::make_unique<BaseScheduler>(*this);
}

/**
 * @brief Sort by operation time
 *
 */
void BaseScheduler::_sort() {
    std::sort(_operations.begin(), _operations.end(), [](Operation const& a, Operation const& b) -> bool {
        return a.get_operation_time() < b.get_operation_time();
    });
    _sorted = true;
}

/**
 * @brief Get final cost (depth)
 *
 * @return size_t
 */
size_t BaseScheduler::get_final_cost() const {
    assert(_sorted);
    return _operations.at(_operations.size() - 1).get_cost();
}

/**
 * @brief Get total time
 *
 * @return size_t
 */
size_t BaseScheduler::get_total_time() const {
    assert(_sorted);
    size_t ret = 0;
    for (size_t i = 0; i < _operations.size(); ++i) {
        std::tuple<size_t, size_t> dur = _operations[i].get_duration();
        ret += std::get<1>(dur) - std::get<0>(dur);
    }
    return ret;
}

/**
 * @brief Get number of swaps
 *
 * @return size_t
 */
size_t BaseScheduler::get_num_swaps() const {
    size_t ret = 0;
    for (size_t i = 0; i < _operations.size(); ++i) {
        if (_operations.at(i).get_type() == qsyn::qcir::GateType::swap)
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
size_t BaseScheduler::get_executable(Router& router) const {
    for (size_t gate_idx : _circuit_topology.get_available_gates()) {
        if (router.is_executable(_circuit_topology.get_gate(gate_idx))) {
            return gate_idx;
        }
    }
    return SIZE_MAX;
}

/**
 * @brief Get cost of operations
 *
 * @return size_t
 */
size_t BaseScheduler::get_operations_cost() const {
    return max_element(_operations.begin(), _operations.end(),
                       [](Operation const& a, Operation const& b) {
                           return a.get_cost() < b.get_cost();
                       })
        ->get_cost();
}

/**
 * @brief Assign gates and sort
 *
 * @param router
 * @return Device
 */
BaseScheduler::Device BaseScheduler::assign_gates_and_sort(std::unique_ptr<Router> router) {
    Device d = _assign_gates(std::move(router));
    _sort();
    return d;
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
BaseScheduler::Device BaseScheduler::_assign_gates(std::unique_ptr<Router> router) {
    for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates()}; !bar.done(); ++bar) {
        if (stop_requested()) {
            return router->get_device();
        }
        route_one_gate(*router, bar.idx());
    }
    return router->get_device();
}

/**
 * @brief Route one gate
 *
 * @param router
 * @param gateIdx
 * @param forget
 * @return size_t
 */
size_t BaseScheduler::route_one_gate(Router& router, size_t gate_id, bool forget) {
    auto const& gate = _circuit_topology.get_gate(gate_id);
    auto ops{router.assign_gate(gate)};
    size_t max_cost = 0;
    for (auto const& op : ops) {
        if (op.get_cost() > max_cost)
            max_cost = op.get_cost();
    }
    if (!forget)
        _operations.insert(_operations.end(), ops.begin(), ops.end());
    _assign_order.emplace_back(gate_id);
    _circuit_topology.update_available_gates(gate_id);
    return max_cost;
}

// SECTION - Class RandomScheduler Member Functions

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> RandomScheduler::clone() const {
    return std::make_unique<RandomScheduler>(*this);
}

/**
 * @brief Assign gate
 *
 * @param router
 * @return Device
 */
RandomScheduler::Device RandomScheduler::_assign_gates(std::unique_ptr<Router> router) {
    [[maybe_unused]] size_t count = 0;

    for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates()}; !bar.done(); ++bar) {
        if (stop_requested()) {
            return router->get_device();
        }
        auto& waitlist = _circuit_topology.get_available_gates();
        assert(waitlist.size() > 0);
        srand(std::chrono::system_clock::now().time_since_epoch().count());

        size_t choose = rand() % waitlist.size();

        route_one_gate(*router, waitlist[choose]);
#ifdef DEBUG
        cout << waitlist << " " << waitlist[choose] << "\n\n";
#endif
        ++count;
    }
    assert(count == _circuit_topology.get_num_gates());
    return router->get_device();
}

// SECTION - Class StaticScheduler Member Functions

/**
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> StaticScheduler::clone() const {
    return std::make_unique<StaticScheduler>(*this);
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
StaticScheduler::Device StaticScheduler::_assign_gates(std::unique_ptr<Router> router) {
    [[maybe_unused]] size_t count = 0;
    for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates()}; !bar.done(); ++bar) {
        if (stop_requested()) {
            return router->get_device();
        }
        auto& waitlist = _circuit_topology.get_available_gates();
        assert(waitlist.size() > 0);

        size_t gate_idx = get_executable(*router);
        if (gate_idx == SIZE_MAX)
            gate_idx = waitlist[0];

        route_one_gate(*router, gate_idx);
        ++count;
    }
    assert(count == _circuit_topology.get_num_gates());
    return router->get_device();
}

}  // namespace qsyn::duostra
