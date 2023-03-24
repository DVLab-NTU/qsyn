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
extern size_t verbose;

void Duostra::makeDepend() {
    vector<Gate> all_gates;
    for (const auto& g : _logicalCircuit->getGates()) {
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
        Gate temp_gate{id, type, g->getPhase(), temp};
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
    if(verbose>3) cout << "Creating dependency of quantum circuit..." << endl;
    makeDepend();
    unique_ptr<CircuitTopo> topo;
    topo = make_unique<CircuitTopo>(_dependency);

    if(verbose>3) cout << "Creating device..." << endl;
    if (topo->get_num_qubits() > _device.getNQubit()) {
        cerr << "You cannot assign more qubits than the device." << endl;
        abort();
    }

    if(verbose>3) cout << "Initial placing..." << endl;
    string placer = "dfs";
    auto plc = getPlacer(placer);
    plc->place_and_assign(_device);

    // scheduler

    if(verbose>3) cout << "Creating Scheduler..." << endl;
    string scheduler_typ = "search";
    auto sched = getScheduler(scheduler_typ, move(topo));

    // router
    if(verbose>3) cout << "Creating Router..." << endl;
    string router_typ = "duostra";
    bool orient = true;
    string greedyCost = "end";
    string cost = (scheduler_typ == "greedy") ? greedyCost : "start";
    auto router = make_unique<Router>(move(_device), router_typ, cost, orient);

    // routing
    cout << "Routing..." << endl;
    sched->assign_gates_and_sort(move(router));

    cout << "Duostra Result: " << endl;
    cout << endl;
    cout << "Placer:         " << placer << endl;
    cout << "Scheduler:      " << scheduler_typ << endl;
    cout << "Router:         " << router_typ << endl;
    cout << endl;
    cout << "Mapping Depth:  " << sched->get_final_cost() << "\n";
    cout << "Total Time:     " << sched->get_total_time() << "\n";
    cout << "#SWAP:          " << sched->get_swap_num() << "\n";
    cout << endl;
    // sched->write_assembly();
    assert(sched->is_sorted());
    _result = sched->get_operations();
    buildCircuitByResult();
    cout.clear();
    return sched->get_final_cost();
}

void Duostra::print_assembly() const {
    cout << "Mapping Result: " << endl;
    cout << endl;
    for (size_t i = 0; i < _result.size(); ++i) {
        const auto& op = _result.at(i);
        string operator_name{gateType2Str[op.get_operator()]};
        cout << left << setw(5) << operator_name << " ";
        tuple<size_t, size_t> qubits = op.get_qubits();
        string res = "q[" + to_string(get<0>(qubits)) + "]";
        if (get<1>(qubits) != ERROR_CODE) {
            res = res + ",q[" + to_string(get<1>(qubits)) + "]";
        }
        res += ";";
        cout << left << setw(20) << res;
        cout << " // (" << op.get_op_time() << "," << op.get_cost() << ")\n";
    }
}

void Duostra::buildCircuitByResult() {
    _physicalCircuit->addQubit(_device.getNQubit());
    for (const auto& operation : _result) {
        string gateName{gateType2Str[operation.get_operator()]};
        tuple<size_t, size_t> qubits = operation.get_qubits();
        vector<size_t> qu;
        qu.emplace_back(get<0>(qubits));
        if (get<1>(qubits) != ERROR_CODE) {
            qu.emplace_back(get<1>(qubits));
        }
        if(operation.get_operator() == GateType::SWAP){
            // NOTE - Decompose SWAP into three CX
            vector<size_t> quReverse;
            quReverse.emplace_back(get<1>(qubits));
            quReverse.emplace_back(get<0>(qubits));
            _physicalCircuit->addGate("CX", qu, Phase(0), true);
            _physicalCircuit->addGate("CX", quReverse, Phase(0), true);
            _physicalCircuit->addGate("CX", qu, Phase(0), true);
        }
        else
            _physicalCircuit->addGate(gateName, qu, operation.getPhase(), true);
    }
}
