/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cstddef>
#include <stack>

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"

using namespace std;

/**
 * @brief Append the target to current QCir
 *
 * @param target
 * @return QCir*
 */
QCir* QCir::compose(QCir const& target) {
    QCir copied_q_cir{target};
    vector<QCirQubit*> targ_qubits = copied_q_cir.get_qubits();
    for (auto& qubit : targ_qubits) {
        if (get_qubit(qubit->get_id()) == nullptr)
            insert_qubit(qubit->get_id());
    }
    copied_q_cir.update_topological_order();
    for (auto& targ_gate : copied_q_cir.get_topologically_ordered_gates()) {
        vector<size_t> bits;
        for (auto const& b : targ_gate->get_qubits()) {
            bits.emplace_back(b._qubit);
        }
        add_gate(targ_gate->get_type_str(), bits, targ_gate->get_phase(), true);
    }
    return this;
}

/**
 * @brief Tensor the target to current tensor of QCir
 *
 * @param target
 * @return QCir*
 */
QCir* QCir::tensor_product(QCir const& target) {
    QCir copied_q_cir{target};

    unordered_map<size_t, QCirQubit*> old_q2_new_q;
    vector<QCirQubit*> targ_qubits = copied_q_cir.get_qubits();
    for (auto& qubit : targ_qubits) {
        old_q2_new_q[qubit->get_id()] = push_qubit();
    }
    copied_q_cir.update_topological_order();
    for (auto& targ_gate : copied_q_cir.get_topologically_ordered_gates()) {
        vector<size_t> bits;
        for (auto const& b : targ_gate->get_qubits()) {
            bits.emplace_back(old_q2_new_q[b._qubit]->get_id());
        }
        add_gate(targ_gate->get_type_str(), bits, targ_gate->get_phase(), true);
    }
    return this;
}

/**
 * @brief Perform DFS from currentGate
 *
 * @param currentGate the gate to start DFS
 */
void QCir::_dfs(QCirGate* curr_gate) const {
    stack<pair<bool, QCirGate*>> dfs;

    if (!curr_gate->is_visited(_global_dfs_counter)) {
        dfs.push(make_pair(false, curr_gate));
    }
    while (!dfs.empty()) {
        pair<bool, QCirGate*> node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topological_order.emplace_back(node.second);
            continue;
        }
        if (node.second->is_visited(_global_dfs_counter)) {
            continue;
        }
        node.second->set_visited(_global_dfs_counter);
        dfs.push(make_pair(true, node.second));

        for (auto const& info : node.second->get_qubits()) {
            if ((info)._child != nullptr) {
                if (!((info)._child->is_visited(_global_dfs_counter))) {
                    dfs.push(make_pair(false, (info)._child));
                }
            }
        }
    }
}

/**
 * @brief Update topological order
 *
 * @return const vector<QCirGate*>&
 */
vector<QCirGate*> const& QCir::update_topological_order() const {
    _topological_order.clear();
    _global_dfs_counter++;
    QCirGate* dummy = new HGate(-1);
    for (size_t i = 0; i < _qubits.size(); i++) {
        dummy->add_dummy_child(_qubits[i]->get_first());
    }
    _dfs(dummy);
    _topological_order.pop_back();  // pop dummy
    reverse(_topological_order.begin(), _topological_order.end());
    assert(_topological_order.size() == _qgates.size());
    delete dummy;

    return _topological_order;
}

/**
 * @brief Print topological order
 */
bool QCir::print_topological_order() {
    auto test_lambda = [](QCirGate* gate) { cout << gate->get_id() << endl; };
    topological_traverse(test_lambda);
    return true;
}

/**
 * @brief Update execution time of gates
 */
void QCir::update_gate_time() const {
    update_topological_order();
    auto lambda = [](QCirGate* curr_gate) {
        vector<QubitInfo> info = curr_gate->get_qubits();
        size_t max_time = 0;
        for (size_t i = 0; i < info.size(); i++) {
            if (info[i]._parent == nullptr)
                continue;
            if (info[i]._parent->get_time() > max_time)
                max_time = info[i]._parent->get_time();
        }
        curr_gate->set_time(max_time + curr_gate->get_delay());
    };
    topological_traverse(lambda);
}
/**
 * @brief Reset QCir
 *
 */
void QCir::reset() {
    _qgates.clear();
    _qubits.clear();
    _topological_order.clear();

    _gate_id = 0;
    _qubit_id = 0;
    _dirty = true;
    _global_dfs_counter = 1;
}