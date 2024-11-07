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
#include <memory>
#include <optional>

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
 * @brief Clone scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
std::unique_ptr<BaseScheduler> GreedyScheduler::clone() const {
    return std::make_unique<GreedyScheduler>(*this);
}

/**
 * @brief Greedy fallback to select the best gate based on cost strategy
 *
 * @param router
 * @param waitlist
 * @return size_t Selected gate index
 */
size_t GreedyScheduler::greedy_fallback(Router& router,
                                        std::vector<size_t> const& waitlist) const {
    std::vector<size_t> cost_list(waitlist.size(), 0);

    for (size_t i = 0; i < waitlist.size(); ++i) {
        auto const& gate = _circuit_topology.get_gate(waitlist[i]);
        cost_list[i] = router.get_gate_cost(gate, _conf.available_time_strategy, _conf.apsp_coeff);
    }

    auto list_idx = _conf.cost_type == MinMaxOptionType::max
                        ? std::max_element(cost_list.begin(), cost_list.end()) - cost_list.begin()
                        : std::min_element(cost_list.begin(), cost_list.end()) - cost_list.begin();
    return waitlist[list_idx];
}

/**
 * @brief Set configuration
 *
 * @param conf
 */
void GreedyScheduler::set_conf(const GreedyConf& conf) {
    _conf = conf;
}

/**
 * @brief Calculate total time to route all remaining gates
 * 
 * @param router Current router state
 * @param next_gate_id ID of the next gate to be routed
 * @return size_t Total time to route all remaining gates
 */
size_t GreedyScheduler::get_estimated_cost(Router& router, size_t next_gate_id){
    // Clone the router and topology for simulation
    auto temp_router = router.clone();
    auto temp_topo = _circuit_topology.clone();
    auto temp_topo_wrap = TopologyCandidate(*temp_topo, _conf.num_candidates);

    // Create a temporary GreedyScheduler to track operations
    GreedyScheduler temp_scheduler(*temp_topo, _tqdm);
    temp_scheduler._conf = _conf; // Copy configuration

    // Route the next gate with forget=false to track operations
    temp_scheduler.route_one_gate(*temp_router, next_gate_id, false); // forget=false
    temp_topo->update_available_gates(next_gate_id);

    // Route remaining gates
    while (!temp_topo_wrap.get_available_gates().empty()) {
        if (stop_requested()) {
            break; // Continue to sum what is processed so far
        }
        auto waitlist = temp_topo_wrap.get_available_gates();
        assert(!waitlist.empty());

        auto gate_idx_opt = temp_scheduler.get_executable_gate(*temp_router);
        size_t gate_idx;
        if (gate_idx_opt.has_value()) {
            gate_idx = gate_idx_opt.value();
        } else {
            gate_idx = temp_scheduler.greedy_fallback(*temp_router, waitlist);
        }
        assert(gate_idx < _circuit_topology.get_num_gates());
        fmt::println("Routing gate {}", gate_idx);

        // Route gate with forget=false to track operations
        temp_scheduler.route_one_gate(*temp_router, gate_idx, false); // forget=false
        temp_topo->update_available_gates(gate_idx);
    }

    // Sort the operations to satisfy the assertion in get_total_time()
    temp_scheduler._sort();

    // Calculate total_time as the sum of all operation durations
    size_t total_time = temp_scheduler.get_total_time();

    return total_time;
}

/**
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
GreedyScheduler::Device GreedyScheduler::_assign_gates(std::unique_ptr<Router> router) {
    size_t count = 0;
    auto topo_wrap = TopologyCandidate(_circuit_topology, _conf.num_candidates);
    for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates(), _tqdm};
         !topo_wrap.get_available_gates().empty(); ++bar) {
        if (stop_requested()) {
            return router->get_device();
        }
        auto waitlist = topo_wrap.get_available_gates();
        assert(!waitlist.empty());

        auto gate_idx_opt = get_executable_gate(*router);
        size_t gate_idx;
        if (gate_idx_opt.has_value()) {
            gate_idx = gate_idx_opt.value();
        } else {
            gate_idx = greedy_fallback(*router, waitlist);
        }
        assert(gate_idx < _circuit_topology.get_num_gates());

        if (count == 0) {
            // Calculate total routing time for the next gate
            auto total_time = get_estimated_cost(*router, gate_idx);
            spdlog::debug("Total time to route all remaining gates: {}", total_time);
            fmt::println("Total time to route all remaining gates: {}", total_time);
        }

        route_one_gate(*router, gate_idx, false); // forget=false

        spdlog::debug("waitlist: [{}] {}\n", fmt::join(waitlist, ", "), gate_idx);

        ++count;
    }
    assert(count == _circuit_topology.get_num_gates());
    return router->get_device();
}

}  // namespace qsyn::duostra
