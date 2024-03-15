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
#include "tl/to.hpp"

namespace qsyn::qcir {

/**
 * @brief Append the target to current QCir
 *
 * @param other
 * @return QCir*
 */
QCir& QCir::compose(QCir const& other) {
    auto targ_qubits = other.get_qubits();
    for (auto& qubit : targ_qubits) {
        if (get_qubit(qubit->get_id()) == nullptr)
            insert_qubit(qubit->get_id());
    }
    for (auto& targ_gate : other.get_gates()) {
        QubitIdList qubits;
        for (auto i : std::views::iota(0ul, targ_gate->get_num_qubits())) {
            qubits.emplace_back(targ_gate->get_qubit(i));
        }
        append(targ_gate->get_operation(), qubits);
    }
    return *this;
}

/**
 * @brief Tensor the target to current tensor of QCir
 *
 * @param other
 * @return QCir*
 */
QCir& QCir::tensor_product(QCir const& other) {
    std::unordered_map<QubitIdType, QubitIdType> old_to_new_qubit;
    auto targ_qubits = other.get_qubits();
    for (auto& qubit : targ_qubits) {
        auto q                            = push_qubit();
        old_to_new_qubit[qubit->get_id()] = q->get_id();
    }
    for (auto& targ_gate : other.get_gates()) {
        QubitIdList qubits;
        for (auto i : std::views::iota(0ul, targ_gate->get_num_qubits())) {
            qubits.emplace_back(old_to_new_qubit[targ_gate->get_qubit(i)]);
        }
        append(targ_gate->get_operation(), qubits);
    }
    return *this;
}

namespace {

/**
 * @brief Perform DFS from currentGate
 *
 * @param currentGate the gate to start DFS
 */
std::vector<QCirGate*> dfs(QCir const& qcir) {
    std::stack<std::pair<bool, QCirGate*>> dfs_stack;
    std::vector<QCirGate*> topo_order;
    std::unordered_set<QCirGate*> visited;

    for (auto const& gate : qcir.get_qubits() | std::views::transform([](auto const& q) { return q->get_first(); })) {
        if (gate != nullptr) {
            dfs_stack.emplace(false, gate);
        }
    }

    while (!dfs_stack.empty()) {
        auto [children_visited, node] = dfs_stack.top();
        dfs_stack.pop();
        if (children_visited) {
            topo_order.emplace_back(node);
            continue;
        }
        if (visited.contains(node)) {
            continue;
        }
        visited.insert(node);
        dfs_stack.emplace(true, node);

        assert(qcir.get_successors(node->get_id()).size() == node->get_num_qubits());

        for (auto const& succ : qcir.get_successors(node->get_id())) {
            if (succ.has_value() && !visited.contains(qcir.get_gate(succ))) {
                dfs_stack.emplace(false, qcir.get_gate(succ));
            }
        }
    }

    std::ranges::reverse(topo_order);

    return topo_order;
}

}  // namespace

/**
 * @brief Update topological order
 *
 * @return const vector<QCirGate*>&
 */
void QCir::_update_topological_order() const {
    if (!_dirty)
        return;

    _gate_list = dfs(*this);
    assert(_gate_list.size() == get_num_gates());

    _dirty = false;
}

/**
 * @brief Reset QCir
 *
 */
void QCir::reset() {
    _qubits.clear();
    _gate_list.clear();

    _gate_id  = 0;
    _qubit_id = 0;
    _dirty    = true;
}

void QCir::adjoint() {
    for (auto& g : _gate_list) {
        g->adjoint();
        auto old_succs = get_successors(g->get_id());
        auto old_preds = get_predecessors(g->get_id());
        _set_successors(g->get_id(), old_preds);
        _set_predecessors(g->get_id(), old_succs);
    }

    for (auto& q : _qubits) {
        auto first = q->get_first();
        auto last  = q->get_last();
        q->set_first(last);
        q->set_last(first);
    }

    _dirty = true;
}

void QCir::concat(QCir const& other, std::map<QubitIdType /* new */, QubitIdType /* orig */> const& qubit_map) {
    for (auto const& new_gate : other.get_gates()) {
        auto const tmp = new_gate->get_qubits();  // circumvent g++ 11.4 compilation bug
        auto qubits    = tmp |
                      std::views::transform([&qubit_map](auto const& qubit) {
                          return qubit_map.at(qubit);
                      }) |
                      tl::to<std::vector>();
        append(new_gate->get_operation(), qubits);
    }
}

}  // namespace qsyn::qcir
