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

size_t Duostra::flow() {
    cout << "Creating dependency of quantum circuit..." << endl;
    unique_ptr<CircuitTopo> topo;
    topo = make_unique<CircuitTopo>(_logicalCircuit);

    cout << "Creating device..." << endl;
    if (topo->get_num_qubits() > _device->getNQubit()) {
        cerr << "You cannot assign more qubits than the device." << endl;
        abort();
    }

    cout << "Initial placing..." << endl;
    auto plc = getPlacer("dfs");
    plc->place_and_assign(_device);

    // scheduler

    cout << "Creating Scheduler..." << endl;
    string scheduler_typ = "greedy";
    auto sched = getScheduler(scheduler_typ, move(topo));

    // router
    cout << "Creating Router..." << endl;
    string router_typ = "duostra";
    bool orient = true;
    string greedyCost = "end";
    string cost = (scheduler_typ == "greedy") ? greedyCost : "start";
    auto router = make_unique<Router>(_device, router_typ, cost, orient);

    // routing
    cout << "Routing..." << endl;
    sched->assign_gates_and_sort(move(router));

    cout << "final cost: " << sched->get_final_cost() << "\n";
    cout << "total time: " << sched->get_total_time() << "\n";
    cout << "total swaps: " << sched->get_swap_num() << "\n";

    cout.clear();
    return sched->get_final_cost();
}