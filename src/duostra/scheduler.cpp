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

BaseScheduler::BaseScheduler(unique_ptr<CircuitTopo> topo) : topo_(move(topo)), ops_({}) {}

BaseScheduler::BaseScheduler(const BaseScheduler& other) : topo_(other.topo_->clone()), ops_(other.ops_) {}

BaseScheduler::BaseScheduler(BaseScheduler&& other) : topo_(move(other.topo_)), ops_(move(other.ops_)) {}

unique_ptr<BaseScheduler> BaseScheduler::clone() const {
    return make_unique<BaseScheduler>(*this);
}

void BaseScheduler::sort() {
    std::sort(ops_.begin(), ops_.end(), [](const Operation& a, const Operation& b) -> bool {
        return a.get_op_time() < b.get_op_time();
    });
    sorted_ = true;
}

void BaseScheduler::assign_gates(unique_ptr<Router> router) {
    cout << "Default scheduler running..." << endl;

    for (TqdmWrapper bar{topo_->get_num_gates()}; !bar.done(); ++bar) {
        route_one_gate(*router, bar.idx());
    }
}

size_t BaseScheduler::get_final_cost() const {
    assert(sorted_);
    return ops_.at(ops_.size() - 1).get_cost();
}

size_t BaseScheduler::get_total_time() const {
    assert(sorted_);

    size_t ret = 0;
    for (size_t i = 0; i < ops_.size(); ++i) {
        tuple<size_t, size_t> dur = ops_[i].get_duration();
        ret += std::get<1>(dur) - std::get<0>(dur);
    }
    return ret;
}

size_t BaseScheduler::get_swap_num() const {
    size_t ret = 0;
    for (size_t i = 0; i < ops_.size(); ++i) {
        if (ops_.at(i).get_operator() == GateType::SWAP) {
            ++ret;
        }
    }
    return ret;
}

size_t BaseScheduler::ops_cost() const {
    return std::max_element(
               ops_.begin(), ops_.end(),
               [](const Operation& a, const Operation& b) {
                   return a.get_cost() < b.get_cost();
               })
        ->get_cost();
}

size_t BaseScheduler::get_executable(Router& router) const {
    for (size_t gate_idx : topo_->get_avail_gates()) {
        if (router.is_executable(topo_->get_gate(gate_idx))) {
            return gate_idx;
        }
    }
    return ERROR_CODE;
}

size_t BaseScheduler::route_one_gate(Router& router, size_t gate_idx, bool forget) {
    const auto& gate = topo_->get_gate(gate_idx);
    auto ops{router.assign_gate(gate)};
    size_t max_cost = 0;
    for (const auto& op : ops) {
        if (op.get_cost() > max_cost) {
            max_cost = op.get_cost();
        }
    }
    if (!forget) {
        ops_.insert(ops_.end(), ops.begin(), ops.end());
    }
    topo_->update_avail_gates(gate_idx);
    return max_cost;
}

RandomScheduler::RandomScheduler(unique_ptr<CircuitTopo> topo) : BaseScheduler(move(topo)) {}

RandomScheduler::RandomScheduler(const RandomScheduler& other) : BaseScheduler(other) {}

RandomScheduler::RandomScheduler(RandomScheduler&& other) : BaseScheduler(move(other)) {}

void RandomScheduler::assign_gates(unique_ptr<Router> router) {
    cout << "Random scheduler running..." << endl;

    [[maybe_unused]] size_t count = 0;

    for (TqdmWrapper bar{topo_->get_num_gates()}; !bar.done(); ++bar) {
        auto& wait_list = topo_->get_avail_gates();
        assert(wait_list.size() > 0);
        srand(chrono::system_clock::now().time_since_epoch().count());

        size_t choose = rand() % wait_list.size();

        route_one_gate(*router, wait_list[choose]);
#ifdef DEBUG
        cout << wait_list << " " << wait_list[choose] << "\n\n";
#endif
        ++count;
    }

    assert(count == topo_->get_num_gates());
}

unique_ptr<BaseScheduler> RandomScheduler::clone() const {
    return make_unique<RandomScheduler>(*this);
}

StaticScheduler::StaticScheduler(unique_ptr<CircuitTopo> topo) : BaseScheduler(move(topo)) {}
StaticScheduler::StaticScheduler(const StaticScheduler& other) : BaseScheduler(other) {}
StaticScheduler::StaticScheduler(StaticScheduler&& other) : BaseScheduler(move(other)) {}

void StaticScheduler::assign_gates(unique_ptr<Router> router) {
    cout << "Static scheduler running..." << endl;

    [[maybe_unused]] size_t count = 0;

    for (TqdmWrapper bar{topo_->get_num_gates()}; !bar.done(); ++bar) {
        auto& wait_list = topo_->get_avail_gates();
        assert(wait_list.size() > 0);

        size_t gate_idx = get_executable(*router);
        if (gate_idx == ERROR_CODE) {
            gate_idx = wait_list[0];
        }

        route_one_gate(*router, gate_idx);

        ++count;
    }
    assert(count == topo_->get_num_gates());
}

unique_ptr<BaseScheduler> StaticScheduler::clone() const {
    return make_unique<StaticScheduler>(*this);
}

unique_ptr<BaseScheduler> getScheduler(const string& typ, unique_ptr<CircuitTopo> topo) {
    if (typ == "random") {
        return make_unique<RandomScheduler>(move(topo));
    } else if (typ == "static") {
        return make_unique<StaticScheduler>(move(topo));
    } else if (typ == "greedy") {
        return make_unique<GreedyScheduler>(move(topo));
    } else if (typ == "search") {
        return make_unique<SearchScheduler>(move(topo));
    } else if (typ == "old") {
        return make_unique<BaseScheduler>(move(topo));
    } else {
        cerr << typ << " is not a scheduler type" << endl;
        abort();
    }
}