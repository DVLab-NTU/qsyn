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

/**
 * @brief Update available gates by executed gate id. Execuded gate also removed
 *  if conditions is satisfied
 *
 * @param executed Index of executed gate
 */
void Topology::update_avail_gates(size_t executed) {
    assert(find(begin(avail_gates_), end(avail_gates_), executed) !=
           end(avail_gates_));
    const QCirGate* g_exec = get_gate(executed);
    avail_gates_.erase(remove(begin(avail_gates_), end(avail_gates_), executed),
                       end(avail_gates_));
    assert(g_exec->getId() == executed);

    executed_gates_[executed] = 0;

    for (const auto& bitInfo : g_exec->getQubits()) {
        if (bitInfo._child == nullptr) continue;
        if (get_gate(bitInfo._child->getId())->is_avail(executed_gates_)) {
            avail_gates_.push_back(bitInfo._child->getId());
        }
    }

    vector<size_t> gates_to_trim;
    for (const auto& bitInfo : g_exec->getQubits()) {
        QCirGate* prev_gate = bitInfo._parent;
        if (prev_gate == nullptr) continue;

        ++executed_gates_[prev_gate->getId()];

        size_t countTrueNext = 0;
        for (const auto& bitNext : prev_gate->getQubits()) {
            if (bitNext._child != nullptr) countTrueNext++;
        }
        if (executed_gates_[prev_gate->getId()] >= countTrueNext) {
            gates_to_trim.push_back(prev_gate->getId());
        }
    }

    for (size_t gate_id : gates_to_trim) {
        executed_gates_.erase(gate_id);
    }
}
