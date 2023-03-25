/****************************************************************************
  FileName     [ schedulerGreedy.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Greedy Scheduler member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <cassert>

#include "scheduler.h"

using namespace std;

extern size_t verbose;

// SECTION - Class Topology Member Functions

class TopologyCandidate {
public:
    TopologyCandidate(const CircuitTopo& topo, size_t candidate)
        : _circuitTopology(topo), _candidates(candidate) {}

    vector<size_t> getAvailableGates() const {
        auto& gates = _circuitTopology.getAvailableGates();
        if (gates.size() < _candidates)
            return gates;
        return vector<size_t>(gates.begin(), gates.begin() + _candidates);
    }

private:
    const CircuitTopo& _circuitTopology;
    size_t _candidates;
};

// SECTION - Struct GreedyConf Member Functions

/**
 * @brief Construct a new Greedy Conf:: Greedy Conf object
 *
 */
GreedyConf::GreedyConf()
    : availableType(true),
      costType(false),
      _candidates(ERROR_CODE),
      _APSPCoeff(1) {}

// SECTION - Class GreedyScheduler Member Functions

/**
 * @brief Construct a new Greedy Scheduler:: Greedy Scheduler object
 *
 * @param topo
 */
GreedyScheduler::GreedyScheduler(unique_ptr<CircuitTopo> topo) : BaseScheduler(move(topo)) {}

/**
 * @brief Construct a new Greedy Scheduler:: Greedy Scheduler object
 *
 * @param other
 */
GreedyScheduler::GreedyScheduler(const GreedyScheduler& other) : BaseScheduler(other), _conf(other._conf) {}

/**
 * @brief Construct a new Greedy Scheduler:: Greedy Scheduler object
 *
 * @param other
 */
GreedyScheduler::GreedyScheduler(GreedyScheduler&& other) : BaseScheduler(move(other)), _conf(other._conf) {}

/**
 * @brief Get scheduler
 *
 * @return unique_ptr<BaseScheduler>
 */
unique_ptr<BaseScheduler> GreedyScheduler::clone() const {
    return make_unique<GreedyScheduler>(*this);
}

/**
 * @brief Assign gates
 *
 * @param router
 */
void GreedyScheduler::assignGates(unique_ptr<Router> router) {
    cout << "Greedy scheduler running..." << endl;

    [[maybe_unused]] size_t count = 0;
    auto topoWrap = TopologyCandidate(*_circuitTopology, _conf._candidates);
    for (TqdmWrapper bar{_circuitTopology->getNumGates()};
         !topoWrap.getAvailableGates().empty(); ++bar) {
        auto waitlist = topoWrap.getAvailableGates();
        assert(waitlist.size() > 0);

        size_t gateIdx = getExecutable(*router);
        gateIdx = greedyFallback(*router, waitlist, gateIdx);
        routeOneGate(*router, gateIdx);

        if (verbose > 3) cout << "waitlist: " << waitlist << " " << gateIdx << "\n\n";

        ++count;
    }
    assert(count == _circuitTopology->getNumGates());
}

/**
 * @brief If no executable gate, i.e. need routing, pick the one with best greedy cost
 *
 * @param router
 * @param waitlist
 * @param gateIdx
 * @return size_t
 */
size_t GreedyScheduler::greedyFallback(Router& router,
                                       const std::vector<size_t>& waitlist,
                                       size_t gateIdx) const {
    if (gateIdx != ERROR_CODE)
        return gateIdx;

    vector<size_t> costList(waitlist.size(), 0);

    for (size_t i = 0; i < waitlist.size(); ++i) {
        const auto& gate = _circuitTopology->getGate(waitlist[i]);
        costList[i] = router.getGateCost(gate, _conf.availableType, _conf._APSPCoeff);
    }

    auto listIdx = _conf.costType
                       ? max_element(costList.begin(), costList.end()) - costList.begin()
                       : min_element(costList.begin(), costList.end()) - costList.begin();
    return waitlist[listIdx];
}