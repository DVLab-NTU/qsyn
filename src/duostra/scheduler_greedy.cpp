/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Greedy Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>

#include "./scheduler.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::duostra {

// SECTION - Class Topology Member Functions

class TopologyCandidate {
public:
    TopologyCandidate(CircuitTopology const& topo, size_t candidate)
        : _circuit_topology(&topo), _num_candidates(candidate) {}

    std::vector<size_t> get_available_gates() const {
        auto& gates = _circuit_topology->get_available_gates();
        if (gates.size() < _num_candidates)
            return gates;
        return std::vector<size_t>(gates.begin(), dvlab::iterator::next(gates.begin(), _num_candidates));
    }

private:
    CircuitTopology const* _circuit_topology;
    size_t _num_candidates;
};

// SECTION - Struct GreedyConf Member Functions

/**
 * @brief Construct a new Greedy Conf:: Greedy Conf object
 *
 */
GreedyConf::GreedyConf()
    : available_time_strategy(DuostraConfig::AVAILABLE_TIME_STRATEGY),
      cost_type(DuostraConfig::COST_SELECTION_STRATEGY),
      num_candidates(DuostraConfig::NUM_CANDIDATES),
      apsp_coeff(DuostraConfig::APSP_COEFF) {}

// SECTION - Class GreedyScheduler Member Functions

/**
 * @brief Get scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> GreedyScheduler::clone() const {
    return std::make_unique<GreedyScheduler>(*this);
}

/**
 * @brief Calculate total time to route all remaining gates
 * 
 * @param router Current router state
 * @param next_gate_id ID of the next gate to be routed
 * @return size_t Total time to route all remaining gates
 */
size_t GreedyScheduler::calculate_total_routing_time(Router& router, size_t next_gate_id) const {
    size_t total_time = 0;
    auto temp_router = router.clone();
    auto temp_topo = _circuit_topology.clone();
    fmt::println("Calculating total routing time for gate {}", next_gate_id);
    // Route the next gate
    auto const& next_gate = temp_topo->get_gate(next_gate_id);
    auto ops = temp_router->assign_gate(next_gate);
    size_t max_cost = 0;
    for (auto const& [op, time] : ops) {
        if (time.second > max_cost)
            max_cost = time.second;
    }
    total_time += max_cost;
    temp_topo->update_available_gates(next_gate_id);

    // Route remaining gates
    auto topo_wrap = TopologyCandidate(*temp_topo, _conf.num_candidates);
    while (!topo_wrap.get_available_gates().empty()) {
        auto waitlist = topo_wrap.get_available_gates();
        assert(!waitlist.empty());

        auto gate_idx = get_executable_gate(*temp_router);
        if (gate_idx == std::nullopt) {
            gate_idx = greedy_fallback(*temp_router, waitlist);
        }
        assert(gate_idx.has_value());
        
        auto const& gate = temp_topo->get_gate(gate_idx.value());
        ops = temp_router->assign_gate(gate);
        max_cost = 0;
        for (auto const& [op, time] : ops) {
            if (time.second > max_cost)
                max_cost = time.second;
        }
        total_time += max_cost;
        temp_topo->update_available_gates(gate_idx.value());
    }

    return total_time;
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
GreedyScheduler::Device GreedyScheduler::_assign_gates(std::unique_ptr<Router> router) {
    [[maybe_unused]] size_t count = 0;
    auto topo_wrap                = TopologyCandidate(_circuit_topology, _conf.num_candidates);
    auto done = false;
    for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates(), _tqdm};
         !topo_wrap.get_available_gates().empty(); ++bar) {
        if (stop_requested()) {
            return router->get_device();
        }
        auto waitlist = topo_wrap.get_available_gates();
        assert(!waitlist.empty());

        auto gate_idx = get_executable_gate(*router);
        if (gate_idx == std::nullopt) {
            gate_idx = greedy_fallback(*router, waitlist);
        }
        assert(gate_idx.has_value());
        route_one_gate(*router, gate_idx.value());
        // call calculate_total_routing_time
        // if (!done) {
        //     auto total_time = calculate_total_routing_time(*router, gate_idx.value());
        //     fmt::println("Total time to route all remaining gates: {}", total_time);
        //     done = true;
        // }

        spdlog::debug("waitlist: [{}] {}\n", fmt::join(waitlist, ", "), gate_idx.value());

        ++count;
    }
    assert(count == _circuit_topology.get_num_gates());
    return router->get_device();
}

/**
 * @brief If no executable gate, i.e. need routing, pick the one with best greedy cost
 *
 * @param router
 * @param waitlist
 * @param gateIdx
 * @return size_t
 */
size_t GreedyScheduler::greedy_fallback(Router& router,
                                        std::vector<size_t> const& waitlist) const {
    std::vector<size_t> cost_list(waitlist.size(), 0);

    for (size_t i = 0; i < waitlist.size(); ++i) {
        auto const& gate = _circuit_topology.get_gate(waitlist[i]);
        cost_list[i]     = router.get_gate_cost(gate, _conf.available_time_strategy, _conf.apsp_coeff);
    }

    auto list_idx = _conf.cost_type == MinMaxOptionType::max
                        ? max_element(cost_list.begin(), cost_list.end()) - cost_list.begin()
                        : min_element(cost_list.begin(), cost_list.end()) - cost_list.begin();
    return waitlist[list_idx];
}

}  // namespace qsyn::duostra
