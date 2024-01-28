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
#include "util/util.hpp"

namespace qsyn::qcir {

/**
 * @brief Append the target to current QCir
 *
 * @param other
 * @return QCir*
 */
QCir* QCir::compose(QCir const& other) {
    auto const targ_qubits = other.get_qubits();
    for (auto& qubit : targ_qubits) {
        if (get_qubit(qubit->get_id()) == nullptr)
            insert_qubit(qubit->get_id());
    }
    other.update_topological_order();
    for (auto& targ_gate : other.get_topologically_ordered_gates()) {
        QubitIdList qubits;
        for (auto const& b : targ_gate->get_qubits()) {
            qubits.emplace_back(b._qubit);
        }
        add_gate(targ_gate->get_type_str(), qubits, targ_gate->get_phase(), true);
    }
    return this;
}

/**
 * @brief Tensor the target to current tensor of QCir
 *
 * @param other
 * @return QCir*
 */
QCir* QCir::tensor_product(QCir const& other) {
    std::unordered_map<size_t, QCirQubit*> old_q2_new_q;
    auto const targ_qubits = other.get_qubits();
    for (auto& qubit : targ_qubits) {
        old_q2_new_q[qubit->get_id()] = push_qubit();
    }
    other.update_topological_order();
    for (auto& targ_gate : other.get_topologically_ordered_gates()) {
        QubitIdList qubits;
        for (auto const& b : targ_gate->get_qubits()) {
            qubits.emplace_back(old_q2_new_q[b._qubit]->get_id());
        }
        add_gate(targ_gate->get_type_str(), qubits, targ_gate->get_phase(), true);
    }
    return this;
}

/**
 * @brief Perform DFS from currentGate
 *
 * @param currentGate the gate to start DFS
 */
void QCir::_dfs(QCirGate* curr_gate) const {
    std::stack<std::pair<bool, QCirGate*>> dfs;

    if (!curr_gate->is_visited(_global_dfs_counter)) {
        dfs.emplace(false, curr_gate);
    }
    while (!dfs.empty()) {
        auto node = dfs.top();
        dfs.pop();
        if (node.first) {
            _topological_order.emplace_back(node.second);
            continue;
        }
        if (node.second->is_visited(_global_dfs_counter)) {
            continue;
        }
        node.second->set_visited(_global_dfs_counter);
        dfs.emplace(true, node.second);

        for (auto const& info : node.second->get_qubits()) {
            if (info._next != nullptr) {
                if (!(info._next->is_visited(_global_dfs_counter))) {
                    dfs.emplace(false, info._next);
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
std::vector<QCirGate*> const& QCir::update_topological_order() const {
    _topological_order.clear();
    if (_qgates.empty())
        return _topological_order;
    _global_dfs_counter++;
    auto dummy = new QCirGate(0, GateRotationCategory::id, dvlab::Phase(0));
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
    auto print_gate_id = [](QCirGate* gate) {
        fmt::println("{}", gate->get_id());
    };
    topological_traverse(print_gate_id);
    return true;
}

/**
 * @brief Update execution time of gates
 */
void QCir::update_gate_time() const {
    update_topological_order();
    auto lambda = [](QCirGate* curr_gate) {
        std::vector<QubitInfo> info = curr_gate->get_qubits();
        size_t max_time             = 0;
        for (size_t i = 0; i < info.size(); i++) {
            if (info[i]._prev == nullptr)
                continue;
            if (info[i]._prev->get_time() > max_time)
                max_time = info[i]._prev->get_time();
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

    _gate_id            = 0;
    _qubit_id           = 0;
    _dirty              = true;
    _global_dfs_counter = 1;
}

void QCir::adjoint() {
    for (auto& g : _qgates) {
        g->adjoint();
        auto qubits = g->get_qubits();
        for (auto& q : qubits) {
            std::swap(q._prev, q._next);
        }
        g->set_qubits(qubits);
    }

    for (auto& q : _qubits) {
        auto first = q->get_first();
        auto last  = q->get_last();
        q->set_first(last);
        q->set_last(first);
    }

    _dirty = true;
}

}  // namespace qsyn::qcir
