/****************************************************************************
  FileName     [ duostra.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "duostra.h"

using namespace std;

void Duostra::makeDepend() {
    vector<Gate> all_gates;
    for (auto& g : _logicalCircuit->getGates()) {
        size_t id = g->getId();

        GateType type = g->getType();

        size_t q2 = ERROR_CODE;
        BitInfo first = g->getQubits()[0];
        BitInfo second = {};
        if (g->getQubits().size() > 1) {
            second = g->getQubits()[1];
            q2 = second._qubit;
        }

        tuple<size_t, size_t> temp{first._qubit, q2};
        Gate temp_gate{id, type, temp};
        if (first._parent != nullptr) temp_gate.add_prev(first._parent->getId());
        if (first._child != nullptr) temp_gate.add_next(first._child->getId());
        if (g->getQubits().size() > 1) {
            if (second._parent != nullptr) temp_gate.add_prev(second._parent->getId());
            if (second._child != nullptr) temp_gate.add_next(second._child->getId());
        }
        all_gates.push_back(move(temp_gate));
    }
    _dependency = make_shared<DependencyGraph>(_logicalCircuit->getNQubit(), move(all_gates));
}

size_t Duostra::flow() {
    cout << "Creating dependency of quantum circuit..." << endl;
    makeDepend();
    unique_ptr<CircuitTopo> topo;
    topo = make_unique<CircuitTopo>(_dependency);

    cout << "Creating device..." << endl;
    if (topo->get_num_qubits() > _device.getNQubit()) {
        cerr << "You cannot assign more qubits than the device." << endl;
        abort();
    }

    cout << "Initial placing..." << endl;
    auto plc = getPlacer("dfs");
    plc->place_and_assign(_device);

    // scheduler

    cout << "Creating Scheduler..." << endl;
    string scheduler_typ = "search";
    auto sched = getScheduler(scheduler_typ, move(topo));

    // router
    cout << "Creating Router..." << endl;
    string router_typ = "duostra";
    bool orient = true;
    string greedyCost = "end";
    string cost = (scheduler_typ == "greedy") ? greedyCost : "start";
    auto router = make_unique<Router>(move(_device), router_typ, cost, orient);

    // routing
    cout << "Routing..." << endl;
    sched->assign_gates_and_sort(move(router));

    cout << "final cost: " << sched->get_final_cost() << "\n";
    cout << "total time: " << sched->get_total_time() << "\n";
    cout << "total swaps: " << sched->get_swap_num() << "\n";

    cout.clear();
    return sched->get_final_cost();
}