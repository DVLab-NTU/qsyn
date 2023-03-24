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
class TopologyCandidate {
public:
    TopologyCandidate(const CircuitTopo& topo, size_t candidate)
        : topo_(topo), cands_(candidate) {}

    std::vector<size_t> get_avail_gates() const {
        auto& gates = topo_.get_avail_gates();

        if (gates.size() < cands_) {
            return gates;
        }

        return std::vector<size_t>(gates.begin(), gates.begin() + cands_);
    }

private:
    const CircuitTopo& topo_;
    size_t cands_;
};

GreedyScheduler::GreedyScheduler(unique_ptr<CircuitTopo> topo)
    : BaseScheduler(move(topo)) {}

GreedyScheduler::GreedyScheduler(const GreedyScheduler& other) : BaseScheduler(other), conf_(other.conf_) {}

GreedyScheduler::GreedyScheduler(GreedyScheduler&& other) : BaseScheduler(move(other)), conf_(other.conf_) {}

void GreedyScheduler::assign_gates(unique_ptr<Router> router) {
    cout << "Greedy scheduler running..." << endl;

    [[maybe_unused]] size_t count = 0;
    auto topo_wrap = TopologyCandidate(*topo_, conf_.candidates);
    for (TqdmWrapper bar{topo_->get_num_gates()};
         !topo_wrap.get_avail_gates().empty(); ++bar) {
        auto wait_list = topo_wrap.get_avail_gates();
        assert(wait_list.size() > 0);

        size_t gate_idx = get_executable(*router);
        gate_idx = greedy_fallback(*router, wait_list, gate_idx);
        route_one_gate(*router, gate_idx);

        if (verbose > 3) cout << "waitlist: " << wait_list << " " << gate_idx << "\n\n";

        ++count;
    }
    assert(count == topo_->get_num_gates());
}

size_t GreedyScheduler::greedy_fallback(Router& router,
                                        const std::vector<size_t>& wait_list,
                                        size_t gate_idx) const {
    if (gate_idx != ERROR_CODE) {
        return gate_idx;
    }
    vector<size_t> cost_list(wait_list.size(), 0);

    for (size_t i = 0; i < wait_list.size(); ++i) {
        const auto& gate = topo_->get_gate(wait_list[i]);
        cost_list[i] =
            router.get_gate_cost(gate, conf_.avail_typ, conf_.apsp_coef);
    }

    auto list_idx = conf_.cost_typ
                        ? max_element(cost_list.begin(), cost_list.end()) -
                              cost_list.begin()
                        : min_element(cost_list.begin(), cost_list.end()) -
                              cost_list.begin();
    return wait_list[list_idx];
}

unique_ptr<BaseScheduler> GreedyScheduler::clone() const {
    return make_unique<GreedyScheduler>(*this);
}