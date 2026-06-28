/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/format.h>

#include <cassert>
#include <queue>
#include <set>
#include <stack>
#include <unordered_set>

#include "qcir/operation.hpp"
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
    // Ensure enough qubits
    if (get_num_qubits() < other.get_num_qubits()) {
        add_qubits(other.get_num_qubits() - get_num_qubits());
    }
    
    // Ensure enough classical bits
    if (get_num_classical_bits() < other.get_num_classical_bits()) {
        add_classical_bits(other.get_num_classical_bits() - get_num_classical_bits());
    }
    
    // Copy classical bit states from other circuit
    for (size_t i = 0; i < other.get_num_classical_bits(); ++i) {
        auto const& other_bit = other.get_classical_bits()[i];
        if (other_bit.has_value() && !_classical_bits[i].has_value()) {
            _classical_bits[i].set_value(other_bit.get_value());
        }
        // Note: Don't copy measurement state - gates will establish that
    }
    
    // Append gates from other circuit
    for (auto& targ_gate : other.get_gates()) {
        append(*targ_gate);
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
    size_t const old_num_qubits = get_num_qubits();
    add_qubits(other.get_num_qubits());
    for (auto& targ_gate : other.get_gates()) {
        auto const old_qubits = targ_gate->get_qubits();
        auto const qubits     = old_qubits | std::views::transform([&](auto qubit) {
                                return qubit + old_num_qubits;
                            }) |
                            tl::to<QubitIdList>();
        append(targ_gate->get_operation(), qubits);
    }
    return *this;
}

namespace {

/**
 * @brief Topological sort using wire successors only (develop-style DFS).
 */
std::vector<QCirGate*> topo_sort_wire_dfs(QCir const& qcir) {
    std::stack<std::pair<bool, QCirGate*>> dfs_stack;
    std::vector<QCirGate*> topo_order;
    std::unordered_set<QCirGate*> visited;

    for (auto const& gate :
         qcir.get_qubits() | std::views::transform([](auto const& q) {
             return q.get_first_gate();
         })) {
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

        assert(qcir.get_successors(node->get_id()).size() ==
               node->get_num_qubits());

        for (auto const& succ : qcir.get_successors(node->get_id())) {
            if (succ.has_value() && !visited.contains(qcir.get_gate(succ))) {
                dfs_stack.emplace(false, qcir.get_gate(succ));
            }
        }
    }

    std::ranges::reverse(topo_order);

    return topo_order;
}

/**
 * @brief Topological sort including classical control dependencies (Kahn).
 */
std::vector<QCirGate*> topo_sort_with_classical(QCir const& qcir) {
    std::vector<QCirGate*> topo_order;
    std::unordered_map<size_t, size_t> in_degree;
    std::queue<QCirGate*> ready_queue;
    std::unordered_set<size_t> seen_gates;

    std::stack<QCirGate*> discovery_stack;
    for (auto const& qubit : qcir.get_qubits()) {
        if (qubit.get_last_gate() != nullptr) {
            discovery_stack.push(qubit.get_last_gate());
        }
    }

    while (!discovery_stack.empty()) {
        auto* gate = discovery_stack.top();
        discovery_stack.pop();

        if (seen_gates.contains(gate->get_id())) {
            continue;
        }
        seen_gates.insert(gate->get_id());

        auto const& predecessors = qcir.get_predecessors(gate->get_id());
        std::set<size_t> unique_preds;
        for (auto const& pred : predecessors) {
            if (pred.has_value()) {
                unique_preds.insert(*pred);
            }
        }
        in_degree[gate->get_id()] = unique_preds.size();

        if (unique_preds.empty()) {
            ready_queue.push(gate);
        }

        for (auto const& pred_id : unique_preds) {
            if (!seen_gates.contains(pred_id)) {
                discovery_stack.push(qcir.get_gate(pred_id));
            }
        }
    }

    while (!ready_queue.empty()) {
        auto* gate = ready_queue.front();
        ready_queue.pop();
        topo_order.push_back(gate);

        auto const& successors = qcir.get_successors(gate->get_id());
        std::set<size_t> unique_succs;
        for (auto const& succ_id : successors) {
            if (succ_id.has_value()) {
                unique_succs.insert(*succ_id);
            }
        }

        for (auto const& succ_id : unique_succs) {
            if (in_degree.contains(succ_id)) {
                in_degree[succ_id]--;
                if (in_degree[succ_id] == 0) {
                    ready_queue.push(qcir.get_gate(succ_id));
                }
            }
        }
    }

    if (topo_order.size() != seen_gates.size()) {
        spdlog::error("Topological sort failed: processed {} gates but found {} gates total",
                      topo_order.size(), seen_gates.size());
    }

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

    _gate_list = have_if_else() ? topo_sort_with_classical(*this)
                               : topo_sort_wire_dfs(*this);
    assert(_gate_list.size() == get_num_gates());

    _dirty = false;
}

/**
 * @brief Reset QCir
 *
 */
void QCir::reset() {
    _qubits.clear();
    _classical_bits.clear();
    _gate_list.clear();
    _id_to_gates.clear();
    _predecessors.clear();
    _successors.clear();
    _measurement_producer_by_cbit.clear();
    _last_consumer_by_cbit.clear();
    _measurement_gate_order.clear();

    _gate_id = 0;
    _dirty   = true;
}

void QCir::adjoint_inplace() {
    for (auto& g : _id_to_gates | std::views::values) {
        g->set_operation(qsyn::qcir::adjoint(g->get_operation()));
        auto old_succs = get_successors(g->get_id());
        auto old_preds = get_predecessors(g->get_id());
        _set_successors(g->get_id(), old_preds);
        _set_predecessors(g->get_id(), old_succs);
    }

    for (auto& q : _qubits) {
        auto first = q.get_first_gate();
        auto last  = q.get_last_gate();
        q.set_first_gate(last);
        q.set_last_gate(first);
    }

    _dirty = true;
}

void QCir::concat(
    QCir const& other,
    std::map<QubitIdType /* new */, QubitIdType /* orig */> const& qubit_map) {
    for (auto const& new_gate : other.get_gates()) {
        auto const tmp =
            new_gate->get_qubits();  // circumvent g++ 11.4 compilation bug
        auto const qubits = tmp | std::views::transform([&qubit_map](auto const& qubit) {
                                return qubit_map.at(qubit);
                            }) |
                            tl::to<std::vector>();
        append(new_gate->get_operation(), qubits);
    }
}

}  // namespace qsyn::qcir
