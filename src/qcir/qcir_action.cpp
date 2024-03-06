/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cstddef>
#include <stack>
#include <unordered_set>

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
    auto targ_qubits = other.get_qubits();
    for (auto& qubit : targ_qubits) {
        if (get_qubit(qubit->get_id()) == nullptr)
            insert_qubit(qubit->get_id());
    }
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
    auto targ_qubits = other.get_qubits();
    for (auto& qubit : targ_qubits) {
        old_q2_new_q[qubit->get_id()] = push_qubit();
    }
    for (auto& targ_gate : other.get_topologically_ordered_gates()) {
        QubitIdList qubits;
        for (auto const& b : targ_gate->get_qubits()) {
            qubits.emplace_back(old_q2_new_q[b._qubit]->get_id());
        }
        add_gate(targ_gate->get_type_str(), qubits, targ_gate->get_phase(), true);
    }
    return this;
}

namespace {

/**
 * @brief Perform DFS from currentGate
 *
 * @param currentGate the gate to start DFS
 */
void dfs(QCirGate* curr_gate, std::vector<QCirGate*>& topo_order) {
    std::stack<std::pair<bool, QCirGate*>> dfs_stack;

    std::unordered_set<QCirGate*> visited;

    if (!visited.contains(curr_gate)) {
        dfs_stack.emplace(false, curr_gate);
    }
    while (!dfs_stack.empty()) {
        auto node = dfs_stack.top();
        dfs_stack.pop();
        if (node.first) {
            topo_order.emplace_back(node.second);
            continue;
        }
        if (visited.contains(node.second)) {
            continue;
        }
        visited.insert(node.second);
        dfs_stack.emplace(true, node.second);

        for (auto const& info : node.second->get_qubits()) {
            if (info._next != nullptr) {
                if (!visited.contains(info._next)) {
                    dfs_stack.emplace(false, info._next);
                }
            }
        }
    }
}

}  // namespace

/**
 * @brief Update topological order
 *
 * @return const vector<QCirGate*>&
 */
std::vector<QCirGate*> const& QCir::_update_topological_order() const {
    if (!_dirty)
        return _topological_order;

    _topological_order.clear();
    if (_qgates.empty())
        return _topological_order;
    auto dummy    = new QCirGate(0, GateRotationCategory::id, dvlab::Phase(0));
    auto children = dummy->get_qubits();
    for (size_t i = 0; i < _qubits.size(); i++) {
        children.push_back(
            {._qubit    = 0,
             ._prev     = nullptr,
             ._next     = _qubits[i]->get_first(),
             ._isTarget = false});
    }
    dummy->set_qubits(children);
    dfs(dummy, _topological_order);
    _topological_order.pop_back();  // pop dummy
    reverse(_topological_order.begin(), _topological_order.end());
    assert(_topological_order.size() == _qgates.size());
    delete dummy;

    _dirty = false;
    return _topological_order;
}

/**
 * @brief Print topological order
 */
bool QCir::print_topological_order() const {
    auto print_gate_id = [](QCirGate* gate) {
        fmt::println("{}", gate->get_id());
    };
    std::ranges::for_each(get_topologically_ordered_gates(), print_gate_id);
    return true;
}

/**
 * @brief Reset QCir
 *
 */
void QCir::reset() {
    _qgates.clear();
    _qubits.clear();
    _topological_order.clear();

    _gate_id  = 0;
    _qubit_id = 0;
    _dirty    = true;
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
