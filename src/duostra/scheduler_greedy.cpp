/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Greedy Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/format.h>

#include <algorithm>
#include <cassert>

#include "./scheduler.hpp"
#include "./variables.hpp"

extern size_t VERBOSE;

extern bool stop_requested();

namespace qsyn::duostra {

// SECTION - Class Topology Member Functions

class TopologyCandidate {
public:
    TopologyCandidate(CircuitTopology const& topo, size_t candidate)
        : _circuit_topology(topo), _num_candidates(candidate) {}

    std::vector<size_t> get_available_gates() const {
        auto& gates = _circuit_topology.get_available_gates();
        if (gates.size() < _num_candidates)
            return gates;
        return std::vector<size_t>(gates.begin(), dvlab::iterator::next(gates.begin(), _num_candidates));
    }

private:
    CircuitTopology const& _circuit_topology;
    size_t _num_candidates;
};

// SECTION - Struct GreedyConf Member Functions

/**
 * @brief Construct a new Greedy Conf:: Greedy Conf object
 *
 */
GreedyConf::GreedyConf()
    : _availableType(DUOSTRA_AVAILABLE),
      _costType(DUOSTRA_COST),
      _candidates(DUOSTRA_CANDIDATES),
      _APSPCoeff(DUOSTRA_APSP_COEFF) {}

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
 * @brief Assign gates
 *
 * @param router
 * @return Device
 */
GreedyScheduler::Device GreedyScheduler::_assign_gates(std::unique_ptr<Router> router) {
    [[maybe_unused]] size_t count = 0;
    auto topo_wrap = TopologyCandidate(_circuit_topology, _conf._candidates);
    for (dvlab::TqdmWrapper bar{_circuit_topology.get_num_gates(), _tqdm};
         !topo_wrap.get_available_gates().empty(); ++bar) {
        if (stop_requested()) {
            return router->get_device();
        }
        auto waitlist = topo_wrap.get_available_gates();
        assert(waitlist.size() > 0);

        size_t gate_idx = get_executable(*router);
        gate_idx = greedy_fallback(*router, waitlist, gate_idx);
        route_one_gate(*router, gate_idx);

        if (VERBOSE > 3)
            fmt::println("waitlist: [{}] {}\n", fmt::join(waitlist, ", "), gate_idx);

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
                                        std::vector<size_t> const& waitlist,
                                        size_t gate_id) const {
    if (gate_id != SIZE_MAX)
        return gate_id;

    std::vector<size_t> cost_list(waitlist.size(), 0);

    for (size_t i = 0; i < waitlist.size(); ++i) {
        auto const& gate = _circuit_topology.get_gate(waitlist[i]);
        cost_list[i] = router.get_gate_cost(gate, _conf._availableType, _conf._APSPCoeff);
    }

    auto list_idx = _conf._costType
                        ? max_element(cost_list.begin(), cost_list.end()) - cost_list.begin()
                        : min_element(cost_list.begin(), cost_list.end()) - cost_list.begin();
    return waitlist[list_idx];
}

}  // namespace qsyn::duostra
