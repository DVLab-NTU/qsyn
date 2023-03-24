/****************************************************************************
  FileName     [ circuitTopology.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define class circuitTopology member functions ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "circuitTopology.h"

using namespace std;

void CircuitTopo::update_avail_gates(size_t executed) {
    assert(find(begin(avail_gates_), end(avail_gates_), executed) !=
           end(avail_gates_));
    const Gate& g_exec = get_gate(executed);
    avail_gates_.erase(remove(begin(avail_gates_), end(avail_gates_), executed),
                       end(avail_gates_));
    assert(g_exec.get_id() == executed);

    executed_gates_[executed] = 0;
    for (size_t next : g_exec.get_nexts()) {
        if (get_gate(next).is_avail(executed_gates_)) {
            avail_gates_.push_back(next);
        }
    }

    vector<size_t> gates_to_trim;
    for (size_t prev_id : g_exec.get_prevs()) {
        const auto& prev_gate = get_gate(prev_id);
        ++executed_gates_[prev_id];

        if (executed_gates_[prev_id] >= prev_gate.get_nexts().size()) {
            gates_to_trim.push_back(prev_id);
        }
    }
    for (size_t gate_id : gates_to_trim) {
        executed_gates_.erase(gate_id);
    }
}

void CircuitTopo::print_gates_with_next() {
    cout << "Print successors of each gate" << endl;
    const auto& gates = dep_graph_->gates();
    for (size_t i = 0; i < gates.size(); i++) {
        vector<size_t> temp = gates[i].get_nexts();
        cout << gates[i].get_id() << "(" << gates[i].get_type() << ") || ";
        for (size_t j = 0; j < temp.size(); j++) {
            cout << temp[j] << " ";
        }
        cout << endl;
    }
}

void CircuitTopo::print_gates_with_prev() {
    cout << "Print predecessors of each gate" << endl;
    const auto& gate = dep_graph_->gates();
    for (size_t i = 0; i < gate.size(); i++) {
        const auto& prevs = gate.at(i).get_prevs();
        cout << gate.at(i).get_id() << "(" << gate.at(i).get_type() << ") || ";

        for (size_t j = 0; j < prevs.size(); j++) {
            cout << prevs[j] << " ";
        }
        cout << endl;
    }
}